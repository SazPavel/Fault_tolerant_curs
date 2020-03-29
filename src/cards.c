#include "cards.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cards mode \n");
        printf("Modes: server [IPaddr]\n");
        printf("       client [IPaddr]\n");
        printf("       safeserver [IPaddr]\n");
        printf("       bot [mode] [IPaddr]\n");
        printf("bot modes:\n");
        printf("\t0 - always pass\n");
        printf("\t1 - pass if score < 17\n");
        printf("\t2 - pass if score < 19\n");
        printf("\t3 - never pass\n");
        exit(-1);
    }

    if (!strcmp(argv[1], "safeserver"))
        server_main((argc > 2) ? argv[2] : "127.0.0.1");
    if (!strcmp(argv[1], "server"))
        cards_server((argc > 2) ? argv[2] : "127.0.0.1", SERVER_WRONG, 0);
    if (!strcmp(argv[1], "client"))
    {
        cards_client(M_USR, 0, (argc > 2) ? argv[2] : "127.0.0.1");
    }
    if (!strcmp(argv[1], "bot"))
        cards_client(M_BOT, (argc > 2) ? atoi(argv[2]) : BOT_PASS,
                     (argc > 3) ? argv[3] : "127.0.0.1");
    exit(0);
}
