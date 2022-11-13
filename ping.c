#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<signal.h>
#include<unistd.h>
#include<sys/time.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include<arpa/inet.h>

void send_msg();
void sig_alrm(int signo);
void handlePing();
void tv_sub(struct timeval *out, struct timeval *in);
unsigned short cksum_in(unsigned short *addr, int len);

int sd, pid, salen, nsent = 0;
struct sockaddr_in sasend;
struct timeval tvrecv, *tvsend;
int main(int argc, char* argv[]){
	if(argc != 2){
		printf("useage : ping domain_name\n");
		exit(-1);
	}

	bzero((char*)&sasend, sizeof(sasend));
	sasend.sin_family = AF_INET;
	sasend.sin_addr.s_addr = inet_addr(argv[1]);
	salen = sizeof(sasend);
	pid = getpid();

	handlePing();
}

void handlePing(){
	int len, hlen, icmplen;
	struct timeval tval;
	char rbuf[1500];

	fd_set readfd;
	struct iphdr *iph;
	struct icmp *icmp;

	double rtt;

	signal(SIGALRM, sig_alrm);

	if((sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
		printf("socket open error\n");
		exit(-1);
	}

	sig_alrm(SIGALRM);

	for(;;){
		if((len = recvfrom(sd, rbuf, sizeof(rbuf), 0, NULL, NULL)) < 0){
			printf("read error\n");
			exit(-1);
		}
		iph = (struct iphdr*)rbuf;
		hlen = iph->ihl * 4;

		if(iph->protocol != IPPROTO_ICMP){
			return;
		}

		if(iph->saddr == sasend.sin_addr.s_addr){
			icmp = (struct icmp*)(rbuf + hlen);
			icmplen = len - hlen;

			if(icmp->icmp_type == ICMP_ECHOREPLY){
				if(icmp->icmp_id != pid){
					return;
				}
				gettimeofday(&tvrecv, NULL);
				tvsend = (struct timeval*)icmp->icmp_data;
				tv_sub(&tvrecv, tvsend);

				rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0;
				printf("%d byte form **: seq = %u, ttl = %d, rtt = %.3fms\n", icmplen, icmp->icmp_seq, iph->ttl, rtt);
			}
		}
	}
}

void sig_alrm(int signo){
	send_msg();
	alarm(1);
	return;
}

void send_msg(){
	int len;
	struct icmp *icmp;
	char sendbuf[1500];
	int datalen = 56;

	icmp = (struct icmp*)sendbuf;

	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = pid;
	icmp->icmp_seq = nsent++;
	memset(icmp->icmp_data, 0xa5, datalen);

	gettimeofday((struct timeval*)icmp->icmp_data, NULL);

	len = 8 + datalen;
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = cksum_in((unsigned short*)icmp, len);

	sendto(sd, sendbuf, len, 0, (struct sockaddr*)&sasend, salen);
}

unsigned short cksum_in(unsigned short *addr, int len){
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	while(nleft > 1){
		sum += *w++;
		nleft -= 2;
	}
	if(nleft == 1){
		*(unsigned char*)(&answer) = *(unsigned char*)w;
		sum += answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}

void tv_sub(struct timeval *out, struct timeval *in){
	if ( (out->tv_usec -= in->tv_usec) < 0) {	/* out -= in */
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
