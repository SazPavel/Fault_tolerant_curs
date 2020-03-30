#include "cards.h"

/* cards_server_main.c contains initial server function which launch servers */

// launch NSERVERS servers for fault tolerancy
int server_main(const char *argv)
{
    int      init_mode = SERVER_WORKING;
    int      i;
    pid_t    pid;

    printf("LAUNCHER PID IS %d\n", getpid());
    printf("STARTING SERVERS...\n");

    for (i = 0; i < NSERVERS; i++, init_mode = SERVER_SLEEPING) {
        switch(pid = fork()) {
            case -1:
                printf("FATAL ERROR!\n");
                exit(-1);
            break;
            case 0:
                cards_server(argv, init_mode, i);
                exit(0);
            break;
            default:
                printf("LAUNCHING SERVER #%d (PID %d) WITH MODE %d\n",
                       i, pid, init_mode);
        }
    }

    printf("FINISHING MAIN PROCESS...\n");
    exit(0);
}