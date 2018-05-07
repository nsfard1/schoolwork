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

#include "networks.h"
#include "srej.h"
#include "cpe464.h"

typedef enum State STATE;

enum State {
	DONE,
	FILENAME,
	RECV_DATA,
	FILE_OK,
	START_STATE,
	WAIT_ON_HANDSHAKE,
	WAIT_ON_FN_ACK,
	TIMEOUT_ON_HANDSHAKE,
	TIMEOUT_ON_FN_ACK
};

STATE start_state(char **argv, Connection *server, uint8_t *packet, int32_t *packet_len, uint32_t window_size, int32_t buf_size);
STATE filename(char *fname, Connection *server, uint8_t *packet, int32_t *packet_len);
STATE recv_data(int32_t output_file, Connection *server, uint32_t window_size, int32_t *seq);
STATE file_ok(Connection *server, int *output_file_fd, char *output_file_name, uint8_t *packet, int32_t *packet_len);
void parse_args(int argc, char **argv);
STATE wait_on_handshake(Connection *server);
STATE timeout_on_handshake(Connection *server, uint8_t *packet, int32_t packet_len);
STATE wait_on_fn_ack(Connection *server, char *fname);
STATE timeout_on_fn_ack(Connection *server, uint8_t *packet, int32_t packet_len);

Window *buffer;

int main(int argc, char **argv) {
	// rcopy local-file remote-file window-size buffer-size error-perc remote-machine remote-port
	Connection server;
	int32_t output_file_fd = 0;
	STATE state = START_STATE;
	uint8_t packet[MAX_LEN];
	int32_t packet_len = 0;
	int32_t buf_size = 0;
	uint32_t window_size = 0;
	int32_t seq_num = START_SEQ_NUM + 1;

	parse_args(argc, argv);
	buf_size = atoi(argv[4]);
	window_size = atoi(argv[3]);
	buffer = malloc(window_size * sizeof(Window));

	sendErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	while (state != DONE) {
		switch (state) {
			case START_STATE:
				state = start_state(argv, &server, packet, &packet_len, window_size, buf_size);
				break;
			case WAIT_ON_HANDSHAKE:
				state = wait_on_handshake(&server);
				break;
			case TIMEOUT_ON_HANDSHAKE:
				state = timeout_on_handshake(&server, packet, packet_len);
				break;
			case FILENAME:
				printf("Client: received setup response\n");
				state = filename(argv[2], &server, packet, &packet_len);
				break;
			case WAIT_ON_FN_ACK:
				state = wait_on_fn_ack(&server, argv[2]);
				break;
			case TIMEOUT_ON_FN_ACK:
				state = timeout_on_fn_ack(&server, packet, packet_len);
				break;
			case FILE_OK:
				printf("Client: received good filename ack\n");
				state = file_ok(&server, &output_file_fd, argv[1], packet, &packet_len);
				break;
			case RECV_DATA:
				state = recv_data(output_file_fd, &server, window_size, &seq_num);
				break;
			case DONE:
				break;
			default:
				printf("ERROR in default state\n");
				break;
		}
	}

	return 0;
}

void report_error() {
	perror("Error!");
	exit(-1);
}

void flush_buffer(Connection *server, uint32_t window_size, int32_t output_file, int32_t *seq, int32_t *start) {
	int i;
	uint8_t packet[MAX_LEN];
	int32_t seq_num = 0;
	printf("flushing buffer\n");

	for (i=0; i<window_size; i++) {
		if (buffer[*start % window_size].status == NEED_TO_FLUSH) {
			write(output_file, buffer[*start % window_size].packet, buffer[*start % window_size].packet_len);
			seq_num = htonl(buffer[*start % window_size].seq_num);
			memcpy(packet, &seq_num, sizeof(uint32_t));
			buffer[*start % window_size].status = FLUSHED;
			(*start)++;
		}
		else {
			break;
		}
	}

	send_buf(packet, sizeof(uint32_t), server, RR, (*seq)++, packet);
}

void send_srej(Connection *server, int32_t *seq_num, int32_t lost_seq_num) {
	uint8_t buf[MAX_LEN];
	printf("sending SREJ%d\n", lost_seq_num);

	lost_seq_num = htonl(lost_seq_num);
	memcpy(buf, &lost_seq_num, sizeof(uint32_t));
	send_buf(buf, sizeof(uint32_t), server, SREJ, (*seq_num)++, buf);
}

STATE start_state(char **argv, Connection *server, uint8_t *packet, int32_t *packet_len, uint32_t window_size, int32_t buf_size) {
	// returns FILENAME if no error, otherwise DONE
	STATE return_val = DONE;
	uint8_t buf[MAX_LEN];

	buf_size = htonl(buf_size);
	window_size = htonl(window_size);
	memcpy(buf, &buf_size, SIZE_OF_BUF_SIZE);
	memcpy(&buf[SIZE_OF_BUF_SIZE], &window_size, SIZE_OF_BUF_SIZE);

	if (server->sk_num > 0) {
		close(server->sk_num);
	}

	if (udp_client_setup(argv[6], atoi(argv[7]), server) < 0) {
		return_val = DONE;
	}
	else {
		printf("Client: setup socket, sending setup request\n");
		(*packet_len) = send_buf(buf, 2 * SIZE_OF_BUF_SIZE, server, SETUP, 0, packet);
		return_val = WAIT_ON_HANDSHAKE;
	}

	return return_val;
}

STATE wait_on_handshake(Connection *server) {
	printf("Client: waiting on setup response\n");
	STATE return_val = DONE;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	uint32_t crc_check = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(server, &retry_cnt, TIMEOUT_ON_HANDSHAKE, FILENAME, DONE)) == FILENAME) {
		crc_check = recv_buf(buf, len, server->sk_num, server, &flag, &seq_num);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_HANDSHAKE;
		}
		else if (flag != SETUP_RESPONSE) {
			printf("This should never happen\n");
			return_val = DONE;
		}
	}

	return return_val;
}

STATE timeout_on_handshake(Connection *server, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(server->sk_num, packet, packet_len, 0, (struct sockaddr *) &(server->remote), server->len) < 0) {
		perror("timeout_on_handshake");
		exit(-1);
	}

	return WAIT_ON_HANDSHAKE;
}

STATE filename(char *fname, Connection *server, uint8_t *packet, int32_t *packet_len) {
	// return start_state if no reply, done if bad filename, file_ok otherwise
	STATE return_val = WAIT_ON_FN_ACK;
	uint8_t buf[MAX_LEN];
	// uint8_t flag = 0;
	// int32_t seq_num = 0;
	int32_t fname_len = strlen(fname) + 1;
	// int32_t recv_check = 0;
	// static int retry_cnt = 0;

	memcpy(buf, fname, fname_len);

	printf("Client: sending filename: %s\n", fname);
	(*packet_len) = send_buf(buf, fname_len, server, FNAME, 1, packet);

	// if ((return_val = process_select(server, &retry_cnt, START_STATE, FILE_OK, DONE)) == FILE_OK) {
	// 	recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);

	// 	if (recv_check == CRC_ERR) {
	// 		return_val = START_STATE;
	// 	}
	// 	else if (flag == FNAME_BAD) {
	// 		printf("File %s not found\n", fname);
	// 		return_val = DONE;
	// 	}
	// }

	return return_val;
}

STATE wait_on_fn_ack(Connection *server, char *fname) {
	printf("Client: waiting on filename ack\n");
	STATE return_val = DONE;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	uint32_t crc_check = 0;
	static int retry_cnt = 0;

	if ((return_val = process_select(server, &retry_cnt, TIMEOUT_ON_FN_ACK, FILE_OK, DONE)) == FILE_OK) {
		crc_check = recv_buf(buf, len, server->sk_num, server, &flag, &seq_num);

		// if crc error ignore packet
		if (crc_check == CRC_ERR) {
			return_val = WAIT_ON_FN_ACK;
		}
		else if (flag == SETUP_RESPONSE) {
			return_val = FILENAME;
		}
		else if (flag == FNAME_BAD) {
			printf("File %s not found\n", fname);
			return_val = DONE;
		}
		else if (flag != FNAME_OK && flag != FNAME_BAD) {
			printf("This should never happen\n");
			return_val = DONE;
		}
	}

	return return_val;
}

STATE timeout_on_fn_ack(Connection *server, uint8_t *packet, int32_t packet_len) {
	if (sendtoErr(server->sk_num, packet, packet_len, 0, (struct sockaddr *) &(server->remote), server->len) < 0) {
		perror("timeout_on_fn_ack");
		exit(-1);
	}

	return WAIT_ON_FN_ACK;
}

STATE recv_data(int32_t output_file, Connection *server, uint32_t window_size, int32_t *seq) {
	int32_t seq_num = 0;
	uint8_t flag = 0;
	int32_t data_len = 0;
	uint8_t data_buf[MAX_LEN];
	uint8_t packet[MAX_LEN];
	static int32_t flush = 0;
	static int32_t temp_seq_num = 0;
	Window temp;
	static int32_t expected_seq_num = START_SEQ_NUM;

	if (select_call(server->sk_num, LONG_TIME, 0, NOT_NULL) == 0) {
		printf("timeout after 10 secs, server gone\n");
		return DONE;
	}

	data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
	printf("recvd data with sequence number: %d\n", seq_num);
	// printf("data: %s\n", (char *) data_buf);

	if (data_len == CRC_ERR) {
		printf("crc error\n");
		return RECV_DATA;
	}

	if (flag == END_OF_FILE) {
		send_buf(NULL, 0, server, EOF_ACK, *seq, packet);
		printf("File done\n");
		return DONE;
	}

	if (seq_num == expected_seq_num) {
		expected_seq_num++;
		write(output_file, &data_buf, data_len);
		if (flush != expected_seq_num) {
			// send RR
			printf("sending RR%d\n", expected_seq_num);
			seq_num = htonl(expected_seq_num);
			memcpy(packet, &seq_num, sizeof(uint32_t));
			send_buf(packet, sizeof(uint32_t), server, RR, (*seq)++, packet);
		}
		else {
			flush_buffer(server, window_size, output_file, seq, &flush);
			expected_seq_num = flush;
			printf("now expecting sequence number: %d\n", expected_seq_num);
		}
	}
	else if (seq_num > expected_seq_num) {
		// store in buffer
		temp_seq_num = seq_num;
		memcpy(temp.packet, data_buf, MAX_LEN);
		temp.packet_len = data_len;
		temp.seq_num = seq_num;
		temp.status = NEED_TO_FLUSH;
		memcpy(&buffer[seq_num % window_size], &temp, sizeof(Window));
		if (!flush) {
			flush = seq_num;
			// send SREJ
			send_srej(server, seq, expected_seq_num);
		}
	}

	return RECV_DATA;
}

STATE file_ok(Connection *server, int *output_file_fd, char *output_file_name, uint8_t *packet, int32_t *packet_len) {
	STATE return_val = DONE;

	if ((*output_file_fd = open(output_file_name, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) {
		perror("file open error");
		return_val = DONE;
	}
	else {
		printf("Client: sending ready response\n");
		(*packet_len) = send_buf(NULL, 0, server, READY, 2, packet);
		return_val = RECV_DATA;
	}

	return return_val;
}

void parse_args(int argc, char **argv) {
	int buffer_size = 0;
	char *ptr;
	float error_percent;
	uint32_t window_size = 0;

	if (argc != 8) {
		report_error();
	}

	if (strlen(argv[1]) > 1000) {
		printf("Local filename too long, needs to be less than 1000.\n");
		exit(-1);
	}

	if (strlen(argv[2]) > 1000) {
		printf("Remote filename too long, needs to be less than 1000.\n");
		exit(-1);
	}

	window_size = atoi(argv[3]);
	if (window_size <= 0) {
		printf("Window size needs to be a positive integer.\n");
		exit(-1);
	}

	buffer_size = atoi(argv[4]);
	if (buffer_size < 400 || buffer_size > 1400) {
		printf("Buffer size needs to be between 400 and 1400.\n");
		exit(-1);
	}

	error_percent = strtof(argv[5], &ptr);
	if (error_percent < 0 || error_percent >= 1) {
		printf("Error rate needs to be between 0 and less than 1.\n");
		exit(-1);
	}
}