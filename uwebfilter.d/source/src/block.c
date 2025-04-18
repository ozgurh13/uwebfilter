#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <sys/socket.h>

#include "block.h"


static uint16_t
checksum(const uint16_t *buffer, int size)
{
	unsigned long cksum = 0;
	while (size > 1) {
		cksum += *buffer++;
		size -= sizeof(uint16_t);
	}
	if (size > 0) {
		cksum += (uint8_t) *buffer;
	}

	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >>16);
	return (uint16_t) (~cksum);
}




struct dnshdr {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
};

struct dns_question {
	uint16_t qtype;
	uint16_t qclass;
};

struct dns_answer {
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t length;
	uint32_t ipaddr;
} __attribute__((packed)) ;



void
dns_answer_send(const uint8_t *packet)
{
	uint8_t buffer[512] = {0};

	const struct iphdr *ip = (const struct iphdr*) packet;
	const struct udphdr *udp = (const struct udphdr*) (packet + ip->ihl * 4);
	const struct dnshdr *dns = (const struct dnshdr*) (packet + ip->ihl * 4 + sizeof(struct udphdr));

	const uint8_t *query = (const uint8_t*)
		(packet + ip->ihl * 4 + sizeof(struct udphdr) + sizeof(struct dnshdr));
	size_t query_length = strlen((const char *) query) + 1;

	/*  DNS payload  */
	uint8_t *ptr = buffer
		+ sizeof(struct iphdr)
		+ sizeof(struct udphdr)
		+ sizeof(struct dnshdr);

	/*  DNS query  */
	memcpy(ptr, query, query_length + sizeof(struct dns_question));
	ptr += query_length + sizeof(struct dns_question);

	/*  DNS answer  */
	memcpy(ptr, query, query_length);
	ptr += query_length;

	struct dns_answer *a = (struct dns_answer *) ptr;
	a->type = htons(0x0001);    /*  TYPE A      */
	a->class = htons(0x0001);   /*  CLASS IN    */
	a->ttl = htonl(10);         /*  10 seconds  */
	a->length = htons(4);
	a->ipaddr = inet_addr("127.0.0.1");
	ptr += sizeof(struct dns_answer);

	/*  IP header  */
	struct iphdr *iphdr = (struct iphdr *)buffer;
	iphdr->ihl = 5;
	iphdr->version = 4;
	iphdr->tos = 0;
	iphdr->tot_len = htons(ptr - buffer);
	iphdr->id = 0;
	iphdr->frag_off = 0;
	iphdr->ttl = 64;
	iphdr->protocol = IPPROTO_UDP;
	iphdr->saddr = ip->daddr;
	iphdr->daddr = ip->saddr;
	iphdr->check = checksum((const uint16_t *) iphdr, sizeof(struct iphdr));

	/*  UDP header  */
	struct udphdr *udphdr = (struct udphdr *) (buffer + sizeof(struct iphdr));
	udphdr->uh_sport = udp->uh_dport;
	udphdr->uh_dport = udp->uh_sport;
	udphdr->uh_ulen = htons(
		sizeof(struct udphdr) +
		sizeof(struct dnshdr) +
		query_length + sizeof(struct dns_question) +  /*  question  */
		query_length + sizeof(struct dns_answer)      /*  answer    */
	);
	udphdr->uh_sum = 0;

	/*  DNS response  */
	struct dnshdr *dnshdr = (struct dnshdr *)
		(buffer + sizeof(struct iphdr) + sizeof(struct udphdr));
	dnshdr->id = dns->id;
	dnshdr->flags = htons(0x8180);
	dnshdr->qdcount = htons(1);
	dnshdr->ancount = htons(1);
	dnshdr->nscount = 0;
	dnshdr->arcount = 0;

	struct sockaddr_in sin = {0};
	sin.sin_family = AF_INET;
	sin.sin_port = udp->uh_sport;
	sin.sin_addr.s_addr = ip->saddr;

	int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sock < 0) {
		perror("socket");
		return;
	}

	int one = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
		perror("setsockopt");
		close(sock);
		return;
	}

	if (sendto(sock, buffer, ntohs(iphdr->tot_len), 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("sendto");
	}

	close(sock);
}








/*  https://www.binarytides.com/raw-sockets-c-code-linux/  */

struct pseudo_header {
	uint32_t saddr;
	uint32_t daddr;
	uint8_t zero;
	uint8_t protocol;
	uint16_t length;
};

void
tcp_rst_send(const uint8_t* packet)
{
	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct tcphdr *tcphdr = (const struct tcphdr *) (packet + (iphdr->ihl << 2));

	char datagram[64] = {0};
	char pseudogram[64] = {0};

	struct iphdr *iph = (struct iphdr *) datagram;
	struct tcphdr *tcph = (struct tcphdr *) (datagram + sizeof(struct ip));

	struct sockaddr_in sin = {
		.sin_family      = AF_INET,
		.sin_port        = tcphdr->th_sport,
		.sin_addr.s_addr = iphdr->saddr
	};

	/*  ip header  */
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
	iph->id = 0;
	iph->frag_off = 0;
	iph->ttl = 255;
	iph->protocol = IPPROTO_TCP;
	iph->check = 0;               /*  set to 0, will later be computed  */
	iph->saddr = iphdr->daddr;
	iph->daddr = iphdr->saddr;
	iph->check = checksum((unsigned short *) datagram, iph->tot_len);

	/*  tcp header  */
	tcph->source = tcphdr->th_dport;
	tcph->dest = tcphdr->th_sport;
	tcph->seq = tcphdr->ack_seq;
	tcph->ack_seq = tcphdr->seq + 1;
	tcph->doff = 5;
	tcph->fin = 0;
	tcph->syn = 0;
	tcph->rst = 1;
	tcph->psh = 0;
	tcph->ack = 0;
	tcph->urg = 0;
	tcph->window = htons(5840);   /*  maximum allowed window size  */
	tcph->check = 0;              /*  set to 0, will be filled by pseudo header  */
	tcph->urg_ptr = 0;

	/*  tcp checksum  */
	struct pseudo_header psh = {
		.saddr = iphdr->daddr,
		.daddr = iphdr->saddr,
		.zero = 0,
		.protocol = IPPROTO_TCP,
		.length = htons(sizeof(struct tcphdr))
	};

	memcpy(pseudogram, (char*) &psh, sizeof(struct pseudo_header));
	memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
	tcph->check = checksum(
		(unsigned short*) pseudogram,
		sizeof(struct pseudo_header) + sizeof(struct tcphdr)
	);


	const int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock < 0) {
		perror("socket");
		return;
	}

	/*  IP_HDRINCL will tell the kernel that the headers are included in the packet  */
	int one = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
		perror("setsockopt");
		close(sock);
		return;
	}

	if (sendto(sock, datagram, iph->tot_len, 0, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("sendto");
	}

	close(sock);
}
