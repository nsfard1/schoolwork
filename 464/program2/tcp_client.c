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

void read_input(fd_set *read_fds);
void parse_pkt(int socket, fd_set *read_fds);
void reset_fds(fd_set *read_fds, int set_stdin);

handle *client;

void report_error(const char *msg) {
	printf("Error: %s\n", msg);
	exit(1);
}

void send_pkt(uint8_t *pkt) {
	int sent = 0;
	uint16_t pkt_len = ntohs(*((uint16_t *)pkt));

	sent = send(client->socket_num, pkt, pkt_len, 0);
	if(sent < 0) {
		perror("send call");
		exit(-1);
	}
}

void exit_ok() {
	// free any allocated space
	free(client->name);
	free(client);

	exit(0);
}

void good_handle(fd_set *read_fds) {
	read_input(read_fds);
}

void receive_message(uint8_t *pkt) {
	uint8_t src_handle_len, dst_handle_len;
	char *src_handle;
	uint8_t num_handles;
	char *message;
	int i;

	pkt += HDR_LEN;

	memcpy(&src_handle_len, pkt, 1);
	pkt++;

	src_handle = malloc(sizeof(char) * (src_handle_len + 1));
	memcpy(src_handle, pkt, sizeof(char) * src_handle_len);
	src_handle[src_handle_len] = 0;
	pkt += src_handle_len;

	memcpy(&num_handles, pkt, sizeof(uint8_t));
	pkt++;

	for (i=0; i<num_handles; i++) {
		memcpy(&dst_handle_len, pkt, sizeof(uint8_t));
		pkt += dst_handle_len + 1;
	}

	message = (char *) pkt;

	printf("\n%s: %s", src_handle, message);
}

void receive_broadcast(uint8_t *pkt) {
	uint8_t src_handle_len;
	char *src_handle;
	char *message;

	pkt += HDR_LEN;

	memcpy(&src_handle_len, pkt, sizeof(uint8_t));
	pkt++;

	src_handle = malloc(sizeof(char) * (src_handle_len + 1));
	memcpy(src_handle, pkt, sizeof(char) * src_handle_len);
	src_handle[src_handle_len] = 0;
	pkt += src_handle_len;

	message = (char *) pkt;

	printf("\n%s: %s", src_handle, message);
}

void receive_list_item(uint8_t *pkt) {
	uint8_t dst_handle_len;
	char *dst_handle;

	pkt += HDR_LEN;

	memcpy(&dst_handle_len, pkt, sizeof(uint8_t));
	pkt++;

	dst_handle = malloc(sizeof(char) * (dst_handle_len + 1));
	memcpy(dst_handle, pkt, sizeof(char) * dst_handle_len);
	dst_handle[dst_handle_len] = 0;

	printf("\t%s\n", dst_handle);
}

uint32_t receive_list_num(uint8_t *pkt) {
	uint32_t num_clients;

	pkt += HDR_LEN;

	memcpy(&num_clients, pkt, sizeof(uint32_t));
	num_clients = ntohl(num_clients);

	printf("\nNumber of clients: %d\n", num_clients);

	return num_clients;
}

void receive_error_init_pkt(uint8_t *pkt) {
	client->name = realloc(client->name, client->len + 1);
	client->name[client->len] = 0;

	printf("Handle already in use: %s\n", client->name);
	free(client->name);
	free(client);
}

void receive_invalid_dst_handle(uint8_t *pkt) {
	uint8_t dst_handle_len;
	char *dst_handle;

	pkt += HDR_LEN;

	memcpy(&dst_handle_len, pkt, sizeof(uint8_t));
	pkt++;

	dst_handle = malloc(sizeof(char) * (dst_handle_len + 1));
	memcpy(dst_handle, pkt, sizeof(char) * dst_handle_len);
	dst_handle[dst_handle_len] = 0;

	printf("\nClient with handle %s does not exist\n", dst_handle);
}

void wait_for_list_items(int num_items, fd_set *read_fds) {
	int i;

	for (i=0; i<num_items; i++) {
		reset_fds(read_fds, 0);
		if (select(client->socket_num + 1, read_fds, NULL, NULL, NULL) < 0) {
			report_error("select error while waiting for list items");
		}

		if (FD_ISSET(client->socket_num, read_fds)) {
			parse_pkt(client->socket_num, read_fds);
		}
	}
}

void perform_cmd(uint8_t flag, uint8_t *pkt, fd_set *read_fds) {
	uint32_t list_num;

	switch (flag) {
			case 2:
				good_handle(read_fds);
				break;
			case 3:
				receive_error_init_pkt(pkt);
				break;
			case 4:
				receive_broadcast(pkt);
				break;
			case 5:
				receive_message(pkt);
				break;
			case 7:
				receive_invalid_dst_handle(pkt);
				break;
			case 9:
				exit_ok();
				break;
			case 11:
				list_num = receive_list_num(pkt);
				wait_for_list_items(list_num, read_fds);
				break;
			case 12:
				receive_list_item(pkt);
				break;
		}
}

void parse_pkt(int socket, fd_set *read_fds) {
	uint16_t pkt_len;
	uint8_t *pkt, flag, hdr[HDR_LEN];
	int message_len = 0;

	message_len = recv(socket, hdr, HDR_LEN, MSG_PEEK);
	if (message_len < 0) {
		perror("recv call");
		exit(-1);
	}
	else if (!message_len) { // server exited
		printf("\nServer Terminated\n");
		exit_ok();
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

		perform_cmd(flag, pkt, read_fds);
	}
}

uint8_t *build_msg_pkt(uint8_t num_handles, handle **handles, char *message) {
	uint8_t *pkt, *temp;
	int i;
	uint16_t packet_len;
	uint8_t flag = 5;

	packet_len = HDR_LEN + 1 + client->len + 1;
	for (i=0; i<num_handles; i++) {
		if (!i) {
			packet_len++;
		}

		packet_len += 1 + handles[i]->len;
	}
	packet_len += strlen(message);

	pkt = malloc(sizeof(uint8_t) * packet_len);
	temp = pkt;
	packet_len = htons(packet_len);
	memcpy(temp, &packet_len, sizeof(uint16_t));
	temp += sizeof(uint16_t);
	memcpy(temp, &flag, sizeof(uint8_t));
	temp++;
	memcpy(temp, &(client->len), sizeof(uint8_t));
	temp++;
	memcpy(temp, client->name, sizeof(char) * client->len);
	temp += client->len;

	memcpy(temp, &num_handles, sizeof(uint8_t));
	temp++;

	for (i=0; i<num_handles; i++) {
		memcpy(temp, &(handles[i]->len), sizeof(uint8_t));
		temp++;
		memcpy(temp, handles[i]->name, sizeof(char) * handles[i]->len);
		temp += handles[i]->len;
	}

	strncpy((char *)temp, message, strlen(message));

	return pkt;
}

uint8_t *build_brdcst_pkt(char *message) {
	uint8_t *pkt, *temp, flag = 4;
	uint16_t packet_len = HDR_LEN;

	packet_len += 1 + client->len;
	packet_len += strlen(message);

	pkt = malloc(sizeof(uint8_t) * packet_len);
	temp = pkt;

	packet_len = htons(packet_len);
	memcpy(temp, &packet_len, sizeof(uint16_t));
	temp += 2;
	memcpy(temp, &flag, sizeof(uint8_t));
	temp++;
	memcpy(temp, &(client->len), sizeof(uint8_t));
	temp++;
	memcpy(temp, client->name, sizeof(char) * client->len);
	temp += client->len;
	strncpy((char *)temp, message, strlen(message));

	return pkt;
}

uint8_t *build_pkt_hdr(uint8_t flag) {
	uint8_t *pkt, *temp;
	uint16_t packet_len = HDR_LEN;

	if (flag == 1) {
		packet_len += 1 + client->len;
	}

	pkt = malloc(sizeof(uint8_t) * packet_len);
	temp = pkt;

	packet_len = htons(packet_len);
	memcpy(temp, &packet_len, sizeof(uint16_t));
	temp += 2;
	memcpy(temp, &flag, sizeof(uint8_t));

	if (flag == 1) {
		temp++;
		memcpy(temp, &(client->len), sizeof(uint8_t));
		temp++;
		memcpy(temp, client->name, sizeof(char) * client->len);
	}

	return pkt;
}

void send_message(uint8_t num_handles, handle **handles, char **message) {
	int i = 0;
	uint8_t **pkt = malloc(sizeof(uint8_t *));

	while (*message) {
		if (i) {
			pkt = realloc(pkt, sizeof(uint8_t *) * (i + 1));
		}
		*(pkt + i) = build_msg_pkt(num_handles, handles, *message);

		i++;
		message++;
	}

	i = 1;
	while (*pkt) {
		send_pkt(*pkt);

		pkt++;
	}
}

void send_broadcast(char **message) {
	int i = 0;
	uint8_t **pkt = malloc(sizeof(uint8_t *));

	while (*message) {
		if (i) {
			pkt = realloc(pkt, sizeof(uint8_t *) * (i + 1));
		}
		*(pkt + i) = build_brdcst_pkt(*message);

		i++;
		message++;
	}

	i = 1;
	while (*pkt) {
		send_pkt(*pkt);

		pkt++;
	}
}

void list_handles() {
	uint8_t *pkt = build_pkt_hdr(10);
	char input[MAX_HANDLE_LEN];

	fgets(input, MAX_HANDLE_LEN, stdin);

	send_pkt(pkt);
}

void exit_cmd() {
	uint8_t *pkt = build_pkt_hdr(8);

	send_pkt(pkt);
}

void parse_msg_cmd() {
	handle **handles;
	char input[MAX_LINE_LEN + 1], **message, *endptr, name[MAX_HANDLE_LEN + 1];
	uint8_t num_handles = 0, i = 0, len = 0;
	int index = 0;

	fgets(input, MAX_LINE_LEN + 1, stdin);
	while (index < MAX_LINE_LEN && input[index] == ' ') {
			index++;
	}

	num_handles = (int) strtol(input, &endptr, 10);
	if (endptr == input || !num_handles) { // no digits found
		num_handles = 1;
	}
	else if (num_handles > 9) { // found more than one digit
		report_error("found more than one digit while parsing msg cmd");
	}
	else {
		index += 2;
	}

	handles = malloc(sizeof(handle *) * num_handles);
	for (; i<num_handles; i++) {
		handles[i] = malloc(sizeof(handle));
		while (index < MAX_LINE_LEN && input[index] == ' ') {
			index++;
		}
		sscanf(input + index, "%250s", name);
		handles[i]->len = strlen(name);
		handles[i]->name = malloc(sizeof(char) * handles[i]->len);
		memcpy(handles[i]->name, name, sizeof(char) * handles[i]->len);
		index += handles[i]->len + 1;
	}

	message = malloc(sizeof(char *));
	i = 0;
	len = strlen(input);
	while (len - index > 0) {
		if (i) {
			message = realloc(message, sizeof(char *) * (i + 1));
		}
		message[i] = malloc(sizeof(char) * MAX_MESSAGE_LEN + 1);
		while (index < MAX_LINE_LEN && input[index] == ' ') {
			index++;
		}
		sscanf(input + index, "%1000[^`]s", message[i++]);
		index += MAX_MESSAGE_LEN;
	}
	send_message(num_handles, handles, message);
}

void parse_brdcst_cmd() {
	char input[MAX_LINE_LEN + 1], **message;
	int i = 0, index = 0, len = 0;

	fgets(input, MAX_LINE_LEN + 1, stdin);

	message = malloc(sizeof(char *));
	len = strlen(input);
	while (len - index > 0) {
		if (i) {
			message = realloc(message, sizeof(char *) * (i + 1));
		}
		message[i] = malloc(sizeof(char) * MAX_MESSAGE_LEN + 1);

		while (index < MAX_LINE_LEN && input[index] == ' ') {
			index++;
		}

		sscanf(input + index, "%1000[^`]s", message[i++]);
		index += MAX_MESSAGE_LEN;
	}

	send_broadcast(message);
}

void parse_cmds() {
	char cmd[4];

	fgets(cmd, 3, stdin);
	if (cmd[0] != '%') {
		printf("Invalid Command\n");
	}
	switch (cmd[1]) {
		case 'M':
		case 'm':
			parse_msg_cmd();
			break;
		case 'B':
		case 'b':
			parse_brdcst_cmd();
			break;
		case 'L':
		case 'l':
			list_handles();
			break;
		case 'E':
		case 'e':
			exit_cmd();
			break;
		default:
			printf("Invalid Command\n");
	}
}

void reset_fds(fd_set *read_fds, int set_stdin) {
	FD_ZERO(read_fds);
	FD_SET(client->socket_num, read_fds);
	if (set_stdin) {
		FD_SET(0, read_fds);
	}
}

void read_input(fd_set *read_fds) {
	reset_fds(read_fds, 1);
	printf("$: ");
	fflush(stdout);

	if (select(client->socket_num + 1, read_fds, NULL, NULL, NULL) < 0) {
		report_error("select error while reading input");
	}

	if (FD_ISSET(0, read_fds)) {
		parse_cmds();
	}

	if (FD_ISSET(client->socket_num, read_fds)) {
		parse_pkt(client->socket_num, read_fds);
	}

	read_input(read_fds);
}

void read_first_pkt(fd_set *read_fds) {
	reset_fds(read_fds, 0);

	if (select(client->socket_num + 1, read_fds, NULL, NULL, NULL) < 0) {
		report_error("select error while reading first pkt");
	}

	if (FD_ISSET(client->socket_num, read_fds)) {
		parse_pkt(client->socket_num, read_fds);
	}
}

void send_first_pkt() {
	uint8_t *pkt;
	uint8_t flag = 1;
	uint16_t pkt_len = HDR_LEN + 1 + client->len;

	pkt = malloc(sizeof(uint8_t) * pkt_len);
	pkt_len = htons(pkt_len);
	memcpy(pkt, &pkt_len, sizeof(uint16_t));
	memcpy(pkt + sizeof(uint16_t), &flag, sizeof(uint8_t));
	memcpy(pkt + HDR_LEN, &(client->len), sizeof(uint8_t));
	memcpy(pkt + HDR_LEN + sizeof(uint8_t), client->name, client->len);

	send_pkt(pkt);
}

void setup_select(char *name, char *port) {
	fd_set *read_fds = malloc(sizeof(fd_set));

	client->socket_num = tcpClientSetup(name, port);

	send_first_pkt();

	read_first_pkt(read_fds);
}

void parse_args(int argc, char *argv[]) {
	client = malloc(sizeof(handle));

	if (argc != 4) {
		printf("usage: %s handle server-name server-port\n", argv[0]);
		exit(1);
	}

	client->len = strlen(argv[1]);
	client->name = malloc(sizeof(char) * client->len);
	memcpy(client->name, argv[1], sizeof(char) * client->len);
	setup_select(argv[2], argv[3]);
}

int main(int argc, char * argv[])
{
	parse_args(argc, argv);
	
	return 0;
}


