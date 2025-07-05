/*this code is the interface server He uses TCP socket and is
 responsible for sent  the keywords that  
received from clients to the search servers 
He also uses parallelism for this purpose and 
concurrency to handle multiple clients */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include<signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#define PORT 2000
#define MAX_SIZE 4096
typedef struct {
	char *word;
	char **response;
	char *ip;
	int port;
	pthread_mutex_t *mutex;
} SearchParams;
void *searchServerThread(void *sst) {
	SearchParams *params = (SearchParams *)sst;
	int sock;
	struct sockaddr_in addr;
	char *buf = malloc(MAX_SIZE);
	int buf_size = MAX_SIZE;
	int total = 0;
	int bytes;
	if (!buf) {
		perror("malloc");
		pthread_exit(NULL);
	}
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("search socket");
		free(buf);
		pthread_exit(NULL);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(params->port);
	if (inet_pton(AF_INET, params->ip, &addr.sin_addr) <= 0) {
		perror("inet_pton");
		close(sock);
		free(buf);
		pthread_exit(NULL);
	}
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect to search server");
		close(sock);
		free(buf);
		pthread_exit(NULL);
	}
	char msg[MAX_SIZE];
	int len = snprintf(msg, sizeof(msg), "%s\n", params->word);
	if (write(sock, msg, len) < 0) {
		perror("write keyword");
		close(sock);
		free(buf);
		pthread_exit(NULL);
	}
	shutdown(sock, SHUT_WR); //shut down or close the writing side of the socket connection.
	while ((bytes = read(sock, buf + total, buf_size - total)) > 0) {
		total += bytes;
		if (total >= buf_size) {
			buf_size *= 2;
			char *new_buf = realloc(buf, buf_size);
			if (!new_buf) {
				perror("realloc failed");
				free(buf);
				close(sock);
				pthread_exit(NULL);
			}
			buf = new_buf;
		}
	}
	buf[total] = '\0';
	pthread_mutex_lock(params->mutex);
	if (total == 0) {
		// strdup Creates a duplicate of a given string using dynamic memory allocation.
		*params->response = strdup("Not found\n");
	} else {
		*params->response = buf;
	}
	pthread_mutex_unlock(params->mutex);

	printf("Received response from %s:%d\n", params->ip, params->port);
	close(sock);
	pthread_exit(NULL);
}
void handle_client(int cl_connect, struct sockaddr_in client_addr,char *ip1,int port1,char *ip2,int port2,char *ip3,int port3) {
	char buf[MAX_SIZE];
	int valread = read(cl_connect, buf, MAX_SIZE - 1);
	if (valread <= 0) {
		close(cl_connect);
		return;
	}
	buf[valread] = '\0';
	printf("Interface Server Received from client: %s\n", buf);
	char *client_ip = inet_ntoa(client_addr.sin_addr);
	printf("The Interface Server Client IP: %s\n", client_ip);
	pthread_t t1, t2, t3;
	char *search1 = NULL;
	char *search2 = NULL;
	char *search3 = NULL;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	SearchParams p1 = { buf, &search1, ip1, port1, &mutex };
	SearchParams p2 = { buf, &search2, ip2, port2, &mutex };
	SearchParams p3 = { buf, &search3, ip3, port3, &mutex };
	pthread_create(&t1, NULL, searchServerThread, &p1);
	pthread_create(&t2, NULL, searchServerThread, &p2);
	pthread_create(&t3, NULL, searchServerThread, &p3);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);

	int total_size = strlen(search1) + strlen(search2) + strlen(search3) + 200;
	char *response = malloc(total_size);
	if (!response) {
		perror("malloc");
		close(cl_connect);
		return;
	}
	response[0] = '\0';
	strncat(response, "Server1:\n", total_size - strlen(response) - 1);
	strncat(response, search1, total_size - strlen(response) - 1);
	strncat(response, "\nServer2:\n", total_size - strlen(response) - 1);
	strncat(response, search2, total_size - strlen(response) - 1);
	strncat(response, "\nServer3:\n", total_size - strlen(response) - 1);
	strncat(response, search3, total_size - strlen(response) - 1);
	strncat(response, "\n", total_size - strlen(response) - 1);


	printf("Interface Server Combined result:\n%s\n", response);
	write(cl_connect, response, strlen(response));
	free(search1);
	free(search2);
	free(search3);
	free(response);
	close(cl_connect);
}
void handler(int x) {

	while(wait(NULL)>0) {}
}
int main(int argc, char *argv[]) {
	if (argc != 7) {
		fprintf(stderr, "Usage: %s <search_server_ip1> <port1> <search_server_ip2> <port2> <search_server_ip3> <port3>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	signal(SIGCHLD,handler);
	char *ip1 = argv[1];
	int port1 = atoi(argv[2]);
	char *ip2 = argv[3];
	int port2 = atoi(argv[4]);
	char *ip3 = argv[5];
	int port3 = atoi(argv[6]);

	int server_fd;
	struct sockaddr_in servaddr, clinaddr;

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("server socket");
		exit(EXIT_FAILURE);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);
	if (bind(server_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 10) < 0) {
		perror("listen");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	printf("Interface Server Listening on port %d...\n", PORT);
	while (1) {
		socklen_t addrlen = sizeof(clinaddr);
		int cl_connect = accept(server_fd, (struct sockaddr *)&clinaddr, &addrlen);
		if (cl_connect < 0) {
			perror("accept");
			continue;
		}
		pid_t pid=fork();
		if(pid==0) {
			close(server_fd);
			handle_client(cl_connect, clinaddr, ip1, port1, ip2, port2, ip3, port3);
			exit(0);
		} else {
			close(cl_connect);
		}

	}
	close(server_fd);
	return 0;
}
