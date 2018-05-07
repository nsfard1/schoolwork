#include <stdio.h>
#include <stdlib.h>
#include <pcap/pcap.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include "checksum.h"

#define ETHER_HDR_LEN 14
#define ETHER_ADDR_LEN 6
#define ARP_HDR_LEN 28
#define IP_HDR_LEN 20
#define ICMP_HDR_LEN 8
#define UDP_HDR_LEN 8
#define TCP_HDR_LEN 20
#define PSEUDO_HDR_LEN 12
#define ARP 0x0806
#define IP 0x0800
#define HW_ADDR_LEN 6
#define MAC_ADDR_LEN 6
#define IP_ADDR_LEN 4
#define UDP 17
#define TCP 6
#define ICMP 1
#define DNS 0x35
#define HTTP 0x50
#define TELNET 0x17
#define FTP 0x15
#define SMTP 0x19
#define POP3 0x6E
#define FIN 0x01
#define SYN 0x02
#define RST 0x04
#define PSH 0x08
#define ACK 0x10
#define URG 0x20
#define ECE 0x40
#define CWR 0x80
#define ARP_REQ 1
#define ARP_REP 2
#define ICMP_REQ 8
#define ICMP_REP 0

typedef struct ether_hdr {
	u_char dst_addr[ETHER_ADDR_LEN];
	u_char src_addr[ETHER_ADDR_LEN];
	uint16_t type;
} __attribute__((packed)) ether_hdr;

typedef struct arp_hdr {
	u_char hw_addr[HW_ADDR_LEN];
	uint16_t opcode;
	u_char sender_mac_addr[MAC_ADDR_LEN];
	u_char sender_ip_addr[IP_ADDR_LEN];
	u_char target_mac_addr[MAC_ADDR_LEN];
	u_char target_ip_addr[IP_ADDR_LEN];
} __attribute__((packed)) arp_hdr;

typedef struct ip_hdr {
	u_char v_hl;
	u_char tos;
	uint16_t length;
	uint16_t id;
	uint16_t flag_frag_off;
	u_char ttl;
	u_char protocol;
	uint16_t checksum;
	u_char src_addr[IP_ADDR_LEN];
	u_char dst_addr[IP_ADDR_LEN];
} __attribute__((packed)) ip_hdr;

typedef struct udp_hdr {
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t length;
	uint16_t checksum;
} __attribute__((packed)) udp_hdr;

typedef struct tcp_hdr {
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t seq_num;
	uint32_t ack_num;
	u_char off_rsvd;
	u_char flags;
	uint16_t win_size;
	uint16_t checksum;
	uint16_t urg_ptr;
} __attribute__((packed)) tcp_hdr;

typedef struct icmp_hdr {
	u_char type;
	u_char code;
	uint16_t checksum;
	uint32_t rest;
} __attribute__((packed)) icmp_hdr;

typedef struct pseudo_hdr {
	u_char src_addr[IP_ADDR_LEN];
	u_char dst_addr[IP_ADDR_LEN];
	u_char reserved;
	u_char protocol;
	uint16_t tcp_len;
} __attribute__((packed)) pseudo_hdr;

void report_error() {
	fprintf(stderr, "Argument Error! Exitting...\n");
	exit(1);
}

void print_udp_info(const u_char *packet, int off) {
	udp_hdr *udp = malloc(sizeof(udp_hdr));

	memcpy(udp, packet + off, UDP_HDR_LEN);
	udp->src_port = ntohs(udp->src_port);
	udp->dst_port = ntohs(udp->dst_port);
	udp->length = ntohs(udp->length);
	udp->checksum = ntohs(udp->checksum);

	printf("\tUDP Header\n");

	printf("\t\tSource Port:  ");
	switch (udp->src_port) {
		case DNS: /* DNS */
			printf("DNS\n");
			break;
		case HTTP: /* HTTP */
			printf("HTTP\n");
			break;
		case TELNET: /* Telnet */
			printf("Telnet\n");
			break;
		case FTP: /* FTP */
			printf("FTP\n");
			break;
		case SMTP: /* SMTP */
			printf("SMTP\n");
			break;
		case POP3: /* POP3 */
			printf("POP3\n");
			break;
		default:
			printf("%d\n", udp->src_port);
			break;
	}

	printf("\t\tDest Port:  ");
	switch (udp->dst_port) {
		case DNS: /* DNS */
			printf("DNS\n");
			break;
		case HTTP: /* HTTP */
			printf("HTTP\n");
			break;
		case TELNET: /* Telnet */
			printf("Telnet\n");
			break;
		case FTP: /* FTP */
			printf("FTP\n");
			break;
		case SMTP: /* SMTP */
			printf("SMTP\n");
			break;
		case POP3: /* POP3 */
			printf("POP3\n");
			break;
		default:
			printf("%d\n", udp->dst_port);
			break;
	}

	printf("\n");
}

void print_tcp_info(const u_char *packet, int off, ip_hdr *ip) {
	uint16_t tcp_len = ip->length - 4 * (ip->v_hl & 0x0F);
	u_char *chksum_pkt = malloc(PSEUDO_HDR_LEN + tcp_len);
	pseudo_hdr *pseudo = malloc(sizeof(pseudo_hdr));
	tcp_hdr *tcp = malloc(sizeof(tcp_hdr));

	memcpy(tcp, packet + off, TCP_HDR_LEN);
	tcp->src_port = ntohs(tcp->src_port);
	tcp->dst_port = ntohs(tcp->dst_port);
	tcp->seq_num = ntohl(tcp->seq_num);
	tcp->ack_num = ntohl(tcp->ack_num);
	tcp->win_size = ntohs(tcp->win_size);
	tcp->checksum = ntohs(tcp->checksum);
	tcp->urg_ptr = ntohs(tcp->urg_ptr);

	memcpy(pseudo->src_addr, ip->src_addr, IP_ADDR_LEN);
	memcpy(pseudo->dst_addr, ip->dst_addr, IP_ADDR_LEN);
	pseudo->protocol = ip->protocol;
	pseudo->tcp_len = ntohs(tcp_len);
	pseudo->reserved = 0;

	printf("\tTCP Header\n");

	printf("\t\tSource Port:  ");
	switch(tcp->src_port) {
		case DNS: /* DNS */
			printf("DNS\n");
			break;
		case HTTP: /* HTTP */
			printf("HTTP\n");
			break;
		case TELNET: /* Telnet */
			printf("Telnet\n");
			break;
		case FTP: /* FTP */
			printf("FTP\n");
			break;
		case SMTP: /* SMTP */
			printf("SMTP\n");
			break;
		case POP3: /* POP3 */
			printf("POP3\n");
			break;
		default:
			printf("%d\n", tcp->src_port);
			break;
	}

	printf("\t\tDest Port:  ");
	switch(tcp->dst_port) {
		case DNS: /* DNS */
			printf("DNS\n");
			break;
		case HTTP: /* HTTP */
			printf("HTTP\n");
			break;
		case TELNET: /* Telnet */
			printf("Telnet\n");
			break;
		case FTP: /* FTP */
			printf("FTP\n");
			break;
		case SMTP: /* SMTP */
			printf("SMTP\n");
			break;
		case POP3: /* POP3 */
			printf("POP3\n");
			break;
		default:
			printf("%d\n", tcp->dst_port);
			break;
	}

	printf("\t\tSequence Number: %u\n", tcp->seq_num);
	printf("\t\tACK Number: %u\n", tcp->ack_num);
	printf("\t\tData Offset (bytes): %d\n", (tcp->off_rsvd >> 4) * 4);
	printf("\t\tSYN Flag: %s\n", tcp->flags & 0x02 ? "Yes" : "No");
	printf("\t\tRST Flag: %s\n", tcp->flags & 0x04 ? "Yes" : "No");
	printf("\t\tFIN Flag: %s\n", tcp->flags & 0x01 ? "Yes" : "No");
	printf("\t\tACK Flag: %s\n", tcp->flags & 0x10 ? "Yes" : "No");
	printf("\t\tWindow Size: %d\n", tcp->win_size);
	printf("\t\tChecksum: ");

	memcpy(chksum_pkt, pseudo, PSEUDO_HDR_LEN);
	memcpy(chksum_pkt + PSEUDO_HDR_LEN, packet + off, tcp_len);

	if (!in_cksum((uint16_t *)chksum_pkt, PSEUDO_HDR_LEN + tcp_len)) {
		printf("Correct ");
	}
	else {
		printf("Incorrect ");
	}

	printf("(0x%04x)\n\n", tcp->checksum);
}

void print_icmp_info(const u_char *packet, int off) {
	icmp_hdr *icmp = malloc(sizeof(icmp_hdr));

	memcpy(icmp, packet + off, ICMP_HDR_LEN);
	icmp->checksum = ntohs(icmp->checksum);

	printf("\tICMP Header\n");
	if (icmp->type == ICMP_REQ) {
		printf("\t\tType: Request\n\n");
	}
	else if (icmp->type == ICMP_REP) {
		printf("\t\tType: Reply\n\n");
	}
	else {
		printf("\t\tType: %d\n\n", icmp->type);
	}
}

void print_ip_info(const u_char *packet) {
	int i;
	ip_hdr *ip = malloc(sizeof(ip_hdr));
	u_char *flipped_pkt = malloc(IP_HDR_LEN);

	memcpy(ip, packet + ETHER_HDR_LEN, IP_HDR_LEN);
	ip->length = ntohs(ip->length);
	ip->id = ntohs(ip->id);
	ip->flag_frag_off = ntohs(ip->flag_frag_off);
	ip->checksum = ntohs(ip->checksum);

	printf("\tIP Header\n");
	printf("\t\tIP Version: %d\n", ip->v_hl >> 4);
	printf("\t\tHeader Len (bytes): %d\n", (ip->v_hl & 0x0F) * 4);
	printf("\t\tTOS subfields:\n");
	printf("\t\t   Diffserv bits: %d\n", ip->tos >> 2);
	printf("\t\t   ECN bits: %d\n", ip->tos & 0x03);
	printf("\t\tTTL: %d\n", ip->ttl);

	if (ip->protocol == ICMP) {
		printf("\t\tProtocol: ICMP\n");
	}
	else if (ip->protocol == UDP) {
		printf("\t\tProtocol: UDP\n");
	}
	else if (ip->protocol == TCP) {
		printf("\t\tProtocol: TCP\n");
	}
	else {
		printf("\t\tProtocol: Unknown\n");
	}

	printf("\t\tChecksum: ");

	for (i=0; i<IP_HDR_LEN; i++) {
		*(flipped_pkt + i) = i % 2 ? *(packet + ETHER_HDR_LEN + i - 1) : *(packet + ETHER_HDR_LEN + i + 1);
	}

	if (!in_cksum((uint16_t *)flipped_pkt, IP_HDR_LEN)) {
		printf("Correct ");
	}
	else {
		printf("Incorrect ");
	}

	printf("(0x%04x)\n", ip->checksum);
	printf("\t\tSender IP: %d.%d.%d.%d\n", ip->src_addr[0], ip->src_addr[1], ip->src_addr[2], ip->src_addr[3]);
	printf("\t\tDest IP: %d.%d.%d.%d\n\n", ip->dst_addr[0], ip->dst_addr[1], ip->dst_addr[2], ip->dst_addr[3]);

	if (ip->protocol == ICMP) {
		print_icmp_info(packet, ETHER_HDR_LEN + (ip->v_hl & 0x0F) * 4);
	}
	else if (ip->protocol == UDP) {
		print_udp_info(packet, ETHER_HDR_LEN + (ip->v_hl & 0x0F) * 4);
	}
	else if (ip->protocol == TCP) {
		print_tcp_info(packet, ETHER_HDR_LEN + (ip->v_hl & 0x0F) * 4, ip);
	}
}

void print_arp_info(const u_char *packet) {
	arp_hdr *arp = malloc(sizeof(arp_hdr));

	memcpy(arp, packet + ETHER_HDR_LEN, ARP_HDR_LEN);
	arp->opcode = ntohs(arp->opcode);

	printf("\tARP header\n");
	if (arp->opcode == ARP_REQ) {
		printf("\t\tOpcode: Request\n");
	}
	else if (arp->opcode == ARP_REP) {
		printf("\t\tOpcode: Reply\n");
	}
	else {
		printf("\t\tOpcode: Unknown\n");
	}

	printf("\t\tSender MAC: %x:%x:%x:%x:%x:%x\n", arp->sender_mac_addr[0], arp->sender_mac_addr[1], arp->sender_mac_addr[2], arp->sender_mac_addr[3], arp->sender_mac_addr[4], arp->sender_mac_addr[5]);
	printf("\t\tSender IP: %d.%d.%d.%d\n", arp->sender_ip_addr[0], arp->sender_ip_addr[1], arp->sender_ip_addr[2], arp->sender_ip_addr[3]);
	printf("\t\tTarget MAC: %x:%x:%x:%x:%x:%x\n", arp->target_mac_addr[0], arp->target_mac_addr[1], arp->target_mac_addr[2], arp->target_mac_addr[3], arp->target_mac_addr[4], arp->target_mac_addr[5]);
	printf("\t\tTarget IP: %d.%d.%d.%d\n\n", arp->target_ip_addr[0], arp->target_ip_addr[1], arp->target_ip_addr[2], arp->target_ip_addr[3]);
}

void print_eth_info(const u_char *packet) {
	ether_hdr *eth = malloc(sizeof(ether_hdr));

	memcpy(eth, packet, ETHER_HDR_LEN);
	eth->type = ntohs(eth->type);

	printf("\tEthernet Header\n");
	printf("\t\tDest MAC: %x:%x:%x:%x:%x:%x\n", eth->dst_addr[0], eth->dst_addr[1], eth->dst_addr[2], eth->dst_addr[3], eth->dst_addr[4], eth->dst_addr[5]);
	printf("\t\tSource MAC: %x:%x:%x:%x:%x:%x\n", eth->src_addr[0], eth->src_addr[1], eth->src_addr[2], eth->src_addr[3], eth->src_addr[4], eth->src_addr[5]);
	if (eth->type == ARP) {
		printf("\t\tType: ARP\n\n");
		print_arp_info(packet);
	}
	else if (eth->type == IP) {
		printf("\t\tType: IP\n\n");
		print_ip_info(packet);
	}
	else {
		printf("\t\tType: %d\n\n", eth->type);
	}
}

int main(int argc, char **argv) {
	pcap_t *handle;
	char errbuff[PCAP_ERRBUF_SIZE];
	struct pcap_pkthdr header;
	const u_char *packet;
	int count = 0;

	// parse arguments
	if (argc != 2) {
		report_error();
	}

	// open pcap file
	handle = pcap_open_offline(*(++argv), errbuff);
	if (!handle) {
		report_error();
	}

	// read all packets
	while ((packet = pcap_next(handle, &header))) {
		printf("\nPacket number: %d  Packet Len: %d\n\n", ++count, header.len);
		// read ethernet header
		print_eth_info(packet);
	}

	return 0;
}