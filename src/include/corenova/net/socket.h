#ifndef __net_socket_H__
#define __net_socket_H__

#include <corenova/interface.h>

#include <netinet/in.h>
#include <sys/un.h>

#ifndef IFNAMSIZ
# define IFNAMSIZ 16
#endif

enum _socketTypes {  SOCKET_DUMMY, SOCKET_DGRAM, SOCKET_NETLINK, SOCKET_RAW, SOCKET_STREAM, SOCKET_UNIX };
const char *_socketDescr[] = { "DUMMY", "DATAGRAM", "NETLINK", "RAW", "STREAM", "UNIX" };

enum _optTypes { BCAST };

typedef struct {

	int32_t skd;
	enum _socketTypes type;
	unsigned char flags;
#    define SOCKET_LISTEN_FLAG  0x1
#    define SOCKET_ACCEPT_FLAG  0x2
#	 define SOCKET_CONNECT_FLAG 0x4

    socklen_t len;
    union {
        struct sockaddr_in in;
        struct sockaddr_un un;
    } addr;
	
} socket_t;

DEFINE_INTERFACE (Socket) {

    socket_t * (*new)           (enum _socketTypes type);
	void       (*destroy)       (socket_t **);
	boolean_t  (*bindInterface) (socket_t *, const char *ifname);
        boolean_t (*bind)           (socket_t *sock, struct sockaddr* addr);
	boolean_t  (*setFlag)       (socket_t *, const char *ifname, short flag);
	boolean_t  (*setOpt)       (socket_t *, int opt);
	boolean_t  (*clearFlag)     (socket_t *, const char *ifname, short flag);
	boolean_t  (*verifyArpType) (socket_t *, const char *ifname, u_int16_t type);
    boolean_t  (*setAddress)    (socket_t *, const char *to, int port);
    
};

typedef struct {

    int       fds[2];
    boolean_t closed;
    
} socket_pair_t;

DEFINE_INTERFACE (SocketPair) {
    
    socket_pair_t *(*new)     (void);
    void           (*close)   (socket_pair_t *);
    void           (*destroy) (socket_pair_t **);
    
};

// special section for some Endian magic functions...

inline uint32_t swapel(uint32_t a) {
    uint32_t b = 0;
    char *bp = (char *) &b;
    char *ap = (char *) &a;
    bp[0] = ap[3];
    bp[1] = ap[2];
    bp[2] = ap[1];
    bp[3] = ap[0];
    return b;
}

inline uint32_t htolel(uint32_t a) {
    uint32_t magic = 1;
    char *mp = (char *) &magic;
    if(mp[3] == 1) {     // big-endian 
        DEBUGP (DDEBUG,"htolel","converting from big-endian to little-endian")
        return swapel(a);
    }
    return a;
}

inline uint32_t ltohel(uint32_t a) {
    uint32_t magic = 1;
    char *mp = (char *) &magic;
    if(mp[3] == 1) {     // big-endian
        DEBUGP (DDEBUG,"htolel","converting from little-endian to big-endian")
        return swapel(a);
    }
    return a;
}

#endif
