// Linux Feedback agent - Ben

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include "cfgparse.h"

#define MAX_BUF 1024
#define FILENAME "feedback.cfg" // configuration file

volatile sig_atomic_t end = 0;

void sig_handler(int signo) {
	if (signo == SIGINT){
    		printf("received SIGINT\n");
		end = 1 ;
	}
}

int read_cpustat(unsigned long long int *fields) {
	 FILE *fp;
	int retval;
	char buffer[MAX_BUF];
	
	fp = fopen("/proc/stat", "r");
	if (!fgets(buffer, MAX_BUF, fp)) {
		perror ("Error");
	}
	
	retval = sscanf (buffer, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
		&fields[0],
		&fields[1],
		&fields[2],
		&fields[3],
		&fields[4],
		&fields[5],  
		&fields[6],
		&fields[7],
		&fields[8],
		&fields[9]);

 	if (retval < 4) /* we need the first 4 values to calculate the the cpu%*/
	{
		fprintf(stderr, "error reading /proc/stat\n");
		return 1;
	} 
	fclose (fp);
	return 0;
}

int cpu_stat(){
	unsigned long long int fields[10], total_tick, total_tick_old, idle, idle_old, del_total_tick, del_idle;
	int cpu_usage = 1, i= 0;
	struct timespec tim1, tim2;
	tim1.tv_sec = 0;
	tim1.tv_nsec = 500000000L;
	
	if (read_cpustat(fields)) {
		return 0;
	}

	//read cpu ticks from all fields and update total
	for (i=0, total_tick = 0; i<10; i++) {
		total_tick += fields[i];
	}

	//third field is cpu idle ticks
	idle = fields[3];

	nanosleep(&tim1, &tim2);
	total_tick_old = total_tick;
	idle_old = idle;
	
	if (read_cpustat(fields)) {
		return 0;
	}	

	//read cpu ticks from all fields and update total again
	for (i=0, total_tick = 0; i<10; i++) {
		total_tick += fields[i];
	}

	//third field is cpu idle ticks
	idle = fields[3];

	del_total_tick = total_tick - total_tick_old;
	del_idle = idle - idle_old;
 	
	cpu_usage = ((del_total_tick - del_idle) / (double) del_total_tick) * 100; 
	
	return cpu_usage;
}


void *drain_timer(void *void_ready){

	printf("startiing timer\n");

	int *ready_up = (int *)void_ready;
	//we only change ready up here
	*ready_up = 1;	
	//*void_ready = 1;
	sleep(30);
	printf("ending timer\n");
	*ready_up = 0;
	//*void_ready = 0 ;

return NULL;

}


int main (){
	int rc, bind_rc;
	int rcc;
	int cpu_usage;
	int cpu_drain;
	int port;
	int ready_up = 0;
	int yes=1;        // for setsockopt() SO_REUSEADDR, below
	struct config configstruct;
	int listenfd = 0, connfd = 0;	
	struct sockaddr_in serv_addr;
	char sendBuff[MAX_BUF];
	time_t ticks;
	pthread_t drain_timer_thread;
	int drain = 1; //0 drain 1 no drain
	
	if (signal(SIGINT, sig_handler) == SIG_ERR)
  		printf("\ncan't catch SIGINT\n");	
//handel sigterm
	//struct sigaction action;
    	//memset(&action, 0, sizeof(struct sigaction));
   	//action.sa_handler = term;
    	//sigaction(SIGTERM, &action, NULL);

	// Read config file
	configstruct = get_config(FILENAME); //values are char
	printf("Port is %s\n", configstruct.port);
	printf("Drain CPU setting is %s\n", configstruct.drain_cpu);

	//change drain and port values to int
	cpu_drain = atoi(configstruct.drain_cpu);  
	port = atoi(configstruct.port);
	
	listenfd = socket(AF_INET, SOCK_STREAM,0);	
	memset(&serv_addr, '0' , sizeof(serv_addr));
	memset(sendBuff, '0', sizeof(sendBuff));

	serv_addr.sin_family = AF_INET;
    	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	serv_addr.sin_port = htons(port);

	 // Resuse socket if it is in TIME_WAIT
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    	bind_rc = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		if (bind_rc != 0 ){
			printf("there was an error binding to the socket. ERROR:%d\n", bind_rc);
			return -1;
		}
    	listen(listenfd, 10);

	while (!end)
	{
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
		cpu_usage = cpu_stat();
	//printf("first cpu_usage is %d\n", cpu_usage);
	//if greater than drain value set to drain      
              	if ( cpu_usage >= cpu_drain ) {
			cpu_usage = 0;
			drain = 1;
			//if we are running a count down timer close this.
			if ( ready_up == 1 ) {
				printf("closing time thread\n");
                                rcc = pthread_cancel(drain_timer_thread);
                                if (rcc != 0) {
                                	printf("Error closing timer thread. Error:%d\n", rcc);
                                }
                                ready_up = 0;
			}
		} else {
			// if we were in drain turn off drain state and start ready ups
			if ( drain == 1 ) {
				drain = 0;
				//start timer to send ready upds for 30 seconds
                                rc = pthread_create(&drain_timer_thread, NULL, drain_timer, &ready_up);
                 	        if (rc !=0) {
                                      printf("Error creating thread ERROR:%d\n", rc);
                              }
			}
		}
		
	//	printf(" reaedy up is %d\n", ready_up);
	//	printf("cpu usage is %d\n", cpu_usage);
	//	printf("drain is %d\n", drain);
	//we need cpu idle not usage
		cpu_usage = 100 - cpu_usage;
		if (ready_up == 1) {
			snprintf(sendBuff, sizeof(sendBuff), "ready up %d%%\r\n", cpu_usage);
        	} else {
			 snprintf(sendBuff, sizeof(sendBuff), "%d%%\r\n", cpu_usage);
		}
		write(connfd, sendBuff, strlen(sendBuff));

        	close(connfd);	
	}
	
	rcc = pthread_cancel(drain_timer_thread);
        	if (rcc != 0) {
                	printf("Error closing timer thread. Error:%d\n", rcc);
                }
	
	close(connfd);
        shutdown(listenfd,2);
	close(listenfd);
	pthread_join(drain_timer_thread, NULL);
	return 0;
}
