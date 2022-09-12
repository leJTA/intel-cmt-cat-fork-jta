#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <errno.h>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>

#include "catpc_utils.hpp"
#include "catpc_monitor.hpp"
#include "catpc_allocator.hpp"

#define MAX_CLIENTS 16			// Maximum number of slaves managed by CATPC master
#define SERVER_PORT 10000
#define PERIOD 1000000					// Period in milliseconds

struct connection_t {
	int sock;
	struct sockaddr_in address;
	socklen_t addr_len;
};

struct notification_t {
	std::mutex mtx;

	bool app_added;				// new app flag
	bool app_removed;				// removed app flag
	std::string cmdline;			// command line of the app

	notification_t() : app_added{false}, app_removed{false}, cmdline{""}
	{}

	notification_t(const notification_t&& other)
	{
		this->app_added = other.app_added;
		this->app_removed = other.app_removed;
		this->cmdline = other.cmdline;
	}
};

void connection_handler(connection_t*);
void termination_handler(int signum);
void fifo_monitoring();

FILE* log_file = NULL;
int sock;
unsigned client_count = 0;
std::vector<connection_t*> connections;
sig_atomic_t terminate = 0;

std::unordered_map<int, std::unordered_map<std::string, catpc_application*>> sock_to_application;
std::unordered_map<int, notification_t> sock_to_notification;
std::unordered_map<int, std::vector<llc_ca>> sock_to_llcs;

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
	
	// start fifo monitoring thread
	std::thread fifo_thread(fifo_monitoring);

	// Loop to accept incomming connections
	while (!terminate) {
		connections.push_back(new connection_t());
		connections.back()->sock = accept(sock, (struct sockaddr*)&connections.back()->address, &connections.back()->addr_len);
		if (connections.back()->sock < 0) {
			log_fprint(log_file, "ERROR: accept failed: %s (%d)\n", strerror(errno), errno);
			continue;
		}
		client_count++;

		// insert corresponding and mutex
		sock_to_notification.insert(std::make_pair(connections.back()->sock, notification_t()));
		
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

	if (fifo_thread.joinable()) {
		fifo_thread.join();
	}
	for (std::thread& thd : threads) {
		if (thd.joinable()) {
			thd.join();
		}
	}

	log_fprint(log_file, "INFO: Done.\n");
	fclose(log_file);
	return EXIT_SUCCESS;
}

void connection_handler(connection_t* conn)
{
	notification_t& notif = sock_to_notification[conn->sock];
	int bytes_read = 0, bytes_sent = 0;
	enum catpc_message msg = CATPC_GET_MONITORING_VALUES;
	std::string cmdline;
	catpc_application app;
	size_t sz, len;
	char buf[512];

	int step = 0;

	while (true) {
		{
			// Read the notification object to know if there is a new app or if an app has been removed/terminated
			std::lock_guard<std::mutex> lk(notif.mtx);
			if (notif.app_added) {
				notif.app_added = false;	// reset the flag
				cmdline = notif.cmdline;
				msg = CATPC_ADD_APP_TO_MONITOR;
			}  
			else if (step == 0) {
				msg = CATPC_GET_ALLOCATION_CONF;
			}
			else if (step % 10 == 0) {
				msg = CATPC_PERFORM_ALLOCATION;
				for (auto app : sock_to_application[conn->sock]) {
					app.second->CLOS_id = 2;
				}
			}
			step++;
		}

		switch (msg)
		{
		case CATPC_REMOVE_APP_TO_MONITOR:
		case CATPC_ADD_APP_TO_MONITOR : 
			if ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
				len = cmdline.size();
				send(conn->sock, &len, sizeof(size_t), 0);
				send(conn->sock, cmdline.c_str(), len * sizeof(char), 0);
			}
			break;
		
		case CATPC_GET_MONITORING_VALUES:
			if ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
				// Read the number of applications
				bytes_read = recv(conn->sock, &sz, sizeof(size_t), 0);

				for (size_t i = 0; i < sz && bytes_read > 0; ++i) {
					// Read the cmdline (size then chars)
					bytes_read = recv(conn->sock, &len, sizeof(size_t), 0);
					bytes_read = recv(conn->sock, buf, len * sizeof(char), 0);
					app.cmdline.assign(buf);

					// Read monitoring data
					bytes_read = recv(conn->sock, &app.values, sizeof(catpc_monitoring_values), 0);

					// Read CLOS id
					bytes_read = recv(conn->sock, &app.CLOS_id, sizeof(unsigned int), 0);

					// store data
					sock_to_application[conn->sock][app.cmdline] = new catpc_application{app};

					log_fprint(log_file, "%s : MR[%.1fkB] = %1.4f\n", app.cmdline.c_str(), 
						sock_to_application[conn->sock][app.cmdline]->values.llc / 1024.0, 
						(double)sock_to_application[conn->sock][app.cmdline]->values.llc_misses / sock_to_application[conn->sock][app.cmdline]->values.llc_references);
				}
			}
			break;

		case CATPC_GET_ALLOCATION_CONF:
		if ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
			sock_to_llcs[conn->sock].clear();								// Clear the llc_ca list
			bytes_read = recv(conn->sock, &sz, sizeof(size_t), 0);	// read the number of llc/sockets
			for (size_t i = 0; i < sz; ++i) {
				sock_to_llcs[conn->sock].push_back(llc_ca());
				llc_ca& llc = sock_to_llcs[conn->sock].back();			// reference to the new element for better readability
				
				bytes_read = recv(conn->sock, &llc.id, sizeof(int), 0);
				bytes_read = recv(conn->sock, &llc.clos_count, sizeof(unsigned), 0);

				for (unsigned j = 0; j < llc.clos_count; ++j) {
					llc.clos_list.push_back(CLOS());
					bytes_read = recv(conn->sock, &llc.clos_list.back(), sizeof(CLOS), 0);
				}
			}
		}
		break;

		case CATPC_PERFORM_ALLOCATION:
			if ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
				for (std::pair<std::string, catpc_application*> element : sock_to_application[conn->sock]) {
					len = cmdline.size();
					send(conn->sock, &len, sizeof(size_t), 0);
					send(conn->sock, element.first.c_str(), len * sizeof(char), 0);
					send(conn->sock, &element.second->CLOS_id, sizeof(unsigned int), 0);
				}
			}
		break;

		default:
				log_fprint(log_file, "unknow message value: %d\n", msg);

		}
		
		// Error handling
		if (bytes_read < 0) {
			log_fprint(log_file, "ERROR: recv from %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
			break;
		}

		if (bytes_sent == 0) {
			log_fprint(log_file, "INFO: recv: slave disconnected %s\n", inet_ntoa(conn->address.sin_addr));
			break;
		}
		else if (bytes_sent < 0) {	// bytes_sent < 0
			log_fprint(log_file, "ERROR: send to %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
			break;
		}

		msg = CATPC_GET_MONITORING_VALUES;	// set to the default message value
		usleep(PERIOD);
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

void fifo_monitoring()
{
	log_fprint(log_file, "INFO: starting fifo monitoring\n");
	int fd;
	const char* catpc_fifo = "/tmp/catpc.fifo";
	char buf[512];

	mkfifo(catpc_fifo, 0666);

	while (!terminate) {
		fd = open(catpc_fifo, O_RDONLY);
		if (read(fd, buf, sizeof(buf)) < 0) {
			log_fprint(log_file, "ERROR: read failed: %s(%d)\n", strerror(errno), errno);
			return;
		}
		log_fprint(log_file, "INFO: app launched: \"%s\"\n", buf);

		for (std::pair<const int, notification_t>& it : sock_to_notification) {
			{
				std::lock_guard<std::mutex> lk(it.second.mtx);
				it.second.app_added = true;
				it.second.cmdline.assign(buf);
			}
		}
		close(fd);
	}
}