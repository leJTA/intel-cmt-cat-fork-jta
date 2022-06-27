#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "catpc_utils.h"
#include "catpc_monitor.h"

#define MAX_CLIENTS 16			// Maximum number of slaves managed by CATPC master
#define SERVER_PORT 10000

struct connection_t {
	int sock;
	struct sockaddr_in address;
	socklen_t addr_len;
};

void* connection_handler(void*);
void termination_handler(int signum);

FILE* log_file = NULL;
int sock;
unsigned client_count = 0;
struct connection_t* connections[MAX_CLIENTS];
sig_atomic_t terminate = 0;


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
	sid = setsid();	// set new session
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

	pthread_t threads[MAX_CLIENTS];
	struct sockaddr_in address;
	unsigned i = 0;

	log_file = fopen("/var/log/catpc.log", "w");
	if (log_file == NULL) {
		exit(EXIT_FAILURE);
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(SERVER_PORT);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
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
		connections[i] = (struct connection_t *)malloc(sizeof(struct connection_t));
		connections[i]->sock = accept(sock, (struct sockaddr*)&connections[i]->address, &connections[i]->addr_len);
		if (connections[i]->sock < 0) {
			log_fprint(log_file, "ERROR: accept failed: %s (%d)\n", strerror(errno), errno);
			continue;
		}
		client_count++;
		
		log_fprint(log_file, "INFO: slave connected: %s\n", inet_ntoa(connections[i]->address.sin_addr));	

		// Launch connection handler on a thread
		if (pthread_create(&threads[i], NULL, connection_handler, (void*)connections[i]) != 0) {
			log_fprint(log_file, "ERROR: could not create thread for slave %s\n", inet_ntoa(connections[i]->address.sin_addr));
		}
		++i;
	}

	for (i = 0; i < client_count; ++i) {
		close(connections[i]->sock);
		pthread_kill(threads[i], SIGKILL);
	}

	fclose(log_file);
	return EXIT_SUCCESS;
}

void* connection_handler(void* ptr)
{
	struct connection_t* conn = (struct connection_t *)ptr;
	int bytes_read, bytes_sent;
	enum catpc_message msg;
	struct monitoring_values_tab* mvalues_tab ;

	mvalues_tab = (struct monitoring_values_tab*)malloc(sizeof(struct monitoring_values_tab));
	msg = CATPC_GET_MONITORING_VALUES;
	
	while ((bytes_sent = send(conn->sock, &msg, sizeof(msg), 0)) > 0) {
		// Read Length of data
		bytes_read = recv(conn->sock, &mvalues_tab->size, sizeof(mvalues_tab->size), 0);
		log_fprint(log_file, "INFO: receiving data of length %d\n", mvalues_tab->size);
		
		// Allocate memory
		mvalues_tab->values = (struct monitoring_values*)malloc(mvalues_tab->size * sizeof(struct monitoring_values));
		
		// Read values
		bytes_read = recv(conn->sock, mvalues_tab->values, mvalues_tab->size * sizeof(struct monitoring_values), 0);
		
		if (bytes_read < 0) {
			log_fprint(log_file, "ERROR: recv from %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
		}

		sleep(1);
	}

	if (bytes_sent == 0) {
		log_fprint(log_file, "INFO: recv: client closed. Exiting...\n");
	}
	else {	// bytes_sent < 0
		log_fprint(log_file, "ERROR: send to %s: %s (%d)\n", inet_ntoa(conn->address.sin_addr), strerror(errno), errno);
	}

	close(conn->sock);
	return NULL;
}

void termination_handler(int signum) 
{
   terminate = 1;
	close(sock);
}