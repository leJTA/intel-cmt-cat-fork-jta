#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <vector>

#include "catpc_utils.hpp"
#include "catpc_monitor.hpp"

#define SERVER_PORT 10000

FILE* log_file = NULL;
std::unordered_map<std::string, application*> applications;

int main(int argc, char** argv)
{
	pid_t pid = fork();
	char* master_name = NULL;

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

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	struct hostent* host_info = NULL;
	struct sockaddr_in server_addr;
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int bytes_read = 0, bytes_sent = 0;
	enum catpc_message msg;
	std::string cmdline;
	size_t sz;
	int ret = 0;
	char buf[512];

	log_file = fopen("/var/log/catpc.slave.log", "w");
	if (log_file == NULL) {
		exit(EXIT_FAILURE);
	}

	// Start Monitoring
	ret = init_monitoring();
	if (ret < 0) {
		log_fprint(log_file, "ERROR: unable to init monitoring\n");
		exit(EXIT_FAILURE);
	}

	// get master info by name
	host_info = gethostbyname(master_name);
	if (host_info == NULL) {
		log_fprint(log_file, "ERROR: unknown host: %s\n", master_name);
		exit(EXIT_FAILURE);
	}

	server_addr.sin_family = AF_INET;
	memcpy((char*)&server_addr.sin_addr, host_info->h_addr_list[0], host_info->h_length);
	server_addr.sin_port = htons(SERVER_PORT);
	
	if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		log_fprint(log_file, "ERROR: connection to %s failed: %s (%d)\n", inet_ntoa(server_addr.sin_addr), strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	// read message from master
	while ((bytes_read = recv(sock, &msg, sizeof(msg), 0)) > 0) {
		switch (msg) {
		case CATPC_GET_MONITORING_VALUES:
			log_fprint(log_file, "INFO: message received: CATPC_GET_MONITORING_VALUES\n");
			
			// poll monitoring values
			ret = poll_monitoring_data(applications);
			if (ret < 0) {
				log_fprint(log_file, "ERROR: polling monitoring data (%d)\n", ret);
				goto exit;
			}

			// send values tab
			sz = applications.size();
			bytes_sent = send(sock, &sz, sizeof(size_t), 0);													// send the number of applications
			for (std::pair<std::string, application*> element : applications) {
				sz = element.first.size();																				
				bytes_sent = send(sock, &sz, sizeof(size_t), 0);												// send the length of the cmdline string
				bytes_sent = send(sock, element.first.c_str(), sz * sizeof(char), 0);					// send the cmdline string
				bytes_sent = send(sock, &element.second->values, sizeof(monitoring_values), 0);		// send monitoring values
				bytes_sent = send(sock, &element.second->CLOS_id, sizeof(ushort), 0);					// send CLOS id
			}
								
			break;
		case CATPC_ADD_APP_TO_MONITOR:
			log_fprint(log_file, "INFO: message received: CATPC_ADD_APP_TO_MONITOR\n");
			
			// receive cmd line string
			bytes_read = recv(sock, &sz, sizeof(size_t), 0);
			bytes_read = recv(sock, buf, sz * sizeof(char), 0);
			cmdline.assign(buf);

			// add application to the map
			applications[cmdline] = new application{cmdline, monitoring_values(), 0};

			// start monitoring app
			ret = start_monitoring(cmdline);
			if (ret < 0) {
				log_fprint(log_file, "ERROR: unable to start monitoring for app \"%s\"\n", cmdline.c_str());
				exit(EXIT_FAILURE);
			}
			
			log_fprint(log_file, "Added : %s\n", cmdline.c_str());
			break;

		case CATPC_GET_ALLOCATION_CONF:
			// TODO SEND ALLOCATION CONFIGURATION
			break;
		case CATPC_PERFORM_ALLOCATION:
			// TODO PERFORM ALLOCATION
			break;
		default:
			log_fprint(log_file, "ERROR: unknow message value: %d\n", msg);
		}

		// error checking
		if (bytes_sent < 0) {
			log_fprint(log_file, "ERROR: send: %s (%d)\n", strerror(errno), errno);
		}
		if (bytes_read < 0) {
			log_fprint(log_file, "ERROR: recv: %s (%d)\n", strerror(errno), errno);
		}
	}

	if (bytes_read == 0) {
		log_fprint(log_file, "INFO: recv: server closed. Terminating...\n");
	}
	else {	// bytes_read < 0
		log_fprint(log_file, "ERROR: recv: %s (%d)\n", strerror(errno), errno);
	}

exit:
	// stop monitoring before exit
	stop_monitoring(applications);
	
	// cleaning up everything
	fclose(log_file);
	close(sock);

	return EXIT_SUCCESS;
}