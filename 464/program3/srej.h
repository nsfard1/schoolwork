#ifndef __SREJ_H__
#define __SREJ_H__

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
#include "cpe464.h"

#define MAX_LEN 1500
#define SIZE_OF_BUF_SIZE 4
#define START_SEQ_NUM 2
#define MAX_TRIES 10
#define LONG_TIME 10
#define SHORT_TIME 1
#define NO_STATUS 0
#define SENT 1
#define RECVD 2
#define LOST 3
#define NEED_TO_FLUSH 4
#define FLUSHED 5

#pragma pack(1)

typedef struct Win Window;

struct Win {
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint32_t seq_num;
	uint32_t packet_len;
	uint8_t status;
	uint8_t last;
};

typedef struct header Header;

struct header {
	uint32_t seq_num;
	uint16_t checksum;
	uint8_t flag;
};

enum FLAG {
	SETUP = 1,
	SETUP_RESPONSE = 2,
	DATA = 3,
	UNUSED = 4,
	RR = 5,
	SREJ = 6,
	FNAME = 7,
	FNAME_OK = 8,
	FNAME_BAD = 9,
	ACK = 10,
	END_OF_FILE = 11,
	EOF_ACK = 12,
	READY = 13,
	CRC_ERR = -1
};

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int create_header(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int32_t recv_buf(uint8_t *buf, int32_t len, int32_t recv_sk_num, Connection *connection, uint8_t *flag, int32_t *seq_num);
int retrieve_header(char *data_buf, int recv_len, uint8_t *flag, int32_t *seq_num);
int process_select(Connection *client, int *retry_count, int select_to_state, int data_rdy_state, int done_state);

#endif