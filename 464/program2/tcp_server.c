/******************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "testing.h"
#include "networks.h"

#define MAXBUF 1024

server *tcp_server;

void report_error() {
	printf("Error!\n");
	exit(1);
}

handle *is_handle_in_list(uint8_t len, char *name) {
	handle *temp = tcp_server->head;
	char str[MAX_HANDLE_LEN + 1];

	while (temp) {
		if (temp->len == len) {
			memset(str, 0, MAX_HANDLE_LEN);
			memcpy(str, temp->name, len);
			str[len] = 0;

			if (!strcmp(str, name)) { // same name
				return temp;
			}
		}

		temp = temp->next;
	}

	return NULL;
}

void send_pkt(int socket, uint8_t *pkt) {
	int sent = 0;
	uint16_t pkt_len = ntohs(*((uint16_t *)pkt));

	sent = send(socket, pkt, pkt_len, 0);
	if(sent < 0) {
		perror("send call");
		exit(-1);
	}
}

void send_handle_confirmation(int socket) {
	uint8_t *pkt = malloc(HDR_LEN);
	uint16_t len = htons(HDR_LEN);
	uint8_t flag = 2;

	memcpy(pkt, &len, sizeof(uint16_t));
	memcpy(pkt + sizeof(uint16_t), &flag, sizeof(uint8_t));

	send_pkt(socket, pkt);
}

void send_error_on_initial_pkt(int client_socket) {
	uint8_t *pkt = malloc(HDR_LEN);
	uint16_t len = htons(HDR_LEN);
	uint8_t flag = 3;

	memcpy(pkt, &len, sizeof(uint16_t));
	memcpy(pkt + sizeof(uint16_t), &flag, sizeof(uint8_t));

	send_pkt(client_socket, pkt);
}

void send_exit_ack(int socket) {
	uint8_t *pkt = malloc(HDR_LEN);
	uint16_t len = htons(HDR_LEN);
	uint8_t flag = 9;

	memcpy(pkt, &len, sizeof(uint16_t));
	memcpy(pkt + sizeof(uint16_t), &flag, sizeof(uint8_t));

	send_pkt(socket, pkt);
}

void send_handle_doesnt_exist(uint8_t len, uint8_t *name, int socket) {
	uint16_t pkt_len = HDR_LEN + 1 + len;
	uint8_t *pkt = malloc(sizeof(uint8_t) * pkt_len);
	uint8_t flag = 7;

	pkt_len = htons(pkt_len);

	memcpy(pkt, &pkt_len, sizeof(uint16_t));
	memcpy(pkt + sizeof(uint16_t), &flag, sizeof(uint8_t));
	memcpy(pkt + HDR_LEN, &len, sizeof(uint8_t));
	memcpy(pkt + HDR_LEN + sizeof(uint8_t), name, len);

	send_pkt(socket, pkt);
}

void add_handle(uint8_t *pkt, int client_socket) {
	uint8_t len;
	char *name;
	handle *h, *temp = tcp_server->head;

	memcpy(&len, pkt + HDR_LEN, sizeof(uint8_t));
	name = malloc(sizeof(char) * len);
	memcpy(name, pkt + HDR_LEN + 1, len);

	if (is_handle_in_list(len, name)) {
		send_error_on_initial_pkt(client_socket);
	}
	else {
		h = malloc(sizeof(handle));
		h->len = len;
		h->name = name;
		h->socket_num = client_socket;
		if (h->socket_num >= tcp_server->max_socket_num) {
			tcp_server->max_socket_num = h->socket_num + 1;
		}

		if (temp) {
			while (temp->next) {
				temp = temp->next;
			}

			temp->next = h;
		}
		else {
			tcp_server->head = h;
		}

		tcp_server->num_clients++;

		send_handle_confirmation(h->socket_num);
	}
}

void remove_handle(int socket) {
	handle *h = tcp_server->head, *prev;

	if (h && h->socket_num == socket) {
		tcp_server->head = h->next;
		free(h->name);
		free(h);
		tcp_server->num_clients--;
	}
	else {
		while (h) {
			prev = h;
			h = h->next;
			if (h && h->socket_num == socket) {
				prev->next = h->next;
				free(h->name);
				free(h);
				tcp_server->num_clients--;
			}
		}
	}

	send_exit_ack(socket);
	close(socket);
}

void forward_broadcast(uint8_t *pkt, int socket) {
	handle *h = tcp_server->head;

	while (h) {
		if (h->socket_num != socket) {
			send_pkt(h->socket_num, pkt);
		}

		h = h->next;
	}
}

void forward_message(uint8_t *pkt, int socket) {
	uint8_t *temp = pkt;
	uint8_t handle_len, num_handles;
	int i;
	handle *check;
	char str[MAX_HANDLE_LEN + 1];

	temp += HDR_LEN;
	handle_len = *temp;
	temp += handle_len + 1;
	num_handles = *temp;
	temp++;

	for (i=0; i<num_handles; i++) {
		memcpy(str, temp + 1, *temp);
		str[*temp] = 0;
		check = is_handle_in_list(*temp, str);

		if (!check) {
			send_handle_doesnt_exist(*temp, temp + 1, socket);
		}
		else {
			send_pkt(check->socket_num, pkt);
		}

		temp += 1 + *temp;
	}
}

uint8_t *build_handle_info_pkt(handle *h) {
	uint8_t *pkt, *temp;
	uint16_t packet_len;
	uint8_t flag = 12;

	packet_len = HDR_LEN + 1 + h->len;

	pkt = malloc(sizeof(uint8_t) * packet_len);
	temp = pkt;
	packet_len = htons(packet_len);
	memcpy(temp, &packet_len, sizeof(uint16_t));
	temp += sizeof(uint16_t);
	memcpy(temp, &flag, sizeof(uint8_t));
	temp++;
	memcpy(temp, &(h->len), sizeof(uint8_t));
	temp++;
	memcpy(temp, h->name, h->len);

	return pkt;
}

void send_handle_list(int socket) {
	handle *h = tcp_server->head;

	while (h) {
		send_pkt(socket, build_handle_info_pkt(h));
		h = h->next;
	}
}

uint8_t *build_num_handles_pkt() {
	uint8_t *pkt, *temp;
	uint16_t packet_len;
	uint8_t flag = 11;
	uint32_t num_clients;

	packet_len = HDR_LEN + sizeof(uint32_t);

	pkt = malloc(sizeof(uint8_t) * packet_len);
	temp = pkt;
	packet_len = htons(packet_len);
	memcpy(temp, &packet_len, sizeof(uint16_t));
	temp += sizeof(uint16_t);
	memcpy(temp, &flag, sizeof(uint8_t));
	temp++;
	num_clients = htonl(tcp_server->num_clients);
	memcpy(temp, &num_clients, sizeof(uint32_t));

	return pkt;
}

void send_num_handles(int socket) {
	send_pkt(socket, build_num_handles_pkt());
}

void perform_cmd(uint8_t flag, uint8_t *pkt, int socket) {
	switch (flag) {
			case 1:
				add_handle(pkt, socket);
				break;
			case 4:
				forward_broadcast(pkt, socket);
				break;
			case 5:
				forward_message(pkt, socket);
				break;
			case 8:
				remove_handle(socket);
				break;
			case 10:
				send_num_handles(socket);
				send_handle_list(socket);
				break;
		}
}

void parse_pkt(int socket) {
	uint16_t pkt_len;
	uint8_t *pkt, flag, hdr[HDR_LEN];
	int message_len = 0;

	message_len = recv(socket, hdr, HDR_LEN, MSG_PEEK);
	if (message_len < 0) {
		perror("recv call");
		exit(-1);
	}
	else if (!message_len) { // client exited, remove them from client list
		remove_handle(socket);
	}
	else {
		message_len = recv(socket, hdr, HDR_LEN, 0);
		if (message_len < 0) {
			perror("recv call");
			exit(-1);
		}

		memcpy(&pkt_len, hdr, sizeof(uint16_t));
		pkt_len = ntohs(pkt_len);

		memcpy(&flag, hdr + sizeof(uint16_t), sizeof(uint8_t));

		pkt = malloc(sizeof(uint8_t) * pkt_len);
		memcpy(pkt, hdr, HDR_LEN);

		if (pkt_len > 3) {
			message_len = recv(socket, pkt + HDR_LEN, pkt_len - HDR_LEN, 0);
			if (message_len < 0) {
				perror("recv call");
				exit(-1);
			}
		}

		perform_cmd(flag, pkt, socket);
	}
}

int setup_server(int argc, char *argv[]) {
	int server_socket;
	uint16_t port;
	char *endptr;

	if (argc > 1) {
		port = strtol(argv[1], &endptr, 10);

		if (endptr == argv[1] || !port) { // no digits found
			report_error();
		}
		server_socket = tcpServerSetup(port);
	}
	else {
		server_socket = tcpServerSetup(0);
	}

	return server_socket;
}

void reset_fds(fd_set *read_fds) {
	handle *h = tcp_server->head;

	FD_ZERO(read_fds);
	FD_SET(tcp_server->socket_num, read_fds);

	while (h) {
		FD_SET(h->socket_num, read_fds);

		h = h->next;
	}
}

void start_server() {
	int num_clients, temp_socket;
	fd_set read_fds;
	handle *temp;

	reset_fds(&read_fds);

	while((num_clients = select(tcp_server->max_socket_num + 1, &read_fds, NULL, NULL, NULL))) {

		if (num_clients < 0) {
			report_error();
		}

		if (FD_ISSET(tcp_server->socket_num, &read_fds)) { // new client needs to be accepted
			temp_socket = tcpAccept(tcp_server->socket_num);\

			parse_pkt(temp_socket);
		}

		temp = tcp_server->head;
		while (temp) {
			if (FD_ISSET(temp->socket_num, &read_fds)) {
				parse_pkt(temp->socket_num);
			}

			temp = temp->next;
		}

		reset_fds(&read_fds);
	}
}

int main(int argc, char *argv[])
{
	server *s = malloc(sizeof(server));

	s->socket_num = setup_server(argc, argv);
	s->max_socket_num = s->socket_num + 1;
	s->num_clients = 0;

	tcp_server = s;

	start_server();
	
	return 0;
}
