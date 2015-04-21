#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "feedback.cfg"
#define MAXBUF 1024
#define DELIM "="

struct config
{
	char port[MAXBUF];
	char drain_cpu[MAXBUF];
};

struct config get_config(char *filename)
{
	struct config configstruct;
	FILE *file = fopen(filename,"r");

	if (file !=NULL)
	{
		char line[MAXBUF];
		int i =0;
		
		while(fgets(line, sizeof(line), file) != NULL)
		{
			char *cfline;
			cfline = strstr((char *)line,DELIM);
			cfline = cfline + strlen(DELIM);
			
			if (i == 0)
			{
				memcpy(configstruct.port,cfline,strlen(cfline));
		//		printf("port is %s\n", configstruct.port);
			} else if (i == 1) 
			{
				memcpy(configstruct.drain_cpu,cfline,strlen(cfline));
		//		printf(" drain cpu is %s\n", configstruct.drain_cpu);
			} 
			i++;
		}
	}
	fclose(file);

	return configstruct; 
}
/*
//int main (int arg, char **argc)
int main() {

	struct config configstruct;
	
	configstruct = get_config(FILENAME);
	
//	printf("port is %s\n", configstruct.port);
//	printf(" drain cpu is %s\n", configstruct.drain_cpu);

	//change port to int
	int intport;
	intport = atoi(configstruct.port);
//	printf("port is now an integer %d\n", intport);

	return 0;
}
*/
