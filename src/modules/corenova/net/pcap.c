
#include <corenova/source-stub.h>

THIS = {
    .version = "1.0",
    .author = "Alex Burkoff <saint@corenova.com>",
    .description = "Collect and report packets via PCAP",
    .implements = LIST("Pcap"),
    .requires = LIST("corenova.data.object",
    "corenova.data.parameters",
    "corenova.data.string",
    "corenova.data.list",
    "corenova.sys.quark")

};

#include <corenova/net/pcap.h>

#define SLL_ADDRLEN     8       /* length of address field */

struct sll_header {
    u_int16_t sll_pkttype; /* packet type */
    u_int16_t sll_hatype; /* link-layer address type */
    u_int16_t sll_halen; /* link-layer address length */
    u_int8_t sll_addr[SLL_ADDRLEN]; /* link-layer address */
    u_int16_t sll_protocol; /* protocol */
};


static boolean_t pcapLoop(void *descr);

unsigned short
l3checksum(struct ip *ip) {

    uint32_t sum = 0;
    unsigned short len = ip->ip_hl * 4;

    unsigned short *p = (unsigned short*) ip;

    for (; len > 1; len -= 2)
        sum += *p++;

    if (len) /* take care of left over byte */
        sum += (unsigned short) *(unsigned char *) p;

    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16); /* add carry */

    return ~sum;

}

unsigned short
l4checksum(struct ip *ip) {

    unsigned short len = ntohs(ip->ip_len) - ip->ip_hl * 4;

    union {

        struct phd {
            uint32_t src, dst;
            unsigned char zero, proto;
            unsigned short length;
        } ph;
        unsigned short pa[6];
    } ph;

    ph.ph.src = ip->ip_src.s_addr;
    ph.ph.dst = ip->ip_dst.s_addr;
    ph.ph.zero = 0;
    ph.ph.proto = ip->ip_p;
    ph.ph.length = htons(len);

    unsigned short *p = ph.pa;
    uint32_t sum = p[0] + p[1] + p[2] + p[3] + p[4] + p[5];

    p = (unsigned short*) (((void *) ip) + ip->ip_hl * 4);

    for (; len > 1; len -= 2)
        sum += *p++;

    if (len) /* take care of left over byte */
        sum += (unsigned short) *(unsigned char *) p;

    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16); /* add carry */

    return ~sum;

}

static void
pcapPacketProcessor(u_char *param, const struct pcap_pkthdr* pkthdr, const u_char * packet) {

    struct ether_header *eth_hdr = (struct ether_header*) packet;
    struct ip *ip_hdr = (struct ip*) (packet + sizeof (struct ether_header));
    struct tcphdr *tcp_hdr = NULL;
    struct udphdr *udp_hdr = NULL;
    pcap_instance_t *instance = (pcap_instance_t*) param;

    unsigned short ip_csum = 0, tcp_csum = 0, udp_csum = 0, checksum = 0;

    if (instance->link_type == DLT_NULL) {

        // by default DLT_NULL is prepended with address family

        ip_hdr = (struct ip*) (packet + 4);

    }

    if (instance->link_type == DLT_LINUX_SLL) {


        ip_hdr = (struct ip*) (packet + sizeof ( struct sll_header));

    }

    switch (instance->callback_type) {

        case CALLBACK_PCAP_RAW:
        {

            pcap_raw_t data = {.len = pkthdr->len, .link_type = instance->link_type, .data = (char*) packet};

            instance->callback(instance->ifname, (void *) &data);

        }

            break;


        case CALLBACK_RAW:
        {

            pcap_raw_t data = {.len = ntohs(ip_hdr->ip_len), .data = (char*) ip_hdr};

            instance->callback(instance->ifname, (void *) &data);

        }

            break;

        case CALLBACK_IPACCOUNTING:
        case CALLBACK_ACCOUNTING:
        {

            pcap_accounting_t data;

            if ((instance->link_type == DLT_EN10MB && ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) || instance->link_type == DLT_NULL || instance->link_type == DLT_LINUX_SLL) {

                ip_csum = ip_hdr->ip_sum;
                ip_hdr->ip_sum = 0;

                checksum = l3checksum(ip_hdr);

                if (checksum == ip_csum) {

                    data.src = ip_hdr->ip_src;
                    data.dst = ip_hdr->ip_dst;

                    if (instance->callback_type == CALLBACK_IPACCOUNTING) {

                        data.len = ntohs(ip_hdr->ip_len);

                    } else {

                        data.len = pkthdr->len;

                    }

                    data.proto_ip = 0;
                    data.sport = 0;
                    data.dport = 0;

                    switch (ip_hdr->ip_p) {

                        case IPPROTO_TCP:

                            tcp_hdr = (struct tcphdr*) (((void *) ip_hdr) + ip_hdr->ip_hl * 4);
                            tcp_csum = tcp_hdr->th_sum;
                            tcp_hdr->th_sum = 0;

                            checksum = l4checksum(ip_hdr);

                            if (checksum == tcp_csum) {

                                data.proto_ip = IPPROTO_TCP;
                                data.sport = tcp_hdr->th_sport;
                                data.dport = tcp_hdr->th_dport;

                            }

                            instance->callback(instance->ifname, (void*) &data);
                            break;

                        case IPPROTO_UDP:

                            udp_hdr = (struct udphdr*) (((void *) ip_hdr) + ip_hdr->ip_hl * 4);
                            udp_csum = udp_hdr->uh_sum;
                            udp_hdr->uh_sum = 0;

                            checksum = l4checksum(ip_hdr);


                            if (checksum == udp_csum) {

                                data.proto_ip = IPPROTO_UDP;
                                data.sport = udp_hdr->uh_sport;
                                data.dport = udp_hdr->uh_dport;

                            }

                            instance->callback(instance->ifname, (void*) &data);
                            break;

                        default:

                            data.proto_ip = ip_hdr->ip_p;
                            instance->callback(instance->ifname, (void*) &data);
                            break;

                    }

                } else {

                    DEBUGP(DDEBUG, "pcapPacketProcessor", "bad IP csum");

                }

            } else {

//                DEBUGP(DDEBUG, "pcapPacketProcessor", "unhandled packet type %hx", ntohs(eth_hdr->ether_type));

            }

        }
    }
}

static boolean_t
pcapLoop(void *descr) {

    pcap_instance_t *instance = (pcap_instance_t *) descr;

    instance->active = TRUE;

    pcap_loop(instance->descr, -1, pcapPacketProcessor, (u_char*) instance);

    instance->active = FALSE;

    DEBUGP(DINFO, "pcapLoop", "finished on %s", instance->ifname);

    return FALSE;

}

static pcap_instance_t *
newPcap(char *ifname, void (*callback)(char *, void *data), int callback_type) {

    pcap_instance_t *instance;

    instance = malloc(sizeof (pcap_instance_t));

    instance->ifname = strdup(ifname);

    instance->descr = pcap_open_live(instance->ifname, BUFSIZ, 0, 1, instance->errbuf);
    instance->quark = NULL;
    instance->active = FALSE;
    instance->callback = callback;
    instance->callback_type = callback_type;

    if (!instance->descr) {

        DEBUGP(DFATAL, "newPcap", "pcap_open_live(): %s", instance->errbuf);
        I(Pcap)->destroy(&instance);
        return NULL;

    }

    instance->link_type = pcap_datalink(instance->descr);

    DEBUGP(DDEBUG, "newPcap", "link type is %i", instance->link_type);

    instance->quark = I(Quark)->new(pcapLoop, instance);

    I(Quark)->setname(instance->quark, "pcapQuark");

    return instance;

}

static void filterPcap(pcap_instance_t *instance, char *filter) {

    static struct bpf_program bpf_compiled_filter;

    if (pcap_compile(instance->descr, &bpf_compiled_filter, filter, 0, 0) != -1)
        pcap_setfilter(instance->descr, &bpf_compiled_filter);
    else
        DEBUGP(DERR, "filter", "pcap filter failed to compile : %s", pcap_geterr(instance->descr));


}

static boolean_t
startPcap(pcap_instance_t * instance) {

    if (!instance->active && instance->quark) {

        I(Quark)->spin(instance->quark);
        return instance->active;

    }

    return FALSE;

}

static boolean_t
stopPcap(pcap_instance_t * instance) {

    if (instance->active) {

        pcap_breakloop(instance->descr);

        while (instance->active) {

            usleep(500);

        }

        pcap_close(instance->descr);

        return TRUE;

    }

    return FALSE;

}

static void
destroyPcap(pcap_instance_t **instance) {

    if (instance && *instance) {

        pcap_instance_t *p = *instance;

        if (p->ifname) {

            free(p->ifname);

        }

        if (p->quark) {

            I(Quark)->destroy(&p->quark);

        }

        free(p);
        p = NULL;
    }

}

IMPLEMENT_INTERFACE(Pcap) = {
    .new = newPcap,
    .start = startPcap,
    .stop = stopPcap,
    .filter = filterPcap,
    .destroy = destroyPcap
};

