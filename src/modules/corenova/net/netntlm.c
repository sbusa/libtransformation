
#include <corenova/source-stub.h>

THIS = {
    .version = "1.0",
    .author = "Alex Burkoff <alex.burkoff@gmail.com>",
    .description = "Authenticate using NTLM protocol against AD servers.",
    .requires = LIST("corenova.data.string"),
    .implements = LIST("NetNTLM"),
};

#include <corenova/net/netntlm.h>
#include <corenova/data/string.h>

#define MIN(X, Y) (X < Y ? X : Y)

#define NTLM_BUFSIZE 320

#ifdef WORDS_BIGENDIAN
#define UI16LE(n) bswap_16(n)
#define UI32LE(n) bswap_32(n)
#else
#define UI16LE(n) (n)
#define UI32LE(n) (n)
#endif

static ntlm_handle_t*
_new() {

    ntlm_handle_t *p = calloc(1, sizeof (ntlm_handle_t));


    return p;

}

static void
_destroy(ntlm_handle_t **pPtr) {

    if (pPtr) {

        ntlm_handle_t *p = *pPtr;

        if (p) {

            free(p);
            *pPtr = NULL;

        }

    }

}

static void
_buildRequest(ntlm_handle_t *h, char *user, char *domain) {

    if (h) {

        buildSmbNtlmAuthRequest(&h->request, user, domain);

    }

}

static void
_buildResponse(ntlm_handle_t *h, char *user, char *password) {

    if (h) {

        buildSmbNtlmAuthResponse(&h->challenge, &h->response, user, password);

    }

}

static void
_dumpRequest(ntlm_handle_t *h) {

    if (h) {

        dumpSmbNtlmAuthRequest(stdout, &h->request);

    }

}

static void
_dumpResponse(ntlm_handle_t *h) {

    if (h) {

        dumpSmbNtlmAuthResponse(stdout, &h->response);

    }

}

static void
_dumpChallenge(ntlm_handle_t *h) {

    if (h) {

        dumpSmbNtlmAuthChallenge(stdout, &h->challenge);

    }

}

static uint16
intelEndian16(uint16 n) {
    uint16 u;
    unsigned char *buf = (unsigned char *) &u;
    buf[0] = n & 0xff;
    buf[1] = (n >> 8) & 0xff;
    return u;
}

static uint32
intelEndian32(uint32 n) {
    uint32 u;
    unsigned char *buf = (unsigned char *) &u;
    buf[0] = n & 0xff;
    buf[1] = (n >> 8) & 0xff;
    buf[2] = (n >> 16) & 0xff;
    buf[3] = (n >> 24) & 0xff;
    return u;
}

static void
fillUnicode(tSmbStrHeader * header, char *buffer, int buffer_start,
        int *idx, const char *s) {
    int len = strlen(s);
    header->len = header->maxlen = intelEndian16(len * 2);
    header->offset = intelEndian32(*idx + buffer_start);
    *idx += len * 2;

    for (; len; --len) {
        *buffer++ = *s++;
        *buffer++ = 0;
    }
}

static void
_buildChallenge(ntlm_handle_t *h, char *domain, char *challenge_string) {

    if (h) {

        tSmbNtlmAuthChallenge *challenge = &h->challenge;

        int idx = 0;

        memset(challenge, 0, sizeof (*challenge));
        memcpy(challenge->ident, "NTLMSSP\0\0\0", 8);
        challenge->msgType = intelEndian32(2);
        fillUnicode(&challenge->uDomain, (char *) challenge->buffer, challenge->buffer - ((uint8 *) challenge), &idx, domain);
        challenge->flags = intelEndian32(0);
        memcpy(challenge->challengeData, challenge_string, 8);
        challenge->bufIndex = idx;

    }

}

static char *
_authReqString(ntlm_handle_t *h) {

    if (h) {

        int dst_size = sizeof (h->request)*2;

        char *result = malloc(dst_size);

        I(String)->base64encode((unsigned char*) &h->request, SmbLength(&h->request), (unsigned char*) result, dst_size);

        return result;

    }

    return NULL;

}

static char *
_authRespString(ntlm_handle_t *h) {

    if (h) {

        int dst_size = sizeof (h->response)*2;

        char *result = malloc(dst_size);

        I(String)->base64encode((unsigned char*) &h->response, SmbLength(&h->response), (unsigned char*) result, dst_size);

        return result;

    }

    return NULL;

}

static char *
_authChallengeString(ntlm_handle_t *h) {

    if (h) {

        int dst_size = sizeof (h->response)*2;

        char *result = malloc(dst_size);

        I(String)->base64encode((unsigned char*) &h->challenge, SmbLength(&h->challenge), (unsigned char*) result, dst_size);

        return result;

    }

    return NULL;

}

static int
_decodeNtlmString(ntlm_handle_t *h, char *buf) {

    if (h && buf) {

        int src_size = strlen(buf);
        int dst_size = src_size; // / 1.5;
        unsigned char *tmpbuf = malloc(dst_size);

        dst_size = I(String)->base64decode((unsigned char*) buf, src_size, (unsigned char*) tmpbuf, dst_size);

        int type = tmpbuf[8];

        DEBUGP(DDEBUG, "decode", "message type is %hu", type);

        switch (type) {

            case AUTH_REQUEST:
                memcpy(&h->request, tmpbuf, dst_size);
                h->request.bufIndex = 0;
                break;
            case AUTH_CHALLENGE:
                memcpy(&h->challenge, tmpbuf, dst_size);
                h->challenge.bufIndex = 0;
                break;
            case AUTH_RESPONSE:
                memcpy(&h->response, tmpbuf, dst_size);
                h->response.bufIndex = 0;
                break;

        }

        free(tmpbuf);

        return type;

    }

    return 0;

}

static char *
toString(const char *p, size_t len, char *buf) {

    if (len >= NTLM_BUFSIZE)
        len = NTLM_BUFSIZE - 1;

    memcpy(buf, p, len);
    buf[len] = 0;
    return buf;

}

static inline char *
getString(uint32 offset, uint32 len, char *structPtr, size_t buf_start, size_t buf_len, char *output) {


    if (offset < buf_start || offset > buf_len + buf_start || offset + len > buf_len + buf_start)
        len = 0;

    return toString(structPtr + offset, len, output);

}

static char *
unicodeToString(const char *p, size_t len, char *buf) {
    size_t i;

    if (len >= NTLM_BUFSIZE)
        len = NTLM_BUFSIZE - 1;

    for (i = 0; i < len; ++i) {
        buf[i] = *p & 0x7f;
        p += 2;
    }

    buf[i] = '\0';
    return buf;
}

static inline char *
getUnicodeString(uint32 offset, uint32 len, char *structPtr, size_t buf_start, size_t buf_len, char *output) {

    if (offset < buf_start || offset > buf_len + buf_start || offset + len > buf_len + buf_start)
        len = 0;

    return unicodeToString(structPtr + offset, len / 2, output);

}

static char*
_getResponseUser(ntlm_handle_t *h) {

    if (h) {

        char *user = malloc(UI16LE(h->response.uUser.len) + 1);

        if (h->response.flags & FLAG_NEGOTIATE_UNICODE) {

            getUnicodeString(UI32LE(h->response.uUser.offset),
                    UI16LE(h->response.uUser.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    user);

        } else {

            getString(UI32LE(h->response.uUser.offset),
                    UI16LE(h->response.uUser.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    user);


        }


        return user;

    }

    return NULL;

}

static char*
_getResponseHost(ntlm_handle_t *h) {

    if (h) {

        char *host = malloc(UI16LE(h->response.uWks.len) + 1);

        if (h->response.flags & FLAG_NEGOTIATE_UNICODE) {

            getUnicodeString(UI32LE(h->response.uWks.offset),
                    UI16LE(h->response.uWks.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    host);

        } else {

            getString(UI32LE(h->response.uWks.offset),
                    UI16LE(h->response.uWks.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    host);


        }


        return host;

    }

    return NULL;

}

static char*
_getResponseDomain(ntlm_handle_t *h) {

    if (h) {

        char *dom = malloc(UI16LE(h->response.uDomain.len) + 1);

        if (h->response.flags & FLAG_NEGOTIATE_UNICODE) {

            getUnicodeString(UI32LE(h->response.uDomain.offset),
                    UI16LE(h->response.uDomain.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    dom);

        } else {


            getString(UI32LE(h->response.uDomain.offset),
                    UI16LE(h->response.uDomain.len),
                    (char*) &h->response,
                    (int) &h->response.buffer - (int) &h->response,
                    sizeof (h->response.buffer),
                    dom);
        }

        return dom;

    }

    return NULL;

}

IMPLEMENT_INTERFACE(NetNTLM) = {
    .destroy = _destroy,
    .new = _new,
    .buildAuthRequest = _buildRequest,
    .buildAuthResponse = _buildResponse,
    .dumpAuthRequest = _dumpRequest,
    .dumpAuthResponse = _dumpResponse,
    .dumpAuthChallenge = _dumpChallenge,
    .buildAuthChallenge = _buildChallenge,
    .encodeAuthRequest = _authReqString,
    .encodeAuthResponse = _authRespString,
    .encodeAuthChallenge = _authChallengeString,
    .decode = _decodeNtlmString,
    .getResponseUser = _getResponseUser,
    .getResponseHost = _getResponseHost,
    .getResponseDomain = _getResponseDomain,

};
