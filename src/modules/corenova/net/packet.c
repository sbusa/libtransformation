#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This provides network packet abstraction",
	.implements  = LIST ("NetPacket")
};

#include <corenova/net/packet.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

static net_packet_t *
newNetworkPacket (char *payload, int size) {
    net_packet_t *packet = (net_packet_t *)calloc (1,sizeof (net_packet_t));
    if (packet) {
        if (size >= sizeof (struct iphdr)) {
            packet->iphdr = (struct iphdr *)payload;
            
            if (packet->iphdr->version == 4) {
                switch (packet->iphdr->protocol) {
                  case IPPROTO_TCP:
                      if (size >= sizeof (struct iphdr) + sizeof (struct tcphdr))
                          packet->tcphdr = (struct tcphdr *) (payload+4*packet->iphdr->ihl);
                    break;
                  case IPPROTO_UDP:
                      if (size >= sizeof (struct iphdr) + sizeof (struct udphdr))
                          packet->udphdr = (struct udphdr *) (payload+4*packet->iphdr->ihl);
                      
                }
            }
        }
        packet->data = payload;
        packet->size = size;
    }
    return packet;
}

static void
destroyNetworkPacket (net_packet_t **ptr) {
    if (ptr) {
        net_packet_t *packet = *ptr;
        if (packet) {
            /* assume that the data inside packet is a pointer which
             * will handle deallocation separately */
            free (packet);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (NetPacket) = {
    .new        = newNetworkPacket,
    .destroy    = destroyNetworkPacket
};
