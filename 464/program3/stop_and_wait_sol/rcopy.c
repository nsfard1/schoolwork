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
	START_STATE
};

STATE start_state(char **argv, Connection *server);
STATE filename(char *fname, int32_t buf_size, Connection *server);
STATE recv_data(int32_t output_file, Connection *server);
STATE file_ok(int *output_file_fd, char *output_file_name);
void parse_args(int argc, char **argv);

int main(int argc, char **argv) {
	// rcopy local-file remote-file window-size buffer-size error-perc remote-machine remote-port
	Connection server;
	int32_t output_file_fd = 0;
	STATE state = START_STATE;

	parse_args(argc, argv);

	sendErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	while (state != DONE) {
		switch (state) {
			case START_STATE:
				state = start_state(argv, &server);
				break;
			case FILENAME:
				state = filename(argv[1], atoi(argv[4]), &server);
				break;
			case FILE_OK:
				state = file_ok(&output_file_fd, argv[2]);
				break;
			case RECV_DATA:
				state = recv_data(output_file_fd, &server);
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

STATE start_state(char **argv, Connection *server) {
	// returns FILENAME if no error, otherwise DONE
	STATE return_val = DONE;

	if (server->sk_num > 0) {
		close(server->sk_num);
	}

	if (udp_client_setup(argv[6], atoi(argv[7]), server) < 0) {
		return_val = DONE;
	}
	else {
		return_val = FILENAME;
	}

	return return_val;
}

STATE filename(char *fname, int32_t buf_size, Connection *server) {
	// return start_state if no reply, done if bad filename, file_ok otherwise
	STATE return_val = START_STATE;
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t fname_len = strlen(fname) + 1;
	int32_t recv_check = 0;
	static int retry_cnt = 0;

	buf_size = htonl(buf_size);
	memcpy(buf, &buf_size, SIZE_OF_BUF_SIZE);
	memcpy(&buf[SIZE_OF_BUF_SIZE], fname, fname_len);

	send_buf(buf, fname_len + SIZE_OF_BUF_SIZE, server, FNAME, 0, packet);

	if ((return_val = process_select(server, &retry_cnt, START_STATE, FILE_OK, DONE)) == FILE_OK) {
		recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);

		if (recv_check == CRC_ERR) {
			return_val = START_STATE;
		}
		else if (flag == FNAME_BAD) {
			printf("File %s not found\n", fname);
			return_val = DONE;
		}
	}

	return return_val;
}

STATE recv_data(int32_t output_file, Connection *server) {
	int32_t seq_num = 0;
	uint8_t flag = 0;
	int32_t data_len = 0;
	uint8_t data_buf[MAX_LEN];
	uint8_t packet[MAX_LEN];
	static int32_t expected_seq_num = START_SEQ_NUM;

	if (select_call(server->sk_num, LONG_TIME, 0, NOT_NULL) == 0) {
		printf("timeout after 10 secs, server gone\n");
		return DONE;
	}

	data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);

	if (data_len == CRC_ERR) {
		return RECV_DATA;
	}

	if (flag == END_OF_FILE) {
		send_buf(packet, 1, server, EOF_ACK, 0, packet);
		printf("File done\n");
		return DONE;
	}

	send_buf(packet, 1, server, ACK, 0, packet);

	if (seq_num == expected_seq_num) {
		expected_seq_num++;
		write(output_file, &data_buf, data_len);
	}

	return RECV_DATA;
}

STATE file_ok(int *output_file_fd, char *output_file_name) {
	STATE return_val = DONE;

	if ((*output_file_fd = open(output_file_name, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) {
		perror("file open error");
		return_val = DONE;
	}
	else {
		return_val = RECV_DATA;
	}

	return return_val;
}

void parse_args(int argc, char **argv) {
	int buffer_size = 0;
	char *ptr;
	float error_percent;

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