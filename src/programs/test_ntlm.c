#include <corenova/source-stub.h>

THIS = {
    .name = "ntlm_test",
    .version = "0.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Test NTLM authentication mechanism",
    .requires = LIST("corenova.data.string", "corenova.net.netntlm", "corenova.net.tcp", "corenova.net.socket", "corenova.data.list", "corenova.data.string")
};

#include <corenova/core.h>
#include <corenova/sys/debug.h>
#include <corenova/net/netntlm.h>
#include <corenova/data/string.h>
#include <corenova/net/tcp.h>
#include <corenova/net/socket.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>

#define BUFSIZE 1024
char *getAuthHeader(char *buf);
void makeUnauthorizedResponse(char *buf, char *NTLM);
void makeAuthorizedResponse(char *buf);

int main(int argc, char **argv, char **envp) {

    DebugLevel = 6;


    DEBUGP(DWARN, "main", "to test run wget --user=uusseerr --password=pass http://localhost:9000");
    DEBUGP(DWARN, "main", "or curl --anyauth http://uusseerr:pass@localhost:9000");
    DEBUGP(DWARN, "main", "note that username has to be in UTF16, thus double each letter");

    char *buf = malloc(BUFSIZE);

    socket_t *sock = I(TcpConnector)->listen(9000);
    tcp_t *t = NULL;

    while ((t = I(TcpConnector)->accept(sock)) != NULL) {

        size_t len = 0;
        boolean_t authorized = FALSE;

        while ((len = I(TcpConnector)->read(t, &buf, BUFSIZE)) > 0) {

            buf[len] = 0;

            DEBUGP(DDEBUG, "main", "request:\n%s", buf);

            char *authorization = getAuthHeader(buf);

            if (authorized) {

                makeAuthorizedResponse(buf);

            } else

                if (authorization) {

                ntlm_handle_t *h = I(NetNTLM)->new();

                int type = I(NetNTLM)->decode(h, authorization);

                switch (type) {

                    case AUTH_REQUEST:
                    {

                        char *challenge_string = I(String)->random(8);

                        I(NetNTLM)->dumpAuthRequest(h);
                        I(NetNTLM)->buildAuthChallenge(h, "CPN", challenge_string);

                        unsigned rflags = h->request.flags;
                        unsigned cflags = 0;

                        // default to UNICODE strings

                        if (rflags & FLAG_NEGOTIATE_UNICODE) {

                            cflags |= FLAG_NEGOTIATE_UNICODE;

                        } else if (rflags & FLAG_NEGOTIATE_OEM) {

                            cflags |= FLAG_NEGOTIATE_OEM;

                        } else {

                            cflags |= FLAG_NEGOTIATE_UNICODE;

                        }

                        // use NTLM2 if requested, default to NTLM otherwise

                        if (rflags & FLAG_NEGOTIATE_NTLM2) {

                            cflags |= FLAG_NEGOTIATE_NTLM2;

                        } else
                            if (rflags & FLAG_NEGOTIATE_NTLM) {

                            cflags |= FLAG_NEGOTIATE_NTLM;

                        } else {

                            cflags |= FLAG_NEGOTIATE_NTLM;

                        }

                        // negotiate higher encryption if requested

                        if (rflags & FLAG_NEGOTIATE_128) {

                            cflags |= FLAG_NEGOTIATE_128;

                        } else if (rflags & FLAG_NEGOTIATE_56) {

                            cflags |= FLAG_NEGOTIATE_56;

                        }

                        // this challenge is for a server resource

                        cflags |= FLAG_TARGET_SERVER;

                        h->challenge.flags = cflags;
                        I(NetNTLM)->dumpAuthChallenge(h);

                        makeUnauthorizedResponse(buf, I(NetNTLM)->encodeAuthChallenge(h));
                        free(challenge_string);

                    }
                        break;

                    case AUTH_CHALLENGE:
                        DEBUGP(DDEBUG, "main", "unexpected message type CHALLENGE");
                        makeUnauthorizedResponse(buf, NULL);
                        break;

                    case AUTH_RESPONSE:
                        I(NetNTLM)->dumpAuthResponse(h);
                        authorized = TRUE;
                        makeAuthorizedResponse(buf);

                        char *username = I(NetNTLM)->getResponseUser(h);
                        char *hostname = I(NetNTLM)->getResponseHost(h);
                        char *domain = I(NetNTLM)->getResponseDomain(h);

                        DEBUGP(DDEBUG, "main", "username = '%s'", username);
                        DEBUGP(DDEBUG, "main", "hostname = '%s'", hostname);
                        DEBUGP(DDEBUG, "main", "domain = '%s'", domain);

                        free(username);
                        free(hostname);
                        free(domain);

                        break;

                }

            } else {

                makeUnauthorizedResponse(buf, NULL);

            }

            DEBUGP(DDEBUG, "main", "response:\n%s", buf);

            I(TcpConnector)->write(t, buf, strlen(buf));

        }

        I(TcpConnector)->destroy(&t);

    }

    I(Socket)->destroy(&sock);


    return 0;

}

void makeUnauthorizedResponse(char *buf, char *NTLM) {

    strcpy(buf, "HTTP/1.1 401 Unauthorized\r\n");
    strcpy(buf + strlen(buf), "Content-length: 0\r\n");
    strcpy(buf + strlen(buf), "WWW-Authenticate: NTLM");

    if (NTLM) {

        strcpy(buf + strlen(buf), " ");
        strcpy(buf + strlen(buf), NTLM);
        strcpy(buf + strlen(buf), "\r\n");

    } else {

        strcpy(buf + strlen(buf), "\r\n");

    }

    strcpy(buf + strlen(buf), "Connection: Keep-Alive\r\n");
    strcpy(buf + strlen(buf), "\r\n");

}

void makeAuthorizedResponse(char *buf) {

    char *content = "<html><head><title>authorization successful</title></head><body>hello, world</body></html>\r\n";
    char content_length[10];

    sprintf(content_length, "%u", strlen(content));

    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    strcpy(buf + strlen(buf), "Connection: Keep-Alive\r\n");
    strcpy(buf + strlen(buf), "Content-length: ");
    strcpy(buf + strlen(buf), content_length);
    strcpy(buf + strlen(buf), "\r\n");
    strcpy(buf + strlen(buf), "\r\n");
    strcpy(buf + strlen(buf), "<html><head><title>authorization successful</title></head><body>hello, world</body></html>\r\n");

}

char *getAuthHeader(char *buf) {

    char *authorization = NULL;


    list_t *lines = I(String)->tokenize(buf, "\r\n");

    if (I(List)->count(lines) < 1) {

        return NULL;

    }

    list_iterator_t *iter = I(ListIterator)->new(lines);
    list_item_t *item = NULL;


    while ((item = I(ListIterator)->getItem(iter)) != NULL) {

        list_t *entry = I(String)->tokenize((char *) item->data, ":");

        if (I(List)->count(entry) == 2) {

            char *key = (char *) I(List)->first(entry)->data;
            char *val = I(String)->trim((char *) I(List)->last(entry)->data);

            if (I(String)->equal(key, "Authorization")) {

                //     DEBUGP(DDEBUG, "getAuthHeader", "%s", val);

                if (I(String)->first(val, "NTLM ")) {

                    authorization = I(String)->copy(val + 5);

                }

            }

        }

        I(List)->clear(entry, TRUE);
        I(List)->destroy(&entry);

        I(ListIterator)->next(iter);

    }

    I(ListIterator)->destroy(&iter);
    I(List)->clear(lines, TRUE);
    I(List)->destroy(&lines);

    return authorization;

}
