#include <corenova/source-stub.h>

THIS = {
    .name = "test_pcap",
    .version = "0.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Test pcap traffic capture",
    .requires = LIST("corenova.data.string", "corenova.net.pcap", "corenova.data.list", "corenova.net.resolve", "corenova.sys.signals")
};

#include <corenova/core.h>
#include <corenova/sys/debug.h>
#include <corenova/net/pcap.h>
#include <corenova/data/string.h>
#include <corenova/net/tcp.h>
#include <corenova/net/socket.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>
#include <corenova/net/resolve.h>
#include <corenova/sys/signals.h>

boolean_t sig_rcvd = FALSE;
int pkt_cnt = 0;
int pkt_siz = 0;

void callback(char *ifname, void *data) {

    char *protos[255];
    pcap_accounting_t *acct = (pcap_accounting_t *)data;

    memset(protos, 0, sizeof(protos));

    protos[17] = "UDP";
    protos[6] = "TCP";
    protos[1] = "ICMP";

    char *ssrc = I (Resolve)->ip2string(acct->src.s_addr);
    char *sdst = I (Resolve)->ip2string(acct->dst.s_addr);

    DEBUGP(DINFO, "packet", "%u bytes on %s: %s %s:%hu -> %s:%hu", acct->len, ifname, protos[acct->proto_ip], ssrc, ntohs(acct->sport), sdst, ntohs(acct->dport));
    
    free(ssrc);
    free(sdst);

    pkt_cnt++;
    pkt_siz+=acct->len;

}

void sig_handler() {

    sig_rcvd = TRUE;

}

int main(int argc, char **argv, char **envp) {

    DebugLevel = 6;

    if(argc < 2) {

        DEBUGP(DINFO, "main", "usage: %s [ifname]", argv[0]);
        return 0;

    }

    I (Signal)->handler(SIGINT, sig_handler);

    pcap_instance_t *pcap = I (Pcap)->new(argv[1], callback, CALLBACK_ACCOUNTING);

    if(pcap) {

        I (Pcap)->start(pcap);

    } else {

        return 0;

    }

    while(!sig_rcvd) {

        sleep(1);

    }

    I (Pcap)->stop(pcap);
    I (Pcap)->destroy(&pcap);

    DEBUGP(DINFO, "main", "capture done with %u packets and %u bytes", pkt_cnt, pkt_siz);

    return 0;

}

