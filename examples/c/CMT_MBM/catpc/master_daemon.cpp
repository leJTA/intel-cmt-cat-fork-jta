#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <errno.h>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "catpc_utils.hpp"
#include "catpc_monitor.hpp"

#define MAX_CLIENTS 16			// Maximum number of slaves managed by CATPC master
#define SERVER_PORT 10000
#define PERIOD 1000000					// Period in milliseconds

struct connection_t {
	int sock;
	struct sockaddr_in address;
	socklen_t addr_len;
};

void connection_handler(connection_t*);
void termination_handler(int signum);

FILE* log_file = NULL;
int sock;
unsigned client_count = 0;
std::vector<connection_t*> connections;
sig_atomic_t terminate = 0;

std::unordered_map<std::string, application*> applications;


int main(int argc, char** argv)
{
	pid_t pid = fork();

	if (pid < 0) {
		fprintf(stderr, "fork failed!\n");
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		printf("daemon created : %d \n", pid);
		exit(EXIT_SUCCESS);
	}

	/*
	* =======================================
	* Daemon Code
	* =======================================
	*/
	umask(0);
	
	pid_t sid = 0;
	sid = setsid();			// set new session
	if(sid < 0) {
		exit(EXIT_FAILURE);
	}
	
	// close all standard file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	// handle termination signals
	signal(SIGINT, termination_handler);
	signal(SIGTERM, termination_handler);

	std::vector<std::thread> threads;
	sockaddr_in address;
	const int reuse = 1;		// reuse socket

	log_file = fopen("/var/log/catpc.master.log", "w");
	if (log_file == NULL) {
		exit(EXIT_FAILURE);
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(SERVER_PORT);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		log_fprint(log_file, "ERROR: setsockopt (SO_REUSEADDR) failed: %s (%d)\n", strerror(errno), errno);
	
	// Bind
	if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
		log_fprint(log_file, "ERROR: bind failed: %s (%d)\n", strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	// Listen
	if (listen(sock, MAX_CLIENTS)) {
		log_fprint(log_file, "ERROR: listen failed: %s (%d)\n", strerror(errno), errno);
	}
	
	// Loop to accept incomming connections
	while (!terminate) {
		connections.push_back(new connection_t());
		connections.back()->sock = accept(sock, (struct sockaddr*)&connections.back()->address, &connections.back()->addr_len);
		if (connections.back()->sock < 0) {
			log_fprint(log_file, "ERROR: accept failed: %s (%d)\n", strerror(errno), errno);
			continue;
		}
		client_count++;
		
		log_fprint(log_file, "INFO: slave connected: %s\n", inet_ntoa(connections.back()->address.sin_addr));

		// Launch connection handler on a thread
		threads.push_back(std::thread(connection_handler, connections.back()));
		if (!threads.back().joinable()) {
			log_fprint(log_file, "ERROR: could not create thread for slave %s\n", inet_ntoa(connections.back()->address.sin_addr));
		}
	}

	for (connection_t* conn : connections) {
		close(conn->sock);
		delete conn;
	}

	for (std::thread& thd : threads) {
		if (thd.joinable()) {
			thd.join();
		}
	}

	log_fprint(log_file, "INFO: Terminating...\n");
	fclose(log_file);
	return EXIT_SUCCESS;
}

void connection_handler(connection_t* conn)
{
	int bytes_read, bytes_sent;
	enum catpc_message msg;
	application app;
	size_t sz, len;
	char buf[512];

	msg = CATPC_ADD_APP_TO_MONITOR;
	if ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
		std::string cl{ "../run_base_ref_gnu_mpi.0009/hpgmgfv_base.gnu_mpi59300" };
		len = cl.size();
		send(conn->sock, &len, sizeof(size_t), 0);
		send(conn->sock, cl.c_str(), len * sizeof(char), 0);
	}

	msg = CATPC_GET_MONITORING_VALUES;
	
	while ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
		// Read the number of applications
		bytes_read = recv(conn->sock, &sz, sizeof(size_t), 0);

		for (size_t i = 0; i < sz && bytes_read > 0; ++i) {
			// Read the cmdline (size then chars)
			bytes_read = recv(conn->sock, &len, sizeof(size_t), 0);
			bytes_read = recv(conn->sock, buf, len * sizeof(char), 0);
			app.cmdline.assign(buf);

			// Read monitoring data
			bytes_read = recv(conn->sock, &app.values, sizeof(monitoring_values), 0);

			// Read CLOS id
			bytes_read = recv(conn->sock, &app.CLOS_id, sizeof(ushort), 0);

			// store data
			applications[app.cmdline] = new application{app};

			log_fprint(log_file, "%s : MR[%.1fkB] = %1.4f\n", app.cmdline.c_str(), applications[app.cmdline]->values.llc / 1024.0, 
				(double)applications[app.cmdline]->values.llc_misses / applications[app.cmdline]->values.llc_references);
		}
		usleep(PERIOD);
		
		if (bytes_read < 0) {
			log_fprint(log_file, "ERROR: recv from %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
		}
	}

	if (bytes_sent == 0) {
		log_fprint(log_file, "INFO: recv: slave disconnected %s\n", inet_ntoa(conn->address.sin_addr));
	}
	else {	// bytes_sent < 0
		log_fprint(log_file, "ERROR: send to %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
	}

	close(conn->sock);
}

void termination_handler(int signum) 
{
	if (signum == SIGTERM) {
		terminate = 1;
		close(sock);
	}
}