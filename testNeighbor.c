#include <stdio.h>          
#include <stdlib.h>   
#include <string.h>      
#include <unistd.h>          
#include <signal.h> 
#include <errno.h> 
#include <sys/time.h>     
#include <netinet/ip_icmp.h>          
#include <arpa/inet.h>  
 

#define BUF_SIZE 256
#define NUM_SEND 5

static int num_send = NUM_SEND;

int sockfd;
int datalen=56;
struct sockaddr_in dest_addr, from_addr;
pid_t pid;

int nsend=0, nreceived=0;

char sendBuf[BUF_SIZE];
char recvBuf[BUF_SIZE];

struct timeval tvsend, tvrecv;

double min_rtt=500, max_rtt=-1;

double rtt_total, avg_rtt;

u_int16_t cal_chksum(u_int16_t *addr, int len);

void print_stat();

int main(int argc, char *argv[]){

	int ch;

	struct timeval timeout;		

	//int fdmask;
	fd_set readfds;	

	u_int8_t ttl = 1;	

	if(argc < 2){
		printf("usage: testNeighbor IPAddress\n");
		exit(1);
	}

	while((ch = getopt(argc, argv, "c:")) != EOF){
		switch(ch){
			case 'c':
				num_send=atoi(optarg);
				break;			
		}
	}	
	
	if((sockfd=socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
		perror("socket error");
		exit(-1);
	}	

	if(setsockopt(sockfd, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) < 0){
		perror("setting ttl error\n");
		exit(-1);
	}
	
	pid=getpid();

	memset(&dest_addr, 0, sizeof(struct sockaddr_in));

	dest_addr.sin_family=AF_INET;	

	if(!inet_aton(argv[argc-1], &dest_addr.sin_addr)){
		printf("IPAddr invalid\n");
		exit(-1);			
		
	}

	socklen_t sockaddr_len=sizeof(struct sockaddr_in);
	
	int reclen;
	int pktsize;			

	struct icmp *icmp;
	struct ip *ip;
	
	int iphl,icmplen;
		
	double rtt;
	
	//signal(SIGALRM, print_stat);
	//alarm(numsend*1);	
	
	while(nsend < num_send){		
		
		memset(sendBuf, '\0', BUF_SIZE);
		memset(recvBuf, '\0', BUF_SIZE);

		icmp=(struct icmp*)sendBuf;
		icmp->icmp_type = ICMP_ECHO;
		icmp->icmp_code = 0;
		icmp->icmp_cksum = 0;
		icmp->icmp_seq = ++nsend;		
		icmp->icmp_id = pid;

		pktsize=datalen + 8;						

		icmp->icmp_cksum=cal_chksum((u_int16_t *)icmp, pktsize);

		gettimeofday(&tvsend, NULL);				

		if(sendto(sockfd, sendBuf, pktsize, 0, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_in)) < 0){
			perror("sendto error");
			exit(-1);
			//nsend--;					
			//continue;
		}

		timeout.tv_sec=0;
		timeout.tv_usec=100000;
		//fdmask=1 << sockfd;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		if (select(sockfd + 1, &readfds/*(fd_set *)&fdmask*/, (fd_set *)NULL, (fd_set *)NULL, &timeout) < 1)
			continue;		
			
		if((reclen=recvfrom(sockfd, recvBuf, BUF_SIZE, 0, (struct sockaddr*)&from_addr, (socklen_t* )&sockaddr_len)) < 0){
			perror("recv error");
			//continue;
			exit(-1);
		}			
	
		gettimeofday(&tvrecv, NULL);

		ip=(struct ip*)recvBuf;	
		iphl=ip->ip_hl << 2;

		icmp=(struct icmp*)(recvBuf+iphl);

		icmplen=reclen-iphl;

		if(icmplen < 8){
			printf("ICMP packet too short: %d bytes\n", icmplen);
			//continue;
			exit(1);
		}		

		if((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid)){
		
			nreceived++;
		
			rtt=(tvrecv.tv_sec*1000000+tvrecv.tv_usec-(tvsend.tv_sec*1000000+tvsend.tv_usec))/(double)1000;		

			rtt_total += rtt;	
			
			if(rtt < min_rtt)
				min_rtt=rtt;
			if(rtt > max_rtt)
				max_rtt=rtt;	
		
			//printf("%d byte from %s: icmp_seq=%u rtt= %.3fms\n", icmplen, inet_ntoa(from_addr.sin_addr), icmp->icmp_seq, rtt);
		}		
	}
	
	print_stat();	
}


u_int16_t cal_chksum(u_int16_t *addr, int len)  
{  
    int nleft = len;  
    int sum = 0;  
    unsigned short *w = addr;  
    unsigned short answer = 0;    
     
    while (nleft > 1)  
    {  
        sum += *w++;  
        nleft -= 2;  
    }   
    
    if (nleft == 1)  
    {  
        *(unsigned char *)(&answer) = *(unsigned char *)w;  
        sum += answer;  
    }  
  
    sum = (sum>>16) + (sum&0xffff);  
    sum += (sum>>16);  
    answer = ~sum;  
  
    return answer;  
}

void print_stat(){
	
	//printf("\n%d packets transmitted, %d received, %.2f%% packets lost\n", nsend, nreceived, (float)(nsend-nreceived)/(float)nsend*100);
	printf("%.2f%%",(float)(nsend-nreceived)/(float)nsend*100);
	
	if(nreceived){
		avg_rtt=rtt_total/(float)nreceived;
		//printf("rtt: min/ avg/ max = %.3f/ %.3f/ %.3f ms\n",min_rtt, avg_rtt, max_rtt);
		printf("  %.3f",avg_rtt);
	}
	
	close(sockfd);
	exit(0);
}
































