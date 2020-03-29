#ifndef ELT_CARD_H
#define ELT_CARD_H

/* cards.h contains all defines, includes and function headers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <locale.h>
#include <ncurses.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdarg.h>

// client modes - user or AI
#define M_USR 0
#define M_BOT 1
// our port number
#define PORT 2121
// deck size - 36 cards (russian deck)
#define DECK_SIZE 36
// max cards per client - 8
#define MAX_CARDS_CLIENT 8
// max players - 2
#define MAX_PLAYERS 2
// card ranks
#define R_ACE 0
#define R_KING 1
#define R_QUEEN 2
#define R_JACK 3
#define R_10 4
#define R_9 5
#define R_8 6
#define R_7 7
#define R_6 8
#define R_VOID 9
// card suits
#define S_HEARTS 0
#define S_DIAMONDS 1
#define S_CLUBS 2
#define S_SPADES 3
// commands from client to server
#define CLIENT_ERROR -1
#define CLIENT_EXIT 0
#define CLIENT_TAKE 1
#define CLIENT_PASS 2
#define CLIENT_LOSE 3
#define CLIENT_WIN 4
#define CLIENT_DRAW 5
#define CLIENT_INT 6
#define CLIENT_NONE 7
#define CLIENT_READY 8
// commands from server to client
#define SERVER_ERROR -1
#define SERVER_NONE 0
#define SERVER_CARD 1
#define SERVER_WIN 2
#define SERVER_LOSE 3
#define SERVER_CL_EXT 4
// server states
#define SS_FREE 0
#define SS_BUSY 1
// color codes used by server
#define CNRM "\x1B[0m"
#define CRED "\x1B[31m"
// ncurses card size
#define CARD_SIZE_Y 7
#define CARD_SIZE_X 8
// bot modes
#define BOT_PASS 0
#define BOT_LT17 1
#define BOT_LT19 2
#define BOT_MOAR 3

// fault tolerant stuff
#define MAX_STRING_SIZE 1024
#define NSERVERS 2
#define SERVER_WRONG -1
#define SERVER_SLEEPING 0
#define SERVER_WORKING 1
#define SERVER_PORT 40000
#define SYNC_DELAY_SEC 1
#define SYNC_DELAY_USEC 500000
#define MAX_SYNC_DELAY_SEC 6
#define MAX_SYNC_DELAY_USEC 0

// struct for card
struct card {
    int rank;
    int value;
    int suit;
};

// struct for subserver
struct subserv {
    int sd;                       // subserver socket descriptor
    int ind;                      // number (0 or 1)
    int state;                    // free of busy
    int msg;                      // message (needed for the first sendto)
    struct sockaddr_in client;    // client address
    int breaking_news;            // endgame condition flag
    int xmsg;                     // endgame message
    int player_score;             // player's total score
    int pass_flag;                // flag for pass option
};

// struct sent by server to client
// contains card and type of message
struct server_message {
    struct card card;
    int type;
};

// struct for sync msg which is sent
// from the active server to waiting server
struct sync_message {
    struct subserv subservers[MAX_PLAYERS];
    struct card deck[DECK_SIZE];
    int server_mode;
    int server_number;
    int players_count;
};

// server functions
void deck_shuffle(struct card *, int);
void deck_create(struct card *, int);
void deck_print(struct card *, int);
struct card deck_pop_card(struct card *, int);
void serv_print(int);
int handle_client_message(int, struct server_message *, int);
void send_extra_message(int, struct server_message *);
void* subserver(void *);
int server(struct card *);
int cards_server(const char *, int, int);
int server_main(const char *);
int server_log(const char *, ...);

// client functions
int send_to_server(int, int *, struct sockaddr *, unsigned int);
struct server_message recv_from_server(int, struct sockaddr *, unsigned int *);
int client_input(int, int);
int bot_input(int, int, int);
void menu_draw();
int check_game_conditions(struct card *, int, int);
int client(int, int);
int cards_client(int, int, const char *);

//cards_io
void resize_handler();
int player_get_score();
void start_graphic();
void stop_graphic();
void change_game_mode(int, int);
void draw_all();
void player_points_draw(int);
void draw_congratulations(int);
int chose_func_exec(int);
int reading_keystrokes();
void draw_field();
void create_field();
void create_cards_windows();
void buttons_draw(int);
void button_another_game_draw(int);
void button_exit_draw(int);
void button_pass_draw(int);
void button_take_draw(int);
void cursor_draw(int);
void card_draw(struct card, int);
void card_unicode_draw(struct card, int);
void card_win_draw(struct card, int);

#endif
