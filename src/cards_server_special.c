#include "cards.h"

/* cards_server_special.c contains I/O server functions and signal handling */

// global variables from cards_server.c
extern int                    sd;
extern int                    exit_flag;
extern pthread_mutex_t        stat_lock;
extern pthread_mutex_t        card_lock;
extern pthread_t              tid[MAX_PLAYERS];
extern struct card           *deck;
extern struct subserv         serv[MAX_PLAYERS];
extern int                    players_count;
extern char                   ip_addr[MAX_STRING_SIZE];
extern char                   log_filename[MAX_STRING_SIZE];
extern FILE                  *log_file;
extern int                    server_number;
extern int                    server_mode;
extern pthread_t              sync_tid;
extern int                    last_response_time;
extern int                    sync_sd_from;
extern int                    sync_sd_to;
extern struct sync_message    server_params;

// signal SIGUSR handler
void sigusr_handler(int sig)
{
    UNUSED(sig);
}

// signal SIGINT handler
void sigint_handler(int sig)
{
    int    i;

    UNUSED(sig);

    server_log("\nEXIT\n");
    exit_flag = 1;

    for (i = 0; i < MAX_PLAYERS; i++) {
        pthread_cancel(tid[i]);
        pthread_join(tid[i], NULL);
    }

    shutdown(sd, SHUT_RDWR);
    pthread_mutex_destroy(&stat_lock);
    pthread_mutex_destroy(&card_lock);
    free(deck);
    fclose(log_file);

    pthread_cancel(sync_tid);
    pthread_join(sync_tid, NULL);
    close(sd);
    close(sync_sd_from);
    close(sync_sd_to);

    exit(0);
}

// function used for output (to the logging file or to stdout)
int server_log(const char *format, ...)
{
    va_list    args;
    int        retval;


    va_start(args, format);

#if defined (PRINT_FILE)
    log_file = fopen(log_filename, "a");
    if (log_file == NULL)
        retval = -1;
    else
        retval = vfprintf(log_file, format, args);
    fclose(log_file);
#elif defined (PRINT_SCREEN)
    retval = vprintf(format, args);
#endif

    va_end(args);
    return retval;
}


// print subserver info
void serv_print(int ind)
{
    server_log("\nSS %d:\n", ind);
    server_log("state =    %d\n", serv[ind].state);
    server_log("msg   =    %d\n", serv[ind].msg);
    server_log("break =    %d\n", serv[ind].breaking_news);
    server_log("xmsg  =    %d\n", serv[ind].xmsg);
    server_log("score =    %d\n", serv[ind].player_score);
    server_log("pass  =    %d\n", serv[ind].pass_flag);
    server_log("\n");
}

// print server info
void serv_show_info(struct sync_message *sync_msg)
{
    int    i;

    server_log("\n\nSERVER INFO:\n");
    server_log("========================================\n");    
    for (i = 0; i < MAX_PLAYERS; i++) {
        server_log("\nSS %d:\n", i);
        server_log("state =    %d\n", sync_msg->subservers[i].state);
        server_log("msg   =    %d\n", sync_msg->subservers[i].msg);
        server_log("break =    %d\n", sync_msg->subservers[i].breaking_news);
        server_log("xmsg  =    %d\n", sync_msg->subservers[i].xmsg);
        server_log("score =    %d\n", sync_msg->subservers[i].player_score);
        server_log("pass  =    %d\n", sync_msg->subservers[i].pass_flag);
        server_log("\n");
    }
    server_log("player count: %d\n", sync_msg->players_count);
    server_log("deck:\n");
    deck_print(sync_msg->deck, 4);
    server_log("========================================\n\n");
}