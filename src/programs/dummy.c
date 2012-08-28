#include <corenova/source-stub.h>

THIS = {
    .name = "A dummy Tester",
    .version = "0.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This program should do nothing.",
    .requires = LIST("corenova.data.string", "corenova.net.neticmp")
    //	.requires = LIST ("corenova.data.database", "corenova.data.parameters", "corenova.data.streams" )
};

#include <corenova/sys/getopts.h>
#include <corenova/sys/debug.h>
#include <corenova/data/stree.h>
#include <corenova/data/streams.h>
#include <corenova/data/string.h>
#include <corenova/data/object.h>
#include <corenova/data/database.h>
#include <corenova/net/neticmp.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv, char **envp) {

    uint32_t ip = inet_addr("4.2.2.2");
    //uint32_t bind_ip = inet_addr("10.10.10.1");

    neticmp_t *p = I (NetICMP)->new(3000);

    //I (NetICMP)->bind(p, bind_ip);

    int i = 0;

    if(!p) {

        DEBUGP(DWARN, "pingNode", "init failed");

    }


    for(i = 0; i < 15; i++) {

        if(!I (NetICMP)->ping(p, ip))
            DEBUGP(DWARN, "pingNode", "ping() failed");

        if(I (NetICMP)->pong(p, ip)) {

            usleep(200000);

        }
    }

    DEBUGP(DDEBUG, "main", "received %u", p ? p->nrecv : 0);

    I (NetICMP)->destroy(&p);


    return 0;

}

