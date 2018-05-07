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
	WAIT_ON_FILENAME,
	TIMEOUT_ON_FILENAME,
	WAIT_ON_READY_FOR_DATA,
	TIMEOUT_ON_READY_FOR_DATA,
	FILENAME,
	SEND_DATA,
	WAIT_ON_RR,
	TIMEOUT_ON_RR,
	WAIT_ON_EOF_ACK,
	TIMEOUT_ON_EOF_ACK
};

void process_server(int server_sk_num);
void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client);
STATE filename(Connection *client, uint8_t *buf, uint8_t *packet, int32_t *recv_len, int32_t *data_file);
STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num, uint32_t win_size, uint32_t *last_rr);
STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len);
STATE timeout_on_eof_ack(Connection *client, uint8_t *packet, int32_t packet_len);
STATE wait_on_ack(Connection *client);
STATE wait_on_eof_ack(Connection *client);
int parse_args(int argc, char **argv);
STATE handshake(Connection *client, uint8_t *buf, uint32_t *win_size, int32_t *buf_size, uint8_t *packet, int32_t *packet_len);
STATE wait_on_filename(Connection *client, uint8_t *buf, uint8_t *packet, int32_t *packet_len);
STATE timeout_on_filename(Connection *client, uint8_t *packet, int32_t packet_len);
STATE wait_on_ready_for_data(Connection *client, int32_t data_file, uint32_t win_size, int32_t buf_size, int32_t *seq_num, uint8_t *packet, int32_t *packet_len);
STATE timeout_on_ready_for_data(Connection *client, uint8_t *packet, int32_t packet_len);
STATE wait_on_rr(Connection *client, int32_t *rr_num, int32_t *srej_num);
STATE timeout_on_rr(Connection *client, uint8_t *packet, int32_t packet_len);

Window *window = NULL;

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

void update_window(int32_t data_file, uint32_t win_size, int32_t buf_size, uint32_t start_seq_num, uint32_t num_to_update, int first) {
	uint32_t index = start_seq_num % win_size;
	int i;
	int32_t len_read;
	uint8_t buf[MAX_LEN];

	printf("updating window - bottom: %d, num to update: %d\n", start_seq_num + num_to_update, num_to_update);

	// if not filling up window for the first time
	if (!first) { 
		start_seq_num += win_size;
	}

	for (i=0; i < num_to_update; i++) {
		len_read = read(data_file, buf, buf_size);
		printf("storing in index: %d\n", index);

		if (len_read == -1) {
			perror("read_data");
			exit(-1);
		}
		else if (len_read == 0) {
			window[index].status = RECVD;
			printf("packet at index: %d marked as recvd\n", index);
			continue;
		}

		memcpy(window[index].buf, buf, MAX_LEN);
		window[index].seq_num = start_seq_num++;
		window[index].packet_len = len_read;
		window[index].status = NO_STATUS;

		if (len_read < buf_size) {
			window[index].last = 1;
		}
		else {
			window[index].last = 0;
		}

		index = start_seq_num % win_size;
	}
}

int window_done(uint32_t win_size) {
	int i;
	for (i=0; i<win_size; i++) {
		if (window[i].status != RECVD) {
			return 0;
		}
	}
	
	printf("entire window processed\n");
	return 1;
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
			if (recv_len != CRC_ERR && flag == SETUP) {
				printf("Server: got setup request\n");
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
	uint32_t win_size = 0;
	int32_t seq_num = START_SEQ_NUM;
	uint32_t last_rr = START_SEQ_NUM;
	int32_t rr_num = 0;
	int32_t srej_num = 0;
	Window *data;

	while (state != DONE) {
		switch(state) {
			case START:
				state = handshake(client, buf, &win_size, &buf_size, packet, &packet_len);
				break;
			case WAIT_ON_FILENAME:
				state = wait_on_filename(client, buf, packet, &packet_len);
				break;
			case TIMEOUT_ON_FILENAME:
				state = timeout_on_filename(client, packet, packet_len);
				break;
			case FILENAME:
				state = filename(client, buf, packet, &packet_len, &data_file);
				break;
			case WAIT_ON_READY_FOR_DATA:
				state = wait_on_ready_for_data(client, data_file, win_size, buf_size, &seq_num, packet, &packet_len);
				break;
			case TIMEOUT_ON_READY_FOR_DATA:
				state = timeout_on_ready_for_data(client, packet, packet_len);
				break;
			case SEND_DATA:
				state = send_data(client, packet, &packet_len, data_file, buf_size, &seq_num, win_size, &last_rr);
				break;
			case WAIT_ON_RR:
				state = wait_on_rr(client, &rr_num, &srej_num);
				if (state == SEND_DATA) {
					if (rr_num) {
						update_window(data_file, win_size, buf_size, last_rr, rr_num - last_rr, 0);
						last_rr = rr_num;
					}
					else {
						data = &window[srej_num % win_size];
						send_buf(data->buf, data->packet_len, client, DATA, data->seq_num, data->packet);
						data->status = LOST;
					}
				}
				break;
			case TIMEOUT_ON_RR:
				state = timeout_on_rr(client, packet, packet_len);
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

STATE handshake(Connection *client, uint8_t *buf, uint32_t *win_size, int32_t *buf_size, uint8_t *packet, int32_t *packet_len) {
	printf("Server: Starting handshake\n");
	// extract buffer size and window size used for sending data
	memcpy(buf_size, buf, SIZE_OF_BUF_SIZE);
	*buf_size = ntohl(*buf_size);
	memcpy(win_size, &buf[sizeof(*buf_size)], SIZE_OF_BUF_SIZE);
	*win_size = ntohl(*win_size);
	printf("Server: buf_size - %d, win_size - %d\n", *buf_size, *win_size);

	window = malloc(sizeof(Window) * (*win_size));

	// create client socket
	if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("open client sock");
		exit(-1);
	}

	printf("Server: Created client socket\n");

	(*packet_len) = send_buf(NULL, 0, client, SETUP_RESPONSE, 0, packet);

	printf("Server: sent buffer\n");

	return WAIT_ON_FILENAME;
}

STATE wait_on_filename(Connection *client, uint8_t *buf, uint8_t *packet, int32_t *packet_len) {
	printf("Server: wating on filename\n");
	STATE return_val = DONE;
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	static int retry_cnt = 0;
	int32_t recv_len;

	if ((return_val = process_select(client, &retry_cnt, TIMEOUT_ON_FILENAME, FILENAME, DONE)) == FILENAME) {
		recv_len = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

		// if crc error ignore packet
		if (recv_len == CRC_ERR) {
			return_val = WAIT_ON_FILENAME;
		}
		else if (flag == SETUP) {
			if (sendtoErr(client->sk_num, packet, *packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
				perror("resent_setup_response");
				exit(-1);
			}
		}
		else if (flag != FNAME) {
			printf("This should never happen\n");
			return_val = DONE;
		}
		else {
			*packet_len = recv_len;
		}
	}

	return return_val;
}

STATE timeout_on_filename(Connection *client, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_filename");
		exit(-1);
	}

	return WAIT_ON_FILENAME;
}

STATE filename(Connection *client, uint8_t *buf, uint8_t *packet, int32_t *recv_len, int32_t *data_file) {
	uint8_t response[1];
	char fname[MAX_LEN];
	STATE return_val = DONE;

	// extract buffer size used for sending data and also filename
	memcpy(fname, buf, *recv_len);
	printf("Server: recvd filename %s\n", fname);

	if (((*data_file) = open(fname, O_RDONLY)) < 0) {
		(*recv_len) = send_buf(response, 0, client, FNAME_BAD, 1, packet);
		return_val = DONE;
	}
	else {
		(*recv_len) = send_buf(response, 0, client, FNAME_OK, 1, packet);
		return_val = WAIT_ON_READY_FOR_DATA;
	}

	return return_val;
}

STATE wait_on_ready_for_data(Connection *client, int32_t data_file, uint32_t win_size, int32_t buf_size, int32_t *seq_num, uint8_t *packet, int32_t *packet_len) {
	printf("Server: waiting on ready response from client\n");
	STATE return_val = DONE;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq = 0;
	uint32_t crc_check = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(client, &retry_cnt, TIMEOUT_ON_READY_FOR_DATA, SEND_DATA, DONE)) == SEND_DATA) {
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_READY_FOR_DATA;
		}
		else if (flag == FNAME) {
			return_val = TIMEOUT_ON_READY_FOR_DATA;
		}
		else if (flag != READY) {
			printf("This should never happen\n");
			return_val = DONE;
		}
		else {
			update_window(data_file, win_size, buf_size, *seq_num, win_size, 1);
		}
	}

	return return_val;
}

STATE timeout_on_ready_for_data(Connection *client, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_filename");
		exit(-1);
	}

	return WAIT_ON_READY_FOR_DATA;
}

STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num, uint32_t win_size, uint32_t *last_rr) {
	uint8_t buf[MAX_LEN];
	STATE return_val = DONE;
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq = *seq_num;
	uint32_t num = 0;
	int32_t crc_check = 0;
	Window *data;

	while (1) {
		data = &window[(*seq_num)++ % win_size];
		// send next packet
		if (data->status != RECVD && data-> status != LOST) {
			send_buf(data->buf, data->packet_len, client, DATA, data->seq_num, data->packet);
			printf("sent data with sequence number: %d\n", data->seq_num);
			printf("data: %s\n", (char *) data->packet);
			data->status = SENT;
		}
		// check for RR or SREJ
		if (select_call(client->sk_num, 0, 0, NOT_NULL) == 1) {
			crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq);

			if (crc_check != CRC_ERR) {
				if (flag == RR) {
					memcpy(&num, buf, sizeof(uint32_t));
					num = ntohl(num);
					printf("recvd RR%d\n", num);
					update_window(data_file, win_size, buf_size, *last_rr, num - *last_rr, 0);
					*last_rr = num;
				}
				else if (flag == SREJ) {
					memcpy(&num, buf, sizeof(uint32_t));
					num = ntohl(num);
					printf("recvd SREJ%d\n", num);
					data = &window[num % win_size];
					send_buf(data->buf, data->packet_len, client, DATA, data->seq_num, data->packet);
					printf("resending packet with sequence number: %d\n", data->seq_num);
					data->status = LOST;
					// printf("data: %s\n", (char *) data->packet);
				}
				else {
					// printf("This should never happen\n");
					return_val = DONE;
					break;
				}
			}
		}
		if (window_done(win_size)) {
			(*packet_len) = send_buf(NULL, 0, client, END_OF_FILE, *seq_num, packet);
			return_val = WAIT_ON_EOF_ACK;
			break;
		}
		if (*seq_num >= *last_rr + win_size) {
			packet = window[*last_rr % win_size].packet;
			*packet_len = window[*last_rr % win_size].packet_len + sizeof(Header);
			printf("filled up window and didn't recv any RR's, going to wait\n");
			printf("saved bottom - %d, with flag: %d", ntohl(*((int32_t *)packet)), packet[2 * sizeof(uint32_t)]);
			return_val = WAIT_ON_RR;
			break;
		}
	}

	return return_val;
}

STATE wait_on_rr(Connection *client, int32_t *rr_num, int32_t *srej_num) {
	STATE return_val = DONE;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(client, &retry_cnt, TIMEOUT_ON_RR, SEND_DATA, DONE)) == SEND_DATA) {
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_RR;
		}
		else if (flag == RR) {
			memcpy(rr_num, buf, sizeof(uint32_t));
			*rr_num = ntohl(*rr_num);
			printf("recvd RR%d\n", *rr_num);
			*srej_num = 0;
		}
		else if (flag == SREJ) {
			memcpy(srej_num, buf, sizeof(uint32_t));
			*srej_num = ntohl(*srej_num);
			printf("recvd SREJ%d\n", *srej_num);
			*rr_num = 0;
		}
		else if (flag != RR && flag != SREJ) {
			printf("This should never happen\n");
			return_val = DONE;
		}
	}

	return return_val;
}

STATE timeout_on_rr(Connection *client, uint8_t *packet, int32_t packet_len) {
	int32_t seq = 0;

	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_rr");
		exit(-1);
	}

	memcpy(&seq, packet, sizeof(int32_t));
	seq = ntohl(seq);

	printf("timed out while waiting for RR, resending data with sequence number: %d\n", seq);
	// printf("data: %s\n", (char *) &packet[sizeof(Header)]);

	return WAIT_ON_RR;
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

STATE timeout_on_eof_ack(Connection *client, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(client->sk_num, packet, packet_len, 0, (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_eof_ack");
		exit(-1);
	}

	return WAIT_ON_EOF_ACK;
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