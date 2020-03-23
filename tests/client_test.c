#include <assert.h>
#include "cards.h"

void check_game_conditions_test()
{
    struct card not_enough_cards[2] = {(struct card){R_10, 10, 0},
                                      (struct card){R_10, 10, 1}};
    assert(check_game_conditions(not_enough_cards, 2, 20) == CLIENT_NONE);
    struct card five_pics[5] = {(struct card){R_JACK, 2, 0},
                                (struct card){R_JACK, 2, 1},
                                (struct card){R_JACK, 2, 2},
                                (struct card){R_JACK, 2, 3},
                                (struct card){R_QUEEN, 3, 1}};
    assert(check_game_conditions(five_pics, 5, 11) == CLIENT_WIN);
    struct card not_five_pics[5] = {(struct card){R_JACK, 2, 0},
                                    (struct card){R_JACK, 2, 1},
                                    (struct card){R_JACK, 2, 2},
                                    (struct card){R_JACK, 2, 3},
                                    (struct card){R_6, 6, 1}};
    assert(check_game_conditions(not_five_pics, 5, 14) == CLIENT_NONE);
    struct card two_aces[2] = {(struct card){R_ACE, 11, 0},
                               (struct card){R_ACE, 11, 3}};
    assert(check_game_conditions(two_aces, 2, 22) == CLIENT_WIN);
    struct card exactly_21[3] = {(struct card){R_6, 6, 1},
                                 (struct card){R_7, 7, 2},
                                 (struct card){R_8, 8, 3}};
    assert(check_game_conditions(exactly_21, 3, 21) == CLIENT_WIN);
    struct card too_much[3] = {(struct card){R_6, 6, 1},
                                 (struct card){R_7, 7, 2},
                                 (struct card){R_9, 9, 3}};
    assert(check_game_conditions(too_much, 3, 22) == CLIENT_LOSE);
}
    extern int cards_count;

void bot_input_test()
{
    cards_count = 1;
    int i;

    for (i = BOT_PASS; i <= BOT_MOAR; i++)
        assert(bot_input(0, 11, i) == CLIENT_TAKE);

    cards_count = 2;
    assert(bot_input(0, 15, BOT_PASS) == CLIENT_PASS);
    assert(bot_input(0, 15, BOT_LT17) == CLIENT_TAKE);
    assert(bot_input(0, 15, BOT_LT19) == CLIENT_TAKE);
    assert(bot_input(0, 15, BOT_MOAR) == CLIENT_TAKE);
    assert(bot_input(0, 17, BOT_LT17) == CLIENT_PASS);
    assert(bot_input(0, 19, BOT_LT17) == CLIENT_PASS);

    for (i = BOT_PASS; i <= BOT_MOAR; i++)
        assert(bot_input(1, 15, i) == CLIENT_NONE);
}

int main()
{
    check_game_conditions_test();
    bot_input_test();
    printf("tests passed\n");
}

