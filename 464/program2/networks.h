
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#define BACKLOG 5
#define MAX_PKT_LEN 3515
#define MAX_HANDLE_LEN 250
#define MAX_LINE_LEN 4000
#define HDR_LEN 3
#define MAX_MESSAGE_LEN 1000

typedef struct handle {
	int len;
	char *name;
	int socket_num;
	struct handle *next;
} handle;

typedef struct server {
	int socket_num;
	int max_socket_num;
	handle *head;
	uint32_t num_clients;
} server;

// for the server side
int tcpServerSetup(uint16_t port);
int tcpAccept(int server_socket);

// for the client side
int tcpClientSetup(char *host_name, char *port);


#endif
