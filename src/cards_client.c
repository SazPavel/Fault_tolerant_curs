#include "cards.h"

/* cards_client.c contains all logic & network client functions */

// global variables (global for client only)
static int sd = -1;
struct card *my_cards = NULL;
static struct sockaddr_in saddr;
static int exit_flag = 0;
//number of cards a player has
int cards_count = 0;
//0 - if ncurses supports color
//1 - if ncurses supports color, but window size is too small
//2 - if ncurses not supports color
int graphic_mode;
//Coordinates of cursor
int cursor_coord;
//1 - player win
//0 - player lose
int game_result = 1;

static void sigint_handler(int sig)
{
    unsigned int addr_size;
    int interrupt_msg;

    addr_size = sizeof(saddr);
    interrupt_msg = CLIENT_INT;
    // send an exit message to server
    sendto(sd, &interrupt_msg, sizeof(int), MSG_CONFIRM,
           (struct sockaddr*)&saddr, addr_size);
    exit_flag = 1;
}

// sendto overload
int send_to_server(int sd, int *msg, struct sockaddr *saddr,
                   unsigned int addr_size)
{
    if (sendto(sd, msg, sizeof(*msg), MSG_CONFIRM, saddr, addr_size) < 0)
        return CLIENT_ERROR;
    return *msg;
}

// recvfrom overload
struct server_message recv_from_server(int sd, struct sockaddr *saddr,
                                       unsigned int *addr_size)
{
    struct server_message response;
    if (recvfrom(sd, &response, sizeof(response),
                 MSG_WAITALL, saddr, addr_size) < 0)
        response.type = SERVER_ERROR;
    return response;
}

// client input function
int client_input(int pass, int score)
{
    int msg = CLIENT_PASS;
    if (!pass) {
        while (1) {
            msg = reading_keystrokes();
            if (cards_count < 2 && msg == CLIENT_PASS) continue;
            break;
        }
    return msg;
    }
    else return reading_keystrokes();
}

// bot AI function
// this is a very simple bot: it takes new card only if his score < 18
int bot_input(int pass, int score, int bot_mode)
{
    if (pass) return CLIENT_NONE;
    switch(bot_mode) {
        case BOT_LT17:
            return (score < 17 || cards_count < 2) ? CLIENT_TAKE : CLIENT_PASS;
        case BOT_LT19:
            return (score < 19 || cards_count < 2) ? CLIENT_TAKE : CLIENT_PASS;
        case BOT_MOAR:
            return CLIENT_TAKE;
    }
    return (cards_count > 1) ? CLIENT_PASS:CLIENT_TAKE;    
}

void menu_draw()
{
    int msg, score;
    score = player_get_score();
    while(1)
    {
        msg = client_input(1, score);
        if(msg == CLIENT_EXIT)
            break;
    }
}

// winning and losing check
int check_game_conditions(struct card *cards, int count,int score)
{
    int i, five_pics;
    // 2 aces is a special winning condition
    if (count == 2 && score == 22) return CLIENT_WIN;
    // 5 pictures (J, Q, K) is a special winning condition
    if (count == 5) {
        for (five_pics = 1, i = 0; i < count; i++)
            if (cards[i].rank > R_JACK || cards[i].rank == R_ACE) {
                five_pics = 0;
                break;
            }
        if (five_pics) return CLIENT_WIN;
    }
    // 21 points is a normal winning condition
    if (score == 21) return CLIENT_WIN;
    // too high score is a losing condition
    if (score > 21) return CLIENT_LOSE;
    return CLIENT_NONE;
}

// function for client-server interaction
int client(int mode, int bot_mode)
{
    unsigned int addr_size;
    int msg;
    struct server_message response;
    fd_set readset;
    struct timeval timeout;
    my_cards = malloc(MAX_CARDS_CLIENT * sizeof(struct card));
    int my_score = 0;
    int pass = 0;

    if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return -1;
    }
    fcntl(sd, F_SETFL, O_NONBLOCK);
    setlocale(LC_ALL, "");
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (mode == M_USR) start_graphic();
    addr_size = sizeof(saddr);
    while (!exit_flag) {
        FD_ZERO(&readset);
        FD_SET(sd, &readset);
        msg = (mode == M_USR) ? client_input(pass, my_score):
                                bot_input(pass, my_score, bot_mode);

        if (exit_flag) break;
        if(msg != CLIENT_NONE)
            msg = send_to_server(sd, &msg, (struct sockaddr*)&saddr, addr_size);
        if (msg == CLIENT_EXIT || msg == CLIENT_ERROR) break;

        if (msg == CLIENT_PASS) 
        {
            change_game_mode(1, mode);
            pass = 1;
            if (mode == M_USR) draw_all();
            msg = send_to_server(sd, &msg, (struct sockaddr*)&saddr, addr_size);
        }

        if (exit_flag) break;
        if(select(sd + 1, &readset, NULL, NULL, &timeout) < 0) {
            perror("select");
            return -1;
        }
        //if server sent message
        if(FD_ISSET(sd, &readset))
        {
            response = recv_from_server(sd, (struct sockaddr*)&saddr, &addr_size);
            if (response.type == SERVER_CARD) {
                my_cards[cards_count] = response.card;
                my_score += response.card.value;
                cards_count++;
                if (mode == M_USR) draw_all();
            }

            msg = check_game_conditions(my_cards, cards_count, my_score);
            if (response.type == SERVER_WIN)
                msg = CLIENT_LOSE;
            if (response.type == SERVER_LOSE)
                msg = CLIENT_WIN;

            switch(msg) {
                case CLIENT_LOSE:
                    game_result = 0;
                case CLIENT_WIN: {
                    change_game_mode(2, mode);
                    send_to_server(sd, &msg, (struct sockaddr*)&saddr, addr_size);
                    exit_flag = 1;
                    if (mode == M_USR) menu_draw();
                } break;
            }
        }
        // delay used to avoid 100% CPU load
        usleep(1000);
    }

    if (mode == M_USR) stop_graphic();
    close(sd);
    free(my_cards);
    return 0;
}

// main client function
int cards_client(int mode, int bot_mode, const char *server_address)
{
    struct sigaction sigint;
    // signal SIGINT handler
    sigint.sa_handler = sigint_handler;
    sigint.sa_flags = 0;
    sigemptyset(&sigint.sa_mask);
    sigaddset(&sigint.sa_mask, SIGINT);
    sigaction(SIGINT, &sigint, 0);

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(server_address);
    saddr.sin_port = htons(PORT);

    signal(SIGWINCH, resize_handler);
    client(mode, bot_mode);

    return 0;
}
