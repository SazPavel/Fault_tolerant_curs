#include "cards.h"

/* cards_server.c contains logic & network server functions */

// global variables (global for server only)
int                    sd;
int                    exit_flag = 0;
pthread_mutex_t        stat_lock;
pthread_mutex_t        card_lock;
pthread_t              tid[MAX_PLAYERS];
struct card           *deck = NULL;
struct subserv         serv[MAX_PLAYERS];
int                    players_count = 0;
char                   ip_addr[MAX_STRING_SIZE] = {0};
char                   log_filename[MAX_STRING_SIZE] = {0};
FILE                  *log_file = NULL;
int                    server_number = -1;
int                    server_mode = SERVER_WRONG;
pthread_t              sync_tid;
int                    last_response_time = 0;
int                    sync_sd_from = 0;
int                    sync_sd_to = 0;
struct sync_message    server_params;

// SIGUSR handler from cards_server_special
extern  void sigusr_handler(int sig);
// SIGINT handler from cards_server_special
extern void sigint_handler(int sig);

// create answer to client's message as struct server_message
int handle_client_message(int msg, struct server_message *answerptr, int ind)
{
    struct server_message    answer;
    int                      score1;
    int                      score2;
    int                      max;

    switch(msg) {
        case CLIENT_TAKE:
            // give card to the player
            answer.card = deck_pop_card(deck, DECK_SIZE);
            answer.type = SERVER_CARD;
            pthread_mutex_lock(&stat_lock);
            serv[ind].player_score += answer.card.value;
            server_log("player%d's total score: %d\n",
                   ind, serv[ind].player_score);
            pthread_mutex_unlock(&stat_lock);
        break;
        case CLIENT_PASS:
            // if player passed
            pthread_mutex_lock(&stat_lock);
            if (serv[ind].pass_flag && serv[ind^1].pass_flag) {
                score1 = serv[ind].player_score;
                score2 = serv[ind^1].player_score;
                max = (score1 > score2) ? score1 : score2;
                // if scores are equal, player 0 wins (no draw)
                if (score1 == score2)
                    max = serv[0].player_score;

                msg = (serv[ind].player_score == max) ?
                      CLIENT_WIN : CLIENT_LOSE;
            }
            serv[ind].pass_flag = 1;
            pthread_mutex_unlock(&stat_lock);
        default:
            answer.card = (struct card){R_VOID, 0, 0};
            answer.type = SERVER_NONE;
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
    struct sigaction         sigusr;
    unsigned int             addr_size;
    struct sockaddr_in       caddr;
    struct server_message    answer;
    fd_set                   readset;
    struct timeval           timeout;
    int                      sd;
    int                      ind;
    int                      msg;
    int                      pass_flag = 0;
    int 					 ping_delay = 21;

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
        if (serv[ind].cont_flag == 0) {
            serv[ind].xmsg = -1;
            serv[ind].breaking_news = 0;
            serv[ind].player_score = 0;
            serv[ind].pass_flag = 0;
        }
        pthread_mutex_unlock(&stat_lock);
        // mutex section end

        // first message handling
        // it's a message sent to main server
        // it needs separate handling because connection
        // must be established between a client and THIS subserver
        if (serv[ind].cont_flag == 0) {
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
        }

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
                ping_delay = 21;
                pthread_mutex_lock(&card_lock);
                msg = handle_client_message(msg, &answer, ind);
                pthread_mutex_unlock(&card_lock);
                if (msg != CLIENT_NONE)
                	server_log(" %d recv'd: %d\n", ind, msg);
                // if received LOSE or WIN
                // then it must be sent to all clients
                pass_flag = 0;

                if (msg >= CLIENT_MAX || msg == CLIENT_ERROR)
                {
                    pthread_mutex_lock(&stat_lock);
                    answer.type = SERVER_ERROR_EXT;
                    serv[ind^1].breaking_news = serv[ind].breaking_news = 1;
                    serv[ind^1].xmsg = serv[ind].xmsg = SERVER_ERROR_EXT;
                    send_extra_message(ind, &answer);
                    pthread_mutex_unlock(&stat_lock);
                    break;
                }
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

                if (msg != CLIENT_NONE)
                {
	                server_log("sent:\n");
	                if (answer.type == SERVER_CARD)
	                    deck_print(&(answer.card), 1);
	                else
	                    server_log("%d\n", answer.type);
	            }
            }else{ // isset
            	ping_delay -= 1;
            	if (ping_delay == 0)
            	{
                    pthread_mutex_lock(&stat_lock);
                    answer.type = SERVER_CL_EXT;
                    serv[ind^1].breaking_news = serv[ind].breaking_news = 1;
                    serv[ind^1].xmsg = serv[ind].xmsg = SERVER_CL_EXT;
                    send_extra_message(ind, &answer);
                    pthread_mutex_unlock(&stat_lock);
            	}
            }

            //if another client wins or loses
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
            if (serv[ind].cont_flag == 1)
                serv[ind].cont_flag = 0;
            pthread_mutex_unlock(&stat_lock);

            // delay used to avoid 100% CPU load
            usleep(1000);
        }

        // close this subserver
        close(sd);
        pthread_mutex_lock(&stat_lock);
        server_log(" * Player%d ", ind);
        if (serv[ind].xmsg == SERVER_WIN)
            server_log("LOSE");
        else if (serv[ind].xmsg == SERVER_LOSE)
            server_log("WIN");
        else
            server_log("EXIT");

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
int server()
{
    struct sockaddr_in    saddr;
    struct sockaddr_in    caddr;
    unsigned int          addr_size;
    int                   msg = -1;
    int                   i;
    int                   found_free;

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

    server_sync_update_info(&server_params);

    server_log("Creating threads...\n");
    // init an array of subservers
    for (i = 0; i < MAX_PLAYERS; i++) {
        pthread_create(tid + i, NULL, subserver, (void*)(&serv[i].ind));
    }
    sleep(1);

    found_free = -1;

    server_log("Waiting for connection...\n");

    // if this is not original server
    if (server_params.cont_flag == 1) {
        pthread_mutex_lock(&stat_lock);
        server_params.cont_flag = 0;
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (serv[i].state == SS_FREE)
                serv[i].cont_flag = 0;
            else
                pthread_kill(tid[i], SIGUSR1);
        }
        pthread_mutex_unlock(&stat_lock);
    }


    while (1) {
        // receive the first message which will be redirected to pthread function
        recvfrom(sd, &msg, sizeof(msg), MSG_WAITALL,
                 (struct sockaddr*)&caddr, &addr_size);
        if (msg == CLIENT_EXIT || msg == CLIENT_INT)
            continue;
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

// main server function
int cards_server(const char *argv, int init_mode, int number)
{
    struct sigaction    sigint;
    int                 i;

    // init global params
    server_number = number;
    server_mode = init_mode;
    sprintf(log_filename, "server_%d_log.txt", server_number);
    memcpy(ip_addr, argv, strlen(argv));

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

    // init server params structure
    memcpy(server_params.deck, deck, DECK_SIZE * sizeof(struct card));
    for (i = 0; i < MAX_PLAYERS; i++) {
        server_params.subservers[i].state = SS_FREE;
        server_params.subservers[i].sd = sd;
        server_params.subservers[i].ind = i;
        server_params.subservers[i].msg = -1;
        memset(&(server_params.subservers[i].client),
               0, sizeof(server_params.subservers[i].client));
        server_params.subservers[i].breaking_news = 0;
        server_params.subservers[i].xmsg = -1;
        server_params.subservers[i].player_score = 0;
        server_params.subservers[i].pass_flag = 0;
        server_params.subservers[i].cont_flag = 0;
    }
    server_params.server_mode = server_mode;
    server_params.server_number = server_number;
    server_params.players_count = 0;
    server_params.cont_flag = 0;

    // init mutexes
    pthread_mutex_init(&stat_lock, NULL);
    pthread_mutex_init(&card_lock, NULL);
    pthread_create(&sync_tid, NULL, server_sync, NULL);

    // wait for the mode to change
    // (mode can be changed in a pthread sync function)
    while (1) {
        if (server_mode == SERVER_WORKING) {
            server();
            break;
        }

        // delay used to avoid 100% CPU load
        usleep(1000);
    }
    return 0;
}
