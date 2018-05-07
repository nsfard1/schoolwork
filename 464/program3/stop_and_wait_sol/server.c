#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"
#include "srej.h"
#include "cpe464.h"

typedef enum State STATE;

enum State {
	START,
	DONE,
	FILENAME,
	SEND_DATA,
	WAIT_ON_ACK,
	TIMEOUT_ON_ACK,
	WAIT_ON_EOF_ACK,
	TIMEOUT_ON_EOF_ACK
};

void process_server(int server_sk_num);
void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client);
STATE filename(Connection *client, uint8_t *buf, int32_t recv_len, int32_t *data_file, int32_t *buf_size);
STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num);
STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len);
STATE timeout_on_eof_ack(Connection *client, uint8_t *packet, int32_t packet_len);
STATE wait_on_ack(Connection *client);
STATE wait_on_eof_ack(Connection *client);
int parse_args(int argc, char **argv);

int main(int argc, char **argv) {
	int32_t server_sk_num = 0;
	int port_num = 0;

	port_num = parse_args(argc, argv);

	sendErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	server_sk_num = udp_server(port_num);
	process_server(server_sk_num);

	return 0;
}

void report_error() {
	perror("Error!");
	exit(-1);
}

void process_server(int server_sk_num) {
	pid_t pid = 0;
	int status = 0;
	uint8_t buf[MAX_LEN];
	Connection client;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t recv_len = 0;

	while (1) {
		if (select_call(server_sk_num, LONG_TIME, 0, SET_NULL) == 1) {
			recv_len = recv_buf(buf, MAX_LEN, server_sk_num, &client, &flag, &seq_num);
			if (recv_len != CRC_ERR) {
				if ((pid = fork()) < 0) {
					perror("fork");
					exit(-1);
				}
				// child process
				if (pid == 0) {
					process_client(server_sk_num, buf, recv_len, &client);
					exit (0);
				}
			}

			// check to see if any children quit (parent)
			while (waitpid(-1, &status, WNOHANG) > 0) {

			}
		}
	}
}

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client) {
	STATE state = START;
	int32_t data_file = 0;
	int32_t packet_len = 0;
	uint8_t packet[MAX_LEN];
	int32_t buf_size = 0;
	int32_t seq_num = START_SEQ_NUM;

	while (state != DONE) {
		switch(state) {
			case START:
				state = FILENAME;
				break;
			case FILENAME:
				state = filename(client, buf, recv_len, &data_file, &buf_size);
				break;
			case SEND_DATA:
				state = send_data(client, packet, &packet_len, data_file, buf_size, &seq_num);
				break;
			case WAIT_ON_ACK:
				state = wait_on_ack(client);
				break;
			case TIMEOUT_ON_ACK:
				state = timeout_on_ack(client, packet, packet_len);
				break;
			case WAIT_ON_EOF_ACK:
				state = wait_on_eof_ack(client);
				break;
			case TIMEOUT_ON_EOF_ACK:
				state = timeout_on_eof_ack(client, packet, packet_len);
				break;
			case DONE:
				break;
			default:
				printf("In default\n");
				state = DONE;
				break;
		}
	}
}

STATE filename(Connection *client, uint8_t *buf, int32_t recv_len, int32_t *data_file, int32_t *buf_size) {
	uint8_t response[1];
	char fname[MAX_LEN];
	STATE return_val = DONE;

	// extract buffer size used for sending data and also filename
	memcpy(buf_size, buf, SIZE_OF_BUF_SIZE);
	*buf_size = ntohl(*buf_size);
	memcpy(fname, &buf[sizeof(*buf_size)], recv_len - SIZE_OF_BUF_SIZE);

	// create client socket
	if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("open client sock");
		exit(-1);
	}

	if (((*data_file) = open(fname, O_RDONLY)) < 0) {
		send_buf(response, 0, client, FNAME_BAD, 0, buf);
		return_val = DONE;
	}
	else {
		send_buf(response, 0, client, FNAME_OK, 0, buf);
		return_val = SEND_DATA;
	}

	return return_val;
}

STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num) {
	uint8_t buf[MAX_LEN];
	int32_t len_read = 0;
	STATE return_val = DONE;

	len_read = read(data_file, buf, buf_size);

	switch (len_read) {
		case -1:
			perror("send_data");
			return_val = DONE;
			break;
		case 0:
			(*packet_len) = send_buf(buf, 1, client, END_OF_FILE, *seq_num, packet);
			return_val = WAIT_ON_EOF_ACK;
			break;
		default:
			(*packet) = send_buf(buf, len_read, client, DATA, *seq_num, packet);
			return_val = WAIT_ON_ACK;
			break;
	}

	return return_val;
}

STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_ack");
		exit(-1);
	}

	return WAIT_ON_ACK;
}

STATE timeout_on_eof_ack(Connection *client, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_eof_ack");
		exit(-1);
	}

	return WAIT_ON_EOF_ACK;
}

STATE wait_on_ack(Connection *client) {
	STATE return_val = DONE;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(client, &retry_cnt, TIMEOUT_ON_ACK, SEND_DATA, DONE)) == SEND_DATA) {
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_ACK;
		}
		else if (flag != ACK) {
			printf("This should never happen\n");
			return_val = DONE;
		}
	}

	return return_val;
}

STATE wait_on_eof_ack(Connection *client) {
	STATE return_val = DONE;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	uint32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(client, &retry_cnt, TIMEOUT_ON_EOF_ACK, DONE, DONE)) == DONE) {
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_EOF_ACK;
		}
		else if (flag != EOF_ACK) {
			printf("this should never happen\n");
			return_val = DONE;
		}
		else {
			printf("File transfer completed successfully.\n");
			return_val = DONE;
		}
	}

	return return_val;
}

int parse_args(int argc, char **argv) {
	int port_num;
	float error_percent;
	char *ptr;

	if (argc != 2 && argc != 3) {
		printf("bad argc\n");
		report_error();
	}

	error_percent = strtof(argv[1], &ptr);
	if (error_percent < 0 || error_percent > 1) {
		printf("bad err percent\n");
		report_error();
	}

	if (argc == 3) {
		port_num = strtol(argv[2], &ptr, 10);
		if (port_num <= 0) {
			report_error();
		}
	}
	else {
		port_num = 0;
	}

	return port_num;
}