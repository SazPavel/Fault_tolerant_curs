#include <assert.h>
#include "cards.h"

void deck_pop_card_test()
{
    struct card deck[5] = {(struct card){R_JACK, 2, 0},
                                (struct card){R_JACK, 2, 1},
                                (struct card){R_JACK, 2, 2},
                                (struct card){R_JACK, 2, 3},
                                (struct card){R_QUEEN, 3, 1}};
    struct card card, tmp;
    card = deck_pop_card(deck, 5);
    tmp = (struct card){R_JACK, 2, 0};
    assert(!memcmp(&card, &tmp, sizeof(card)));
    tmp = (struct card){R_VOID, 2, 0};
    assert(!memcmp(deck + 0, &tmp, sizeof(card)));
    card = deck_pop_card(deck, 5);
    tmp = (struct card){R_JACK, 2, 1};
    assert(!memcmp(&card, &tmp, sizeof(card)));
}

int main()
{
    deck_pop_card_test();
    printf("tests passed\n");
}
