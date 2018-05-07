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


int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
	int32_t sent_len = 0;
	int32_t sending_len = 0;

	// set up the packet
	if (len > 0) {
		memcpy(&packet[sizeof(Header)], buf, len);
	}

	sending_len = create_header(len, flag, seq_num, packet);

	sent_len = safe_send(packet, sending_len, connection);

	return sent_len;
}

int create_header(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
	Header *hdr = (Header *) packet;
	uint16_t checksum = 0;

	seq_num = htonl(seq_num);
	memcpy(&(hdr->seq_num), &seq_num, sizeof(seq_num));
	hdr->flag = flag;

	memset(&(hdr->checksum), 0, sizeof(checksum));
	checksum = in_cksum((unsigned short *) packet, len + sizeof(Header));
	memcpy(&(hdr->checksum), &checksum, sizeof(checksum));

	return len + sizeof(Header);
}

int32_t recv_buf(uint8_t *buf, int32_t len, int32_t recv_sk_num, Connection *connection, uint8_t *flag, int32_t *seq_num) {
	char data_buf[MAX_LEN];
	int32_t recv_len = 0;
	int32_t data_len = 0;

	recv_len = safe_recv(recv_sk_num, data_buf, len, connection);
	data_len = retrieve_header(data_buf, recv_len, flag, seq_num);

	if (data_len > 0) {
		memcpy(buf, &data_buf[sizeof(Header)], data_len);
	}

	return data_len;
}

int retrieve_header(char *data_buf, int recv_len, uint8_t *flag, int32_t *seq_num) {
	Header *hdr = (Header *) data_buf;
	int return_val = 0;

	if (in_cksum((unsigned short *) data_buf, recv_len) != 0) {
		return_val = CRC_ERR;
	}
	else {
		*flag = hdr->flag;
		memcpy(seq_num, &(hdr->seq_num), sizeof(hdr->seq_num));
		*seq_num = ntohl(*seq_num);

		return_val = recv_len - sizeof(Header);
	}

	return return_val;
}

int process_select(Connection *client, int *retry_count, int select_to_state, int data_rdy_state, int done_state) {
	int return_val = data_rdy_state;

	(*retry_count)++;
	if (*retry_count > MAX_TRIES) {
		printf("Sent data %d times, no ACK, client is probably gone - so I'm terminating.\n", MAX_TRIES);
		return_val = done_state;
	}
	else {
		if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
			*retry_count = 0;
			return_val = data_rdy_state;
		}
		else {
			// no data ready
			return_val = select_to_state;
		}
	}

	return return_val;
}