#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rtnet.h>

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

int main(int argc, char *argv[])
{
	RT_TASK 	    *task;
	RTIME		    start = 0;
	RTIME		    end = 0;
	long long	    acc = 0;
	long		    acc2 = 0;
	int                 udpSocket;
	int		    rsttimeout = 0;
	int                 ret = 0;
	int		    hard_timer_running;
	int                 i, j;
	int                 x = 0, y = 0,z;
	static int	    state = RST;
	static int	    tx_state = TXNONE;
	long	            buf[16] = {0};
	long long 	    timeout = 1000000000L;	// 1 s
	struct sockaddr_in  server_addr;
	struct sockaddr_in  client_addr;
	
	if (argc!=6) {
		fprintf(stderr, "Usage: %s <local-port> <server-ip> <server-port> <delay> <repeats>\n",
			argv[0]);
		exit(1);
	}
	
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	client_addr.sin_family      = AF_INET;
	client_addr.sin_addr.s_addr = INADDR_ANY;
	client_addr.sin_port        = htons(atoi(argv[1]));
	
	server_addr.sin_family = AF_INET;
	inet_aton(argv[2], &server_addr.sin_addr);
	server_addr.sin_port = htons(atoi(argv[3]));

	udpSocket = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
	if (udpSocket < 0) {
		printf("Error opening socket: %d\n", udpSocket);
		return 1;
	}

	// Set timeout for socket in blocking mode
	ret = rt_dev_ioctl(udpSocket, RTNET_RTIOC_TIMEOUT, &timeout);
	if (ret < 0) {
		rt_dev_close(udpSocket);
		printf("Cannot set timeout for socket in blocking mode\n");
		return 1;
	}

	/* Link the Linux process to RTAI. */
	if (!(hard_timer_running = rt_is_hard_timer_running())) {
		start_rt_timer(0);
	}
	task = rt_thread_init(nam2num("RT8P8C"), 1, 0, SCHED_OTHER, 0xFF);
	if (task == NULL) {
		rt_dev_close(udpSocket);
		printf("CANNOT LINK LINUX SIMPLECLIENT PROCESS TO RTAI\n");
		return 1;
	}

	/* Lock allocated memory into RAM. */
	printf("RT-8p8c (user space).\n");
	mlockall(MCL_CURRENT|MCL_FUTURE);

	/* Switch over to hard realtime mode. */
	rt_make_hard_real_time();

	/* Bind socket to local address specified as parameter. */
	ret = rt_dev_bind(udpSocket, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in));

	/* Specify destination address for socket; needed for rt_socket_send(). */
	rt_dev_connect(udpSocket, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));


	for (z=0;z<atoi(argv[5]);z++) {
		
		acc = 0;
		y = 0;
		state = RST;
		tx_state = TXNONE;
		acc2 = 0;
		j = 0;
		
		while (1) {
			ret = 0;
			switch (state) 
			{
				case RST:
					//printf("state: reset\n");
					tx_state = TXRST;
					//ready = false;
					state = RSTWAIT;
					rsttimeout = 0;
					break;
				case RSTWAIT:
					//printf("state: resetwait\n");
					tx_state = TXNONE;
					state = RSTWAIT;
					ret = rt_dev_recv(udpSocket, buf, 4, 0);
					if (ret == -ETIMEDOUT) {
						if (rsttimeout++ > 2)
							state = RST;
						else
							state = RSTWAIT;
					} else if (ret >= 4) {
						if (buf[0] == frRST) {	// "<RST"
							state = CFG;
						} else {
							state = RST;
							printf("reset error %i\n",ret);
						}
					} else {
						state = RST;
						printf("reset wait error %i\n",ret);
					}
					break;
				case CFG:
					//printf("state: cfg\n");
					tx_state = TXCFG;
					//i = check_params(period);
					state = CFGWAIT;
					break;
				case CFGWAIT:
					//printf("state: cfgwait\n");
					tx_state = TXNONE;
					state = CFGWAIT;
					ret = rt_dev_recv(udpSocket, buf, 4, 0);
					if (ret == -ETIMEDOUT) {
						state = CFGWAIT;
					} else if (ret >= 4) {
						if (buf[0] == frCFG) {	// "<CFG"
							state = TX;
							//ready = true;
							//printf("state: txrx\n");
						} else {
							state = CFG;
							printf("CFGWAIT error %i\n",ret);
						}
					} else {
						state = RST;
						printf("CFGWAIT error %i\n",ret);
					}
					break;
				case TX:
					tx_state = TXCMD;
					y++;
					state = RX;
					rsttimeout = 0;
					break;
				case RX:
					tx_state = TXNONE;
					state = RX;
					ret = rt_dev_recv(udpSocket, buf, 24, 0);
					end = rt_get_real_time_ns();
					if (ret == -ETIMEDOUT) {
						if (rsttimeout++ > 2) {
							printf("RX timeout: %i\n",z);
							state = TX;
						} else
							state = RX;
					} else if (ret >= 4) {
						if (buf[0] == frCMD) {	// "<CMD"
							state = TX;
							acc += end - start;
							acc2 += buf[5];
							j ++;
							//ready = true;
						} else {
							state = TX;
							printf("RX error %i\n",ret);
						}
					} else {
						state = RST;
						printf("RX error %i\n",ret);
					}
					//if (check_params(period)) 
					//	state = CFG;
					break;
				default:
					tx_state = TXNONE;
					state = RST;
					//ready = false;
			}
			
			//printf ("%i\n", y);
			if (y >1024)
				break;
			
			ret = 0;
			switch (tx_state)
			{
				case TXRST:
					buf[0] = toRST;
					ret = rt_dev_send(udpSocket, buf, 4, 0);
					break;
				case TXCFG:
					buf[0] = toCFG;
					for (i=1; i<=(MAXGEN*3); i++) {
						buf[i] = 1;
					}
					ret = rt_dev_send(udpSocket, buf, (MAXGEN*3+1)*4, 0);
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
					buf[i] = atoi(argv[4]);
					ret = rt_dev_send(udpSocket, buf, (MAXGEN+2)*4, 0);
					start = rt_get_real_time_ns();
					break;
				case TXNONE:
					break;
			}

			if(ret < 0)
				printf("Send error\n");
		}
//		printf("Period: %i - %llu, %i, %lu\n", z, (acc/j),j,(acc2/j));
	}

	printf("state: stopped\n");
	printf("Period: %i - %llu, %i, %lu\n", z, (acc/j),j,(acc2/j));
	
	/* Switch over to soft realtime mode. */
	rt_make_soft_real_time();

	/* Close socket, must be in soft-mode because socket was created as non-rt. */
	rt_dev_close(udpSocket);

	/* Unlink the Linux process from RTAI. */
	if (!hard_timer_running) {
		stop_rt_timer();
	}
	rt_task_delete(task);
	
	return 0;
}
