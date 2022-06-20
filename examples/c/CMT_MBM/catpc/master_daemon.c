#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "utils.h"

#define MAX_CLIENTS 16			// Maximum number of slaves managed by CATPC
#define SERVER_PORT 10000

void* connection_handler(void*);

FILE* log_file = NULL;
int clt_count = 0;


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

	// Start of the daemon Code
	umask(0);
	
	pid_t sid = 0;
	sid = setsid();	// set new session
	if(sid < 0) {
		exit(EXIT_FAILURE);
	}
	
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	pthread_t thread_id[MAX_CLIENTS];
	int i = 0;
	int serv_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), clt_fds[MAX_CLIENTS];
	struct sockaddr_in server, clients[MAX_CLIENTS];
	socklen_t clt_len;

	log_file = fopen("/var/log/catpc.log", "w");
	if (log_file == NULL) {
		exit(EXIT_FAILURE);
	}
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(SERVER_PORT);

	// Bind
	if (bind(serv_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
		log_fprint(log_file, "ERROR: bind failed\n");
		exit(EXIT_FAILURE);
	}

	// Listen
	if (listen(serv_fd, MAX_CLIENTS)) {
		log_fprint(log_file, "ERROR: listen failed\n");
	}
	
	// Loop to accept incomming connections
	clt_len = sizeof(struct sockaddr_in);
	while (1) {
		clt_fds[i] = accept(serv_fd, (struct sockaddr*)&clients[i], &clt_len);
		if (clt_fds[i] < 0) {
			log_fprint(log_file, "ERROR: accept failed\n");
		}
		clt_count++;
		
		log_fprint(log_file, "INFO: slave connected %s\n", inet_ntoa(clients[i].sin_addr));	

		// Launch connection handler on a thread
		if (pthread_create(&thread_id[i], NULL, connection_handler, (void*)&clt_fds[i]) != 0) {
			log_fprint(log_file, "ERROR: could not create thread for slave %s\n", inet_ntoa(clients[i].sin_addr));
		}
		++i;
	}

	fclose(log_file);
	return EXIT_SUCCESS;
}

void* connection_handler(void* sock_fd)
{
	int fd = *(int*)sock_fd;
	int bytes_read;
	struct monitoring_data mdata;
	while ((bytes_read = recv(fd, &mdata, sizeof(mdata), 0)) > 0) {
		log_fprint(log_file, "INFO: received data {llc = %ld, ipc = %f, misses = %ld, references = %ld}\n", mdata.llc, mdata.ipc, mdata.llc_misses, mdata.llc_references);
		send(fd,  &mdata, sizeof(mdata), 0);
	}

	if (bytes_read == 0) {
		log_fprint(log_file, "INFO: slave disconnected\n");
	}
	else if (bytes_read < 0) {
		log_fprint(log_file, "ERROR: recv: %s (%d)\n", strerror(errno), errno);
	}

	close(fd);
	return NULL;
}