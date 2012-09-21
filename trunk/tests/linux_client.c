#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define toRST 0x5453523E
#define frRST 0x5453523C
#define toCMD 0x444D433E
#define frCMD 0x444D433C
#define toCFG 0x4746433E
#define frCFG 0x4746433C

#define MAXGEN 4

typedef enum {
        RST, RSTWAIT, 
	CFG, CFGWAIT,
	TX, RX
} STATES;

typedef enum {
        TXNONE, TXRST, TXCFG, TXCMD
} TX_STATES;


int main (int argc, char *argv[])
{
	int                 udpSocket;
	int                 local_port;
	int                 status;
	int                 size;
	socklen_t           clientLength;
	int                 server_port;
	char                *server_ip;
	struct sockaddr_in  serverName;
	struct sockaddr_in  clientName;
	
	int32_t	            buf[16] = {0};

	int ret = 0;
	int i, x = 0;

	static int state = RST;
	static int tx_state = TXNONE;



	if (argc!=4) {
		fprintf(stderr, "Usage: %s <local-port> <server-ip> <server-port>\n",
			argv[0]);
		exit(1);
	}

	local_port  = atoi(argv[1]);
	server_ip   = argv[2];
	server_port = atoi(argv[3]);

	udpSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSocket<0) {
		perror("socket()");
		exit(1);
	}

	memset(&serverName, 0, sizeof(serverName));
	serverName.sin_family = AF_INET;
	serverName.sin_addr.s_addr = htonl(INADDR_ANY);
	serverName.sin_port = htons(local_port);
	status = bind(udpSocket, (struct sockaddr *) &serverName,
		  sizeof(serverName));
	if (status<0) {
		perror("bind()");
		exit(1);
	}


	memset(&clientName, 0, sizeof(clientName));
	clientName.sin_family = AF_INET;
	inet_aton(server_ip, &clientName.sin_addr);
	clientName.sin_port = htons(server_port);
	clientLength = sizeof(clientName);

	for (;;) {
		size = 0;
		switch (state) 
		{
			case RST:
				perror("reset");
				tx_state = TXRST;
				//ready = false;
				state = RSTWAIT;
				break;
			case RSTWAIT:
				perror("resetwait");
				tx_state = TXNONE;
				state = RSTWAIT;
				size = recvfrom(udpSocket, buf, 4, 0,
						(struct sockaddr *)&clientName, &clientLength);
				if (size >= 4) {
					if (buf[0] == frRST) {	// "<RST"
						state = CFG;
					} else {
						state = RST;
						perror("reset error");
					}
				} else {
					state = RST;
					perror("reset error");
				}
				break;
			case CFG:
				perror("cfg");
				tx_state = TXCFG;
				//i = check_params(period);
				state = CFGWAIT;
				break;
			case CFGWAIT:
				perror("cfgwait");
				tx_state = TXNONE;
				state = CFGWAIT;
				size = recvfrom(udpSocket, buf, 4, 0,
						(struct sockaddr *)&clientName, &clientLength);
				if (size >= 4) {
					if (buf[0] == frCFG) {	// "<RST"
						state = TX;
						//ready = true;
						perror("txrx");
					} else {
						state = RST;
						perror("CFGWAIT error");
					}
				} else {
					state = RST;
					perror("CFGWAIT error");
				}
				break;
			case TX:
				tx_state = TXCMD;
				state = RX;
				break;
			case RX:
				tx_state = TXNONE;
				state = RX;
				size = recvfrom(udpSocket, buf, 4, 0,
						(struct sockaddr *)&clientName, &clientLength);
				if (size >= 4) {
					if (buf[0] == frCMD) {	// "<RST"
						state = TX;
						//ready = true;
					} else {
						state = RST;
						perror("RX error");
					}
				} else {
					state = RST;
					perror("RX error");
				}
				//if (check_params(period)) 
				//	state = CFG;
				break;
			default:
				tx_state = TXNONE;
				state = RST;
				//ready = false;
		}
		
		ret = 0;
		switch (tx_state)
		{
			case TXRST:
				buf[0] = toRST;
				ret = sendto(udpSocket, buf, 4, 0,
					(struct sockaddr *)&clientName, clientLength);
				break;
			case TXCFG:
				buf[0] = toCFG;
				for (i=1; i<=(MAXGEN*3); i++) {
					buf[i] = 1;
				}
				buf[i] = 93;
				ret = sendto(udpSocket, buf, (MAXGEN*3+2)*4, 0,
					(struct sockaddr *)&clientName, clientLength);
				break;
			case TXCMD:
				buf[0] = toCMD;
				if (x) {
					for (i=1; i<=(MAXGEN); i++) {
						buf[i] = 1 << (17-i);
					}
					x = 0;
				} else {
					for (i=1; i<=(MAXGEN); i++) {
						buf[i] = 1 << (12+i);
					}
					x = 1;
				}
				ret = sendto(udpSocket, buf, (MAXGEN+1)*4, 0,
					(struct sockaddr *)&clientName, clientLength);
				break;
			case TXNONE:
				break;
		}

		if(ret < 0)
			perror("Error: sendto()");

    }

    return 0;
}
