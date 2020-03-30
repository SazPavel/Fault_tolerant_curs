
#include "cards.h"

/* cards_server_syncl.c contains server sync functions */

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

// server function for creation of the sync info
// pthread mutex must be locked inside this function
int server_sync_create_info(struct sync_message *sync_msg)
{
    pthread_mutex_lock(&stat_lock);
    pthread_mutex_lock(&card_lock);

    sync_msg->players_count = players_count;
    sync_msg->server_number = server_number;
    sync_msg->server_mode = server_mode;
    memcpy((sync_msg->subservers), serv, MAX_PLAYERS * sizeof(struct subserv));
    memcpy((sync_msg->deck), deck, DECK_SIZE * sizeof(struct card));

    pthread_mutex_unlock(&card_lock);
    pthread_mutex_unlock(&stat_lock);

    return 0;
}

// server function which updates server variables
// according to sync info parameters
// pthread mutex must be locked inside this function
int server_sync_update_info(struct sync_message *sync_msg)
{
    pthread_mutex_lock(&stat_lock);
    pthread_mutex_lock(&card_lock);

    memcpy(&server_params, sync_msg, sizeof(struct sync_message));

    players_count = sync_msg->players_count;
    //server_number = sync_msg->server_number;
    //server_mode = sync_msg->server_mode;
    memcpy(serv, &(sync_msg->subservers), MAX_PLAYERS * sizeof(struct subserv));
    memcpy(deck, &(sync_msg->deck), DECK_SIZE * sizeof(struct card));


    pthread_mutex_unlock(&card_lock);
    pthread_mutex_unlock(&stat_lock);

    return 0;
}

// server pthread function for sync message processing
void *server_sync()
{
    struct timeval         timeout;
    struct sync_message    sync_msg;
    struct sockaddr_in     saddr_from;
    struct sockaddr_in     saddr_to;
    fd_set                 readset;
    unsigned int           addr_size = sizeof(struct sockaddr_in);
    int                    ret;
    int                    i;

    // init endpoint parameters
    memset(&saddr_from, 0, addr_size);
    memset(&saddr_to, 0, addr_size);

    saddr_from.sin_family = AF_INET;
    saddr_from.sin_addr.s_addr = inet_addr(ip_addr);
    saddr_from.sin_port = htons(SERVER_PORT + server_number);
 
    saddr_to.sin_family = AF_INET;
    saddr_to.sin_addr.s_addr = inet_addr(ip_addr);
    saddr_to.sin_port = htons(SERVER_PORT + (server_number  + 1));

    sync_sd_from = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sync_sd_from < 0) {
        perror("sync socket from");
        return NULL;
    }
    sync_sd_to = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sync_sd_to < 0) {
        perror("sync socket to");
        return NULL;
    }

    if (bind(sync_sd_from, (struct sockaddr*)&saddr_from, addr_size) < 0) {
        perror("sync bind from");
        return NULL;
    }

    fcntl(sync_sd_from, F_SETFL, O_NONBLOCK);
    fcntl(sync_sd_to, F_SETFL, O_NONBLOCK);

    server_log("socket FROM: sd %d  port %d\n",
               sync_sd_from, ntohs(saddr_from.sin_port));
    server_log("socket TO: sd %d  port %d\n",
               sync_sd_to, ntohs(saddr_to.sin_port));

    // in this loop the sleeping server should process
    // sync packets from the working server and
    // update its subservers accordingly.
    // If no response received for the certain amount of time
    // the sleeping server will change its mode to SERVER_WORKING
    while (1) {
        FD_ZERO(&readset);
        timeout.tv_sec = SYNC_DELAY_SEC;
        timeout.tv_usec = SYNC_DELAY_USEC;
        FD_SET(sync_sd_from, &readset);

        // this select() currently is used only as a timer
        if(select(0, NULL, NULL, NULL, &timeout) < 0) {
            perror("select");
            break;
        }

        // try to receive sync message
        if (server_mode == SERVER_SLEEPING)
        {
            ret = recvfrom(sync_sd_from, &sync_msg, sizeof(sync_msg),
                           MSG_DONTWAIT, (struct sockaddr*)&saddr_from, &addr_size);
            if (ret <= 0)
            {
                last_response_time += (SYNC_DELAY_SEC * 1000000 + SYNC_DELAY_USEC);
                server_log("no message received; set time to %d usec\n", last_response_time);
                // it seems active server is not working
                // so this server must become the working server
                if (last_response_time > MAX_SYNC_DELAY_SEC * 1000000 + MAX_SYNC_DELAY_USEC) {
                    for (i = 0; i < MAX_PLAYERS; i++) {
                        server_params.subservers[i].cont_flag = 1;
                    }
                    server_params.cont_flag = 1;
                    server_mode = SERVER_WORKING;
                }
            } else {
                //server_log("recv'd %d bytes from sync sd; set time to 0 usec\n", ret);
                last_response_time = 0;
                server_sync_update_info(&sync_msg);

                //server_log("***RCVD:\n");
                //serv_show_info(&server_params);
            }
        }

        // send sync message to the next server
        if (server_number < NSERVERS - 1) {
            server_sync_create_info(&sync_msg);
            ret = sendto(sync_sd_to, &sync_msg, sizeof(sync_msg), 0,
                         (struct sockaddr*)&saddr_to, addr_size);
            if (ret < 0)
                perror("sendto");
            //server_log("sent %d bytes to sync sd\n", ret);

            //server_log("***SENT:\n");
            //serv_show_info(&sync_msg);
        }
    }

    return NULL;
}