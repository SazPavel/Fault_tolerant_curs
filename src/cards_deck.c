#include "cards.h"

/* cards_deck.c contains server functions for card deck manipulation */

// Fisher-Yates shuffle algorithm
void deck_shuffle(struct card *deck, int n)
{
    struct card    tmp;
    int            i;
    int            j;

    for (i = n - 1; i > 0; i--) {
        j = rand() % (i + 1);
        tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
}

// create the deck of playing cards
void deck_create(struct card *deck, int n)
{
    int    i;

    for (i = 0; i < n; i++) {
        deck[i].rank = i / 4;
        deck[i].suit = i % 4;

        // each rank has it's own value
        switch(deck[i].rank) {
            case R_ACE:
                deck[i].value = 11;
            break;
            case R_KING:
                deck[i].value = 4;
            break;
            case R_QUEEN:
                deck[i].value = 3;
            break;
            case R_JACK:
                deck[i].value = 2;
            break;
            default:
                deck[i].value = 14 - deck[i].rank;
        }
    }
}

// print all parameters of all cards
void deck_print(struct card *deck, int n)
{
    int     i;
    char    ranks[10][4] = {"A", "K", "Q", "J", \
                            "10", "9", "8", "7",\
                            "6", "NIL"};
    char    suits[4][4] = {"♥", "♦", "♣", "♠"};

    for (i = 0; i < n; i++)
#ifdef PRINT_SCREEN
        server_log("%s%2d %4s %4s  (%2d)%s\n",
                   (deck[i].suit) < 2 ? CRED : CNRM, i,
                   ranks[deck[i].rank], suits[deck[i].suit],
                   deck[i].value, CNRM);
#else
        server_log("%2d %4s %4s (%2d)\n",
                   i, ranks[deck[i].rank],
                   suits[deck[i].suit], deck[i].value);
#endif
}

// give a card to the player and remove it from the deck
// R_VOID means there is no card
struct card deck_pop_card(struct card *deck, int n)
{
    struct card    ret;
    int            i;

    for (i = 0; i < n && deck[i].rank == R_VOID; i++);
    ret = deck[i];
    deck[i].rank = R_VOID;
    return ret;
}
