#include "cards.h"

/* cards_io.c contains client input and output functions */

extern int            cards_count;
extern int            graphic_mode;
extern int            cursor_coord;
extern int            game_result;
extern struct card   *my_cards;
WINDOW               *working_win;
WINDOW               *card_win[MAX_CARDS_CLIENT];
int                   game_mode;
char                  SCARDS[36][5] =
                      {"🂱", "🃁", "🃑", "🂡",
                       "🂾", "🃎", "🃞", "🂮",
                       "🂽", "🃍", "🃝", "🂭",
                       "🂻", "🃋", "🃛", "🂫",
                       "🂺", "🃊", "🃚", "🂪",
                       "🂹", "🃉", "🃙", "🂩",
                       "🂸", "🃈", "🃘", "🂨",
                       "🂷", "🃇", "🃗", "🂧",
                       "🂶", "🃆", "🃖", "🂦",
                      };

//SIGWINCH handler
void resize_handler(int sig)
{
    int               i;
    int               maxx;
    int               maxy;
    struct winsize    size;

    UNUSED(sig);

    if(ioctl(fileno(stdout), TIOCGWINSZ, &size)) {
        return;
    }

    resize_term(size.ws_row, size.ws_col);

    for(i = 0; i < MAX_CARDS_CLIENT; i++)
        wresize(card_win[i], CARD_SIZE_Y, CARD_SIZE_X);

    maxy = size.ws_row;
    maxx = size.ws_col;
    wclear(working_win);
    wrefresh(working_win);
    refresh();

    if(graphic_mode != 2) {
        if(maxx < 60 || maxy < 20)
            graphic_mode = 1;
        else
            graphic_mode = 0;
    }

    //if card_win[4-7] outside of terminal
    if(maxy < 9)
        graphic_mode = 2;
    draw_all();
}

// Counting the player's scores
int player_get_score()
{
    int    i = 0;
    int    score = 0;

    for(;i < cards_count; i++)
        score += my_cards[i].value;
    return score;
}

/*
change client mode
new_mode:
0 - standart
1 - player pass
2 - game over
client_mode:
0 - player
1 - bot
*/
void change_game_mode(int new_mode, int client_mode)
{
    game_mode = new_mode;
    cursor_coord = 0;
    if (client_mode == M_USR)
        draw_all();
}

// Graphic initialization
void start_graphic()
{
    int    maxx;
    int    maxy;

    // init ncurses screen parameters
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    getmaxyx(stdscr, maxy, maxx);
    if(has_colors()) {
        graphic_mode = 0;
        start_color();
        init_pair(1, COLOR_RED, COLOR_WHITE);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
    }
    if(maxx < 60 || maxy < 20)
        graphic_mode = 1;
    if(maxy < 9)
        graphic_mode = 2;

    game_mode = 0;
    refresh();
    create_field();
    create_cards_windows();
    draw_all();
}

// Graphic deinitialization
void stop_graphic()
{
    int    i = 0;

    for(; i < MAX_CARDS_CLIENT; i++)
        delwin(card_win[i]);
    delwin(working_win);
    delwin(stdscr);
    endwin();

    /*If ncurses configurated with --disable-leaks and --with-valgrind
    this will be free memory*/
    //_nc_freeall();
}

// draw field, cards, buttons, player_score
void draw_all()
{
    int    i;
    int    maxx;
    int    maxy;

    getmaxyx(working_win, maxy, maxx);
    draw_field();

    for(i = 0; i < cards_count; i++)
        card_draw(my_cards[i], i);

    buttons_draw(maxy);

    if(game_mode == 2)
        draw_congratulations(maxx);

    player_points_draw(maxx);
    wrefresh(working_win);
}

/*
maxx - size of window
*/
void player_points_draw(int maxx)
{
    int    player_point = player_get_score();

    if(maxx >= 60)
        wmove(working_win, 1, (CARD_SIZE_X + 2) * 4);
    else
        wmove(working_win, 1, 6 * 4 + 1);

    wprintw(working_win, "%s %d", "Your points:", player_point);
}

/*
maxx - size of window
*/
void draw_congratulations(int maxx)
{
    if(maxx >= 60)
        wmove(working_win, 3, (CARD_SIZE_X + 2) * 4);
    else
        wmove(working_win, 3, 6 * 4 + 1);
    if(game_result == 1)
        wprintw(working_win, "%s", "You win");
    else if (game_result == 0)
        wprintw(working_win, "%s", "You lose");
    else if (game_result == -1)
        wprintw(working_win, "%s", "Yout opponent is disconected");
}

/*
command:
0 - no command
1 - exit
2 - pass
3 - take another card
output - define corresponding to the selected appropriate command
*/
int chose_func_exec(int command)
{
    switch(command) {
    case 1:
        return CLIENT_EXIT;
    case 2:
        return CLIENT_PASS;
    case 3:
        return CLIENT_TAKE;
    }
    return CLIENT_NONE;
}

/*
output - result from chose_func_exec
or CLIENT_NONE if not selected function
*/
int reading_keystrokes()
{
    int    ch;

    ch = getch();

    switch(ch) {
    case KEY_UP:
    case 'w':
        if(!game_mode)
            cursor_coord += cursor_coord < 2 ? 1 : -2;
    break;

    case 's':
    case KEY_DOWN:
        if(!game_mode)
            cursor_coord += cursor_coord > 0 ? -1 : 2;
    break;

    case KEY_ENTER:
    case '\n':
            return chose_func_exec(cursor_coord + 1);

    case ERR:
        return CLIENT_NONE;
    }

    draw_all();
    return CLIENT_NONE;
}

void draw_field()
{
    wclear(working_win);
    box(working_win, 0, 0);
    wrefresh(working_win);
}

void create_field()
{
    working_win = newwin(0, 0, 0, 0);
    draw_field();
}

void create_cards_windows()
{
    int    i;
    int    x;
    int    y;

    for(i = 0; i < MAX_CARDS_CLIENT; i++) {
        y = i < MAX_CARDS_CLIENT / 2 ? 1 : CARD_SIZE_Y+2;
        if(i < MAX_CARDS_CLIENT / 2)
            x = i * (CARD_SIZE_X + 1) + 1;
        else
            x = (i-MAX_CARDS_CLIENT / 2) * (CARD_SIZE_X + 1) + 1;
        card_win[i] = newwin(CARD_SIZE_Y, CARD_SIZE_X, y, x);
    }
}

/*maxy - size of window*/
void buttons_draw(int maxy)
{
    if(!game_mode) {
        button_take_draw(maxy);
        button_pass_draw(maxy);
    }
    button_exit_draw(maxy);
    cursor_draw(maxy-2-cursor_coord);
}

/*maxy - size of window*/
void button_another_game_draw(int maxy)
{
    wmove(working_win, maxy-3, 3);
    wprintw(working_win, "%s", "another game");
}

/*maxy - size of window*/
void button_exit_draw(int maxy)
{
    wmove(working_win, maxy-2, 3);
    wprintw(working_win, "%s", "exit");
}

/*maxy - size of window*/
void button_pass_draw(int maxy)
{
    wmove(working_win, maxy-3, 3);
    if(cards_count < 2) {
        wattron(working_win, A_DIM);
        wprintw(working_win, "%s", "pass");
        wattroff(working_win, A_DIM);
    }
    else
        wprintw(working_win, "%s", "pass");

}

/*maxy - size of window*/
void button_take_draw(int maxy)
{
    wmove(working_win, maxy-4, 3);
    wprintw(working_win, "%s", "take card");
}

/*y - coordinates of cursor*/
void cursor_draw(int y)
{
    wmove(working_win, y, 2);
    waddch(working_win, '>' | A_BOLD);
}

/*
card - a playing card for draw
number_of_card - playing card number from 0 to 7
*/
void card_draw(struct card card, int number_of_card)
{
    if(!graphic_mode)
        card_win_draw(card, number_of_card);
    else
        card_unicode_draw(card, number_of_card);
}

/*
card - a playing card for draw
number_of_card - playing card number from 0 to 7
*/
void card_unicode_draw(struct card card, int number_of_card)
{
    int    x = 0;
    int    y = 1;
    int    card_drawing_number = 0;

    x = number_of_card * 3 + 1;
    wmove(working_win, y, x);
    card_drawing_number = card.suit + card.rank*4;
    wprintw(working_win, "%s ", SCARDS[card_drawing_number]);
    wrefresh(working_win);
}

/*
card - a playing card for draw
number_of_card - playing card number from 0 to 7
*/
void card_win_draw(struct card card, int number_of_card)
{
    if(card.suit == S_HEARTS || card.suit == S_DIAMONDS)
        wbkgd(card_win[number_of_card], COLOR_PAIR(1));
    if(card.suit == S_SPADES || card.suit == S_CLUBS)
        wbkgd(card_win[number_of_card], COLOR_PAIR(2));

    box(card_win[number_of_card], 0, 0);
    wmove(card_win[number_of_card], 1, 1);

    switch(card.rank) {
    case R_ACE:
        waddch(card_win[number_of_card], 'A');
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-2);
        waddch(card_win[number_of_card], 'A');
    break;
    case R_KING:
        waddch(card_win[number_of_card], 'K');
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-2);
        waddch(card_win[number_of_card], 'K');
    break;
    case R_QUEEN:
        waddch(card_win[number_of_card], 'Q');
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-2);
        waddch(card_win[number_of_card], 'Q');
    break;
    case R_JACK:
        waddch(card_win[number_of_card], 'J');
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-2);
        waddch(card_win[number_of_card], 'J');
    break;
    case R_10:
        wprintw(card_win[number_of_card], "%d", 14 - card.rank);
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-3);
        wprintw(card_win[number_of_card], "%d", 14 - card.rank);
    break;
    default:
        wprintw(card_win[number_of_card], "%d", 14 - card.rank);
        wmove(card_win[number_of_card], CARD_SIZE_Y-2, CARD_SIZE_X-2);
        wprintw(card_win[number_of_card], "%d", 14 - card.rank);
    }

    //print card suit
    wmove(card_win[number_of_card], CARD_SIZE_Y/2, CARD_SIZE_X/2-1);

    switch(card.suit) {
    case S_HEARTS:
        wprintw(card_win[number_of_card], "♥");
    break;
    case S_DIAMONDS:
        wprintw(card_win[number_of_card], "♦");
    break;
    case S_SPADES:
        wprintw(card_win[number_of_card], "♠");
    break;
    case S_CLUBS:
        wprintw(card_win[number_of_card], "♣");
    break;
    }
    wrefresh(card_win[number_of_card]);
}
