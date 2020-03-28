#include "cards.h"

int server_main(const char *argv)
{
	char commands[MAX_STRING_SIZE];
	int init_mode = SERVER_WORKING;
	int i;
	pid_t pid;

	printf("LAUNCHER PID IS %d\n", getpid());
	printf("STARTING SERVERS...\n");

	for (i = 0; i < NSERVERS; i++, init_mode = SERVER_SLEEPING)
	{
		switch(pid = fork()) {
			case -1:
				printf("FATAL ERROR!\n");
				exit(-1);
			break;
			case 0:
				cards_server(argv, init_mode);
				exit(0);
			break;
			default:
				printf("LAUNCHING SERVER #%d (PID %d) WITH MODE %d\n",
					   i, pid, init_mode);

		}

	}

	//sprintf(commands, "./cards server %s %d", argv, init_mode);
	//printf("EXECUTING: %s\n", commands);

	//system(commands);
	printf("FINISHING MAIN PROCESS...\n");
	exit(0);
}