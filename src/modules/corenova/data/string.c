#include <corenova/source-stub.h>

THIS = {
    .version = "2.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module provides a set of string operations.",
    .implements = LIST("String"),
    .requires = LIST("corenova.data.list", "corenova.data.md5")
};

#include <corenova/data/list.h>
#include <corenova/data/md5.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/
#include <stdarg.h>

#define CRC32C(c,d) (c=(c>>8)^crc_c[(c^(d))&0xFF])

static const unsigned int crc_c[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

static uint32_t
_crc32(const char *buffer, uint32_t length) {

    uint32_t i;
    uint32_t crc32 = ~0L;

    for (i = 0; i < length; i++) {
        CRC32C(crc32, (unsigned char) buffer[i]);
    }

    return ~crc32;

}

static char *
newString(const char *format, ...) {
    va_list ap;
    char buf[STRING_MAXLEN];
    char *copy = NULL;

    va_start(ap, format);
    vsnprintf(buf, sizeof (buf), format, ap);
    va_end(ap);

    copy = I(String)->copy(buf);

    return copy;
}

static char *
newRandomString(uint32_t len) {

    static char buf[STRING_MAXLEN + 1];
    const char *chars = "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789";
    uint32_t max = strlen(chars);
    uint32_t i = 0;
    struct timeval tv;
    char *copy = NULL;

    MODULE_LOCK();

    /* use time value as seed */
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec * tv.tv_usec);

    /* set the max length */
    if (len > STRING_MAXLEN)
        len = STRING_MAXLEN;

    for (; i < len; ++i)
        buf[ i ] = chars[ rand() % max ];

    buf[ i ] = '\0';

    copy = I(String)->copy(buf);
    MODULE_UNLOCK();

    return copy;
}

static int32_t
_iswhite(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
            return TRUE;
    }
    return FALSE;
}

/* --- */

static char *
_skipwhite(const char *string) {
    /* skip whitespace */
    while ((*string != '\0') && (_iswhite(*string))) {
        string++;
    }
    return (char *) string;
}

static char *
_rskipwhite(const char *string) {
    char *end_ptr = (char *) string + strlen(string);
    while ((--end_ptr > string) && (_iswhite(*end_ptr))) {
        *end_ptr = '\0';
    }
    return (char *) string;
}

static char *
_trim(const char *string) {
    string = _skipwhite(string);
    string = _rskipwhite(string);
    return (char *) string;
}

static ssize_t
_join(char **to, const char *with) {
    size_t orig_len = 0, with_len = strlen(with);
    if (!with_len) return 0;
    if (*to) orig_len = strlen(*to);
    *to = realloc(*to, sizeof (char) *(orig_len + with_len + 2));
    if (!*to) return 0;
    memcpy(&(*to)[orig_len], with, sizeof (char) *(with_len + 1));
    return (orig_len + with_len + 1);
}

static boolean_t
equalString(const char *str1, const char *str2) {
    if (str1 && str2) {
        size_t len1 = strlen(str1);
        size_t len2 = strlen(str2);
        if ((len1 == len2) && !strncasecmp(str1, str2, len1))
            return TRUE;
    }
    return FALSE;
}

static boolean_t
_equalWild(char wild, const char *left, const char *right) {
    if (*left == wild) {
        char *rstr = (char *) right;
        /* find the occurances on right string that has left+1 */
        while ((rstr = strchr(rstr, *(left + 1)))) {
            if (_equalWild(wild, left + 1, rstr))
                return TRUE; /* yay! */
            rstr++; /* next possible match */
        }
    }
    if (*right == wild) {
        char *lstr = (char *) left;
        /* find the occurances on left string that has right+1 */
        while ((lstr = strchr(lstr, *(right + 1)))) {
            if (_equalWild(wild, lstr, right + 1))
                return TRUE; /* yay! */
            lstr++; /* next possible match */
        }
    }
    if (*left != *right) return FALSE; /* nada */

    /* implicit *left == *right */
    if (*left == '\0') return TRUE; /* yay! */
    return _equalWild(wild, left + 1, right + 1);

}

static boolean_t
equalWildString(const char *str1, const char *str2) {
    return _equalWild('*', str1, str2);
}

static boolean_t
equalWildWith(char with, const char *str1, const char *str2) {
    return _equalWild(with, str1, str2);
}

static char *
copyString(const char *orig) {
    if (orig)
        return strdup(orig);
    return NULL;
}

static list_t *
tokenizeString(const char *string, const char *delim) {
    if (string && delim) {
        list_t *list = I(List)->new();
        if (list) {
            char *stringCopy = strdup(string);
            char *p, *oldp;
            p = stringCopy;
            while ((oldp = strsep(&p, delim))) {
                I(List)->insert(list, I(ListItem)->new(strdup(oldp)));
            }
            free(stringCopy);
            return list;
        }
    }
    return NULL;
}

static char *
itoa2(int64_t value) {

    register char *p;
    register int32_t minus;
    static char buf[36];
    char *yo;

    MODULE_LOCK();
    p = &buf[36];
    *--p = '\0';

    if (value < 0) {
        minus = 1;
        value = -value;
    } else
        minus = 0;

    if (value == 0)
        *--p = '0';
    else
        while (value > 0) {
            *--p = "0123456789"[value % 10];
            value /= 10;
        }

    if (minus)
        *--p = '-';

    yo = strdup(p);
    MODULE_UNLOCK();

    return yo;
}

static char *
utoa2(uint64_t value) {

    register char *p;
    static char buf[36];
    char *yo = NULL;

    MODULE_LOCK();
    p = &buf[36];
    *--p = '\0';

    if (value == 0)
        *--p = '0';
    else
        while (value > 0) {
            *--p = "0123456789"[value % 10];
            value /= 10;
        }
    yo = strdup(p);
    MODULE_UNLOCK();

    return yo;
}

static md5_t *
_md5(const char *string) {
    md5_t *digest = NULL;
    if (string) {
        md5_ctx_t *context = I(MD5Transform)->new();
        if (context) {
            I(MD5Transform)->update(context, (unsigned char *) string, strlen(string));
            digest = I(MD5Transform)->final(context);
            I(MD5Transform)->destroy(&context);
        }
    }
    return digest;
}

static char *
firstInString(const char *string, const char *match) {
    if (string && match) {
        return strstr(string, match);
    }
    return NULL;
}

static char *
lastInString(const char *string, const char *match) {
    if (string && match) {
        char *ptr = (char *) string, *last = NULL;
        while ((ptr = strstr(ptr, match))) {
            last = ptr;
            ptr += strlen(match);
        }
        return last;
    }
    return NULL;
}

static char *
translateString(const char *string, const char match, const char with) {
    if (string && match && with) {
        char *string2 = I(String)->copy((char *) string);
        char *copy = string2;
        char *pos = NULL;
        while ((pos = strchr(string2, match))) {
            *pos = with;
            string2 = pos;
        }
        return copy;
    }
    return NULL;
}

static int
base64decode(unsigned char *src, int src_len, unsigned char *dst, int dst_len) {

    int idx = 0;

    unsigned char table[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
        '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    unsigned char rtable[255];

    int i = 0;

    for (i = 0; i <= 63; i++) {

        rtable[table[i]] = i;

    }

    for (i = 0; i < src_len / 4; i++) {

        if (idx + 3 > dst_len) {

            return idx;

        }

        unsigned char a = src[i * 4],
                b = src[i * 4 + 1],
                c = src[i * 4 + 2],
                d = src[i * 4 + 3];

        dst[idx++] = rtable[a] << 2 | rtable[b] >> 4;

        if (c != '=') {

            dst[idx++] = (rtable[b] << 4 & 0xff) | (rtable[c] >> 2);

        }

        if (c != '=' && d != '=') {

            dst[idx++] = (rtable[c] << 6 & 0xff) | rtable[d];

        }

    }

    return idx;

}

static int
base64encode(unsigned char *src, int src_len, unsigned char *dst, int dst_len) {

    int idx = 0;
    unsigned char rtable[255];
    unsigned char table[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
        '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    int i = 0;

    for (i = 0; i <= 63; i++) {

        rtable[table[i]] = i;

    }


    for (i = 0; i < src_len / 3; i++) {

        if (idx + 4 >= dst_len) {

            return idx;

        }

        unsigned char a = src[i * 3] >> 2,
                b = ((src[i * 3] & 0x03) << 4) | (src[i * 3 + 1] >> 4),
                c = ((src[i * 3 + 1] & 0x0f) << 2) | (src[i * 3 + 2] >> 6),
                d = src[i * 3 + 2] & 0x3f;

        dst[idx++] = table[a];
        dst[idx++] = table[b];
        dst[idx++] = table[c];
        dst[idx++] = table[d];

    }

    if (src_len % 3 == 1) {

        unsigned char a = src[src_len - 1] >> 2,
                b = ((src[src_len - 1] & 0x03) << 4);

        dst[idx++] = table[a];
        dst[idx++] = table[b];
        dst[idx++] = '=';
        dst[idx++] = '=';

    }

    if (src_len % 3 == 2) {

        unsigned char a = src[src_len - 2] >> 2,
                b = ((src[src_len - 2] & 0x03) << 4) | (src[src_len - 1] >> 4),
                c = ((src[src_len - 1] & 0x0f) << 2);

        dst[idx++] = table[a];
        dst[idx++] = table[b];
        dst[idx++] = table[c];
        dst[idx++] = '=';

    }

    dst[idx]='\0';

    return idx;

}

IMPLEMENT_INTERFACE(String) = {
    .new = newString,
    .random = newRandomString,
    .skipwhite = _skipwhite,
    .rskipwhite = _rskipwhite,
    .trim = _trim,
    .join = _join,
    .copy = copyString,
    .equal = equalString,
    .equalWild = equalWildString,
    .equalWildWith = equalWildWith,
    .itoa2 = itoa2,
    .utoa2 = utoa2,
    .tokenize = tokenizeString,
    .md5 = _md5,
    .crc32 = _crc32,
    .first = firstInString,
    .last = lastInString,
    .translate = translateString,
    .base64encode = base64encode,
    .base64decode = base64decode
};
