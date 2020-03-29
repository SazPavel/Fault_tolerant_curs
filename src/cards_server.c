#include "cards.h"

/* cards_server.c contains logic & network & I/O server functions */

// global variables (global for server only)
static int sd;
static int exit_flag = 0;
static pthread_mutex_t stat_lock;
static pthread_mutex_t card_lock;
static pthread_t tid[MAX_PLAYERS];
static struct card *deck = NULL;
struct subserv serv[MAX_PLAYERS];
static int players_count = 0;
static const char *ip_addr;
static char log_filename[MAX_STRING_SIZE];
static FILE *log_file = NULL;
static int server_number = -1;
static int server_mode = SERVER_WRONG;
static pthread_t sync_tid;
static int last_response_time = 0;
static int sync_sd_from = 0;
static int sync_sd_to = 0;
static int sync_sd = -1;

static void sigusr_handler(int sig)
{
    /*...*/
}
// signal SIGINT handler
static void sigint_handler(int sig)
{
    int i;
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
    //close(sync_sd);

    exit(0);
}

int server_log(const char *format, ...)
{
    int retval;
    va_list args;
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


void serv_show_info()
{
    int i;

    server_log("\n\nSERVER INFO:\n");
    server_log("========================================\n");    
    for (i = 0; i < MAX_PLAYERS; i++)
        serv_print(i);
    server_log("player count: %d\n", players_count);
    server_log("deck:\n");
    deck_print(deck, 4);
    server_log("========================================\n\n");
}

// create answer to client's message as struct server_message
int handle_client_message(int msg, struct server_message *answerptr, int ind)
{
    struct server_message answer;
    int score1, score2, max;

    switch(msg) {
        case CLIENT_TAKE: {
            // give card to the player
            answer.card = deck_pop_card(deck, DECK_SIZE);
            answer.type = SERVER_CARD;
            pthread_mutex_lock(&stat_lock);
            serv[ind].player_score += answer.card.value;
            server_log("player%d's total score: %d\n",
                   ind, serv[ind].player_score);
            pthread_mutex_unlock(&stat_lock);
        } break;
        case CLIENT_PASS: {
            // if player passed
            pthread_mutex_lock(&stat_lock);
            if (serv[ind].pass_flag && serv[ind^1].pass_flag) {
                score1 = serv[ind].player_score;
                score2 = serv[ind^1].player_score;
                max = (score1 > score2) ? score1 : score2;
                // if scores are equal, player 0 wins (no draw)
                if (score1 == score2) max = serv[0].player_score;
                msg = (serv[ind].player_score == max) ?
                      CLIENT_WIN : CLIENT_LOSE;
            }
            serv[ind].pass_flag = 1;
            pthread_mutex_unlock(&stat_lock);
        }
        default: {
            answer.card = (struct card){R_VOID, 0, 0};
            answer.type = SERVER_NONE;
        }
    }
    *answerptr = answer;
    return msg;
}

// extra message - endgame information
void send_extra_message(int ind, struct server_message *answer)
{
    answer->type = serv[ind].xmsg;
    server_log("now %d's answer is %d\n", ind, answer->type);
}

// subserver's pthread function
void* subserver(void *serv_index_ptr)
{
    struct sigaction sigusr;
    int sd, ind, msg, pass_flag = 0;
    unsigned int addr_size;
    struct sockaddr_in caddr;
    struct server_message answer;

    // copy variables from serv declared in other function
    pthread_mutex_lock(&stat_lock);
    ind = *(int*)serv_index_ptr;
    sd = serv[ind].sd;
    addr_size = sizeof(serv[ind].client);
    msg = serv[ind].msg;
    pthread_mutex_unlock(&stat_lock);

    // signal SIGUSR1 handler
    sigusr.sa_handler = sigusr_handler;
    sigusr.sa_flags = 0;
    sigemptyset(&sigusr.sa_mask);
    sigaddset(&sigusr.sa_mask, SIGUSR1);
    sigaction(SIGUSR1, &sigusr, 0);

    while (1) {
        // wait for SIGUSR signal
        pause();

        pthread_mutex_lock(&stat_lock);
        serv[ind].state = SS_BUSY;
        msg = serv[ind].msg;
        server_log("%d recv'd %d\n", ind, msg);
        // create new socket
        sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_addr.s_addr = inet_addr(ip_addr);
        caddr.sin_port = htons(PORT + ind + 1); // new port
        bind(sd, (struct sockaddr*)&caddr, sizeof(caddr));
        caddr = serv[ind].client;
        serv[ind].xmsg = -1;
        serv[ind].breaking_news = 0;
        serv[ind].player_score = 0;
        serv[ind].pass_flag = 0;
        pthread_mutex_unlock(&stat_lock);
        // mutex section end

        // first message handling
        // it's a message sent to main server
        // it needs separate handling because connection
        // must be established between a client and THIS subserver
        pthread_mutex_lock(&card_lock);
        msg = handle_client_message(msg, &answer, ind);
        pthread_mutex_unlock(&card_lock);
        // first message sending
        sendto(sd, &answer, sizeof(answer), MSG_DONTWAIT,
               (struct sockaddr*)&(serv[ind].client), addr_size);
        server_log("sent:\n");
        if (answer.type == SERVER_CARD)
            deck_print(&(answer.card), 1);
        else
            server_log("%d\n", answer.type);
        fd_set readset;
        struct timeval timeout;
        fcntl(sd, F_SETFL, O_NONBLOCK);
        // this loop ends after exit message from client
        // because each subserver is created for one client
        while (1) {
            FD_ZERO(&readset);
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            FD_SET(sd, &readset);
            if(select(sd + 1, &readset, NULL, NULL, &timeout) < 0) {
                perror("select");
                break;
            }
            if(FD_ISSET(sd, &readset))
            {
                if (recvfrom(sd, &msg, sizeof(msg), MSG_WAITALL,
                             (struct sockaddr*)&caddr, &addr_size) < 0) {
                    perror("recvfrom");
                    break;
                }

                pthread_mutex_lock(&card_lock);
                msg = handle_client_message(msg, &answer, ind);
                pthread_mutex_unlock(&card_lock);

                server_log(" %d recv'd: %d\n", ind, msg);
                // if received LOSE or WIN
                // then it must be sent to all clients
                pass_flag = 0;
                if (msg == CLIENT_LOSE || msg == CLIENT_WIN) {
                    pthread_mutex_lock(&stat_lock);
                    serv[ind^1].breaking_news = 1;
                    serv[ind^1].xmsg = (msg == CLIENT_LOSE) ?
                                          SERVER_LOSE : SERVER_WIN;
                    serv[ind].breaking_news = 1;
                    serv[ind].xmsg = (msg == CLIENT_LOSE) ?
                                          SERVER_WIN : SERVER_LOSE;
                    send_extra_message(ind, &answer);
                    pass_flag = 1;
                    pthread_mutex_unlock(&stat_lock);
                }

                if (msg == CLIENT_LOSE || msg == CLIENT_WIN)
                    if (!pass_flag) break;
                if (msg == CLIENT_EXIT || msg == CLIENT_INT) {
                    pthread_mutex_lock(&stat_lock);
                    answer.type = SERVER_CL_EXT;
                    serv[ind^1].breaking_news = serv[ind].breaking_news = 1;
                    serv[ind^1].xmsg = serv[ind].xmsg = SERVER_CL_EXT;
                    send_extra_message(ind, &answer);
                    pthread_mutex_unlock(&stat_lock);
                    break;
                }
                if (sendto(sd, &answer, sizeof(struct server_message),
                           MSG_CONFIRM, (struct sockaddr*)&caddr, addr_size) < 0) {
                    perror("sendto");
                    break;
                }

                server_log("sent:\n");
                if (answer.type == SERVER_CARD)
                    deck_print(&(answer.card), 1);
                else
                    server_log("%d\n", answer.type);
            } // isset

            //serv_create_info();

            //if another client win or lose
            pthread_mutex_lock(&stat_lock);
            if(serv[ind].breaking_news == 1)
            {
                send_extra_message(ind, &answer);
                serv[ind].breaking_news = 0;
                sendto(sd, &answer, sizeof(struct server_message),
                       MSG_CONFIRM, (struct sockaddr*)&caddr, addr_size);
                pthread_mutex_unlock(&stat_lock);
                break;
            }
            pthread_mutex_unlock(&stat_lock);

            // delay used to avoid 100% CPU load
            usleep(1000);
        }
        // close this subserver
        close(sd);
        pthread_mutex_lock(&stat_lock);
        server_log(" * Player%d ", ind);
        if (serv[ind].xmsg == SERVER_WIN) server_log("LOSE");
        else if (serv[ind].xmsg == SERVER_LOSE) server_log("WIN");
        else server_log("EXIT");
        server_log(" with score %d\n", serv[ind].player_score);

        serv[ind].state = SS_FREE;
        serv[ind].sd = 0;
        serv[ind].msg = -1;
        memset(&serv[ind].client, 0, sizeof(serv[ind].client));
        serv[ind].breaking_news = 0;
        serv[ind].xmsg = -1;
        serv[ind].player_score = 0;
        serv[ind].pass_flag = 0;
        players_count--;
        server_log("exited tid %d\n", ind);
        // if last player exited, create deck again
        if (players_count == 0) {
            pthread_mutex_lock(&card_lock);
            deck_create(deck, DECK_SIZE);
            deck_shuffle(deck, DECK_SIZE);
            server_log("created deck\n");
            pthread_mutex_unlock(&card_lock);
        }
        pthread_mutex_unlock(&stat_lock);
    }
    return NULL;
}

// socket functions are here
int server(struct card *deck)
{
    struct sockaddr_in saddr, caddr;
    unsigned int addr_size;
    int msg = -1;
    int i;
    int found_free;

    // init all network stuff
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sd < 0) {
        perror("socket");
        return -1;
    }
    memset(&saddr, 0, sizeof(saddr));
    memset(&caddr, 0, sizeof(caddr));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(ip_addr);
    saddr.sin_port = htons(PORT);
    addr_size = sizeof(saddr);

    if (bind(sd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        return -1;
    }


    server_log("Creating threads...\n");
    // init an array of subservers
    for (i = 0; i < MAX_PLAYERS; i++) {
        serv[i].state = SS_FREE;
        serv[i].sd = sd;
        serv[i].ind = i;
        serv[i].msg = -1;
        memset(&serv[i].client, 0, sizeof(serv[i].client));
        serv[i].breaking_news = 0;
        serv[i].xmsg = -1;
        serv[i].player_score = 0;
        serv[i].pass_flag = 0;

        pthread_create(tid + i, NULL, subserver, (void*)(&serv[i].ind));
    }
    sleep(1);

    found_free = -1;

    server_log("Waiting for connection...\n");
    while (1) {
        // receive the first message which will be redirected to pthread function
        recvfrom(sd, &msg, sizeof(msg), MSG_WAITALL,
                 (struct sockaddr*)&caddr, &addr_size);
        if (msg == CLIENT_EXIT || msg == CLIENT_INT) continue;
        // while no threads are available
        while (found_free == -1) {
            usleep(10000);
            // search for available thread
            for (i = 0; i < MAX_PLAYERS && found_free < 0; i++) {
                pthread_mutex_lock(&stat_lock);
                if (serv[i].state == SS_FREE) {
                    found_free = i;
                    serv[i].msg = msg;
                    serv[i].client = caddr;
                    players_count++;
                }
                pthread_mutex_unlock(&stat_lock);
            }
        }
        server_log(" received %d -> sending it to %d\n", msg, found_free);
        pthread_kill(tid[found_free], SIGUSR1);
        found_free = -1;
    }
    return 0;
}

// server function for creation of the sync info
// pthread mutex must be locked inside this function
int server_sync_create_info(struct sync_message *sync_msg)
{
    pthread_mutex_lock(&stat_lock);
    pthread_mutex_lock(&card_lock);

    sync_msg->players_count = players_count;
    sync_msg->server_number = server_number;
    sync_msg->server_mode = server_mode;
    memcpy(&(sync_msg->subservers), serv, MAX_PLAYERS * sizeof(struct subserv));
    memcpy(&(sync_msg->deck), deck, DECK_SIZE * sizeof(struct card));

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
    fd_set readset;
    struct timeval timeout;
    struct sync_message sync_msg;
    struct sockaddr_in saddr_from;
    //struct sockaddr_in caddr_from;
    struct sockaddr_in saddr_to;
    //struct sockaddr_in caddr_to;
    struct sockaddr_in sync_saddr;
    unsigned int addr_size = sizeof(struct sockaddr_in);
    int ret;
/*
    memset(&sync_saddr, 0, addr_size);
    sync_saddr.sin_family = AF_INET;
    sync_saddr.sin_addr.s_addr = inet_addr(ip_addr);
    sync_saddr.sin_port = htons(SERVER_PORT + server_number);

    sync_sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sync_sd < 0) {
        perror("sync socket from");
        return NULL;
    }

    if (bind(sync_sd, (struct sockaddr*)&sync_saddr, addr_size) < 0)
    {
        perror("sync bind ");
        return NULL;
    }

    fcntl(sync_sd, F_SETFL, O_NONBLOCK);
    server_log("SYNC SOCKET:  sd %d  port %d\n",
               sync_sd, ntohs(sync_saddr.sin_port));
*/

    // init endpoint parameters
    memset(&saddr_from, 0, addr_size);
    //memset(&caddr_from, 0, addr_size);
    memset(&saddr_to, 0, addr_size);
    //memset(&caddr_to, 0, addr_size);

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

    if (bind(sync_sd_from, (struct sockaddr*)&saddr_from, addr_size) < 0)
    {
        perror("sync bind from");
        return NULL;
    }
/*
    if (bind(sync_sd_to, (struct sockaddr*)&saddr_to, addr_size) < 0)
    {
        perror("sync bind to");
        return NULL;
    }
*/
    fcntl(sync_sd_from, F_SETFL, O_NONBLOCK);
    fcntl(sync_sd_to, F_SETFL, O_NONBLOCK);

    server_log("socket FROM: sd %d  port %d\n",
               sync_sd_from, ntohs(saddr_from.sin_port));
    server_log("socket TO: sd %d  port %d\n",
               sync_sd_to, ntohs(saddr_to.sin_port));

    while (1)
    {
        FD_ZERO(&readset);
        timeout.tv_sec = SYNC_DELAY_SEC;
        timeout.tv_usec = SYNC_DELAY_USEC;
        FD_SET(sync_sd_from, &readset);

        if(select(0, NULL, NULL, NULL, &timeout) < 0) {
            perror("select");
            break;
        }

        // recvd stuff from server
        /*
        if(FD_ISSET(sync_sd_from, &readset)) {
            if (server_mode == SERVER_WORKING) {
                server_log("ACHTUNG! SOMETHING WRONG HAPPENED HERE!\n");
            }
            else if (server_mode == SERVER_SLEEPING) {
                server_log("recv'd sync message from the other server\n");
                ret = recvfrom(sync_sd_from, &sync_msg, sizeof(sync_msg),
                               MSG_DONTWAIT, (struct sockaddr*)&saddr_from, &addr_size);
                server_log("recv'd %d bytes from sync sd\n", ret);
            }
        }
        // time out
        else {
            if (server_mode == SERVER_WORKING) {
                //...
            }
            else if (server_mode == SERVER_SLEEPING) {
                last_response_time += (SYNC_DELAY_SEC * 1000000 + SYNC_DELAY_USEC);
                server_log("no message received; set time to %d usec\n", last_response_time);
            }
        }
        */

        if (server_mode == SERVER_SLEEPING)
        {
            ret = recvfrom(sync_sd_from, &sync_msg, sizeof(sync_msg),
                           MSG_DONTWAIT, (struct sockaddr*)&saddr_from, &addr_size);
            if (ret <= 0)
            {
                last_response_time += (SYNC_DELAY_SEC * 1000000 + SYNC_DELAY_USEC);
                server_log("no message received; set time to %d usec\n", last_response_time);
            } else {
                server_log("recv'd %d bytes from sync sd; set time to 0 usec\n", ret);
                last_response_time = 0;
                server_sync_update_info(&sync_msg);

                server_log("***RCVD:\n");
                serv_show_info();
            }
        }

        // send sync message to the next server
        if (server_number < NSERVERS - 1) {
            server_sync_create_info(&sync_msg);
            ret = sendto(sync_sd_to, &sync_msg, sizeof(sync_msg), 0,
                         (struct sockaddr*)&saddr_to, addr_size);
            if (ret < 0)
                perror("sendto");
            server_log("sent %d bytes to sync sd\n", ret);

            server_log("***SENT:\n");
            serv_show_info();

        }

    }
    return NULL;
}

// main server function
int cards_server(const char *argv, int init_mode, int number)
{
    struct sigaction sigint;

    // init global params
    server_number = number;
    server_mode = init_mode;
    sprintf(log_filename, "server_%d_log.txt", server_number);
    ip_addr = argv;


    if (server_mode == SERVER_WORKING) {
        server_log("serv mode is WORKING\n");
    } else if (server_mode == SERVER_SLEEPING) {
        server_log("serv mode is SLEEPING\n");
    } else if (server_mode == SERVER_WRONG) {
        printf("BE GONE! YOU SHALL NOT RUN THIS APPLICATION WITHOUT SAFE MODE!\n");
        printf("RUN 'cards safeserver' INSTEAD AND MAY THE FORCE BE WITH YOU!\n");
        return -1;
    }

    // refresh output file
#ifdef PRINT_FILE
    log_file = fopen(log_filename, "w");
    if (log_file) {
        fprintf(log_file, "LOGGING STARTED\n");
        fclose(log_file);
    }
#endif

    deck = malloc(DECK_SIZE * sizeof(struct card));

    server_log("Starting server...\nPress Ctrl+C to exit\n");
    usleep(1000000);
    server_log("server PPID == %d\n", getppid());
    // signal SIGINT handler
    sigint.sa_handler = sigint_handler;
    sigint.sa_flags = 0;
    sigemptyset(&sigint.sa_mask);
    sigaddset(&sigint.sa_mask, SIGINT);
    sigaction(SIGINT, &sigint, 0);
    // set random
    srand(time(0));

    // create deck
    deck_create(deck, DECK_SIZE);
    deck_shuffle(deck, DECK_SIZE);
    //deck_print(deck, DECK_SIZE);



    // init mutexes
    pthread_mutex_init(&stat_lock, NULL);
    pthread_mutex_init(&card_lock, NULL);
    pthread_create(&sync_tid, NULL, server_sync, NULL);



    // in this loop the sleeping server should process
    // sync packets from the working server and
    // update its subservers accordingly.
    // If no response received for the certain amount of time
    // the sleeping server will change its mode to SERVER_WORKING
    while (1)
    {
        if (server_mode == SERVER_WORKING) {
            server(deck);
            break;
        }

        usleep(1000);


    }


    return 0;
}
