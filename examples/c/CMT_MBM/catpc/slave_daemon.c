#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#include "utils.h"

#define SERVER_PORT 10000

FILE* log_file = NULL;

int main(int argc, char** argv)
{
	pid_t pid = fork();
	int i = 0;
	char* master_name;

	if (argc < 2) {
		printf("Usage : %s [master-name]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	master_name = argv[1];

	if (pid < 0) {
		fprintf(stderr, "fork failed!\n");
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		fprintf(stderr, "daemon created : %d \n", pid);
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

	struct hostent* host_info;
	struct sockaddr_in server;
	int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct monitoring_data mdata = {.llc=103216544, .ipc=2.7, .llc_misses=31165446, .llc_references=41165446};
	int bytes_read, bytes_sent;

	log_file = fopen("/var/log/catpc.slave.log", "w");
	if (log_file == NULL) {
		exit(EXIT_FAILURE);
	}

	host_info = gethostbyname(master_name);
	if (host_info == NULL) {
		log_fprint(log_file, "ERROR: unknown host : %s\n", master_name);
		exit(EXIT_FAILURE);
	}

	server.sin_family = AF_INET;
	memcpy((char*)&server.sin_addr, host_info->h_addr_list[0], host_info->h_length);
	server.sin_port = htons(SERVER_PORT);
	
	if (connect(sock_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
		log_fprint(log_file, "ERROR: connection to server failed\n");
		exit(EXIT_FAILURE);
	}

	log_fprint(log_file, "INFO: start sending data\n");
	for (i = 0; i < 10; ++i) {
		bytes_sent = send(sock_fd, &mdata, sizeof(mdata), 0);
		if (bytes_sent < 0) {
			log_fprint(log_file, "ERROR: send: %s (%d)\n", strerror(errno), errno);
		}
		
		bytes_read = recv(sock_fd, &mdata, sizeof(mdata), 0);
		if (bytes_read < 0) {
			log_fprint(log_file, "ERROR: recv: %s (%d)\n", strerror(errno), errno);
		}
		
		log_fprint(log_file, "INFO: received data {llc = %ld, ipc = %1.3f, misses = %ld, references = %ld}\n", mdata.llc, mdata.ipc, mdata.llc_misses, mdata.llc_references);
	}

	return EXIT_SUCCESS;
}