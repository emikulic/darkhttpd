/* darkhttpd - a simple, single-threaded, static content webserver.
 * http://unix4lyfe.org/darkhttpd/
 * Copyright (c) 2003-2013 Emil Mikulic <emikulic@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

static const char
    pkgname[]   = "darkhttpd/1.8",
    copyright[] = "copyright (c) 2003-2013 Emil Mikulic";

#ifndef DEBUG
# define NDEBUG
static const int debug = 0;
#else
static const int debug = 1;
#endif

#ifdef __linux
# define _GNU_SOURCE /* for strsignal() and vasprintf() */
# define _FILE_OFFSET_BITS 64 /* stat() files bigger than 2GB */
# include <sys/sendfile.h>
#endif

#ifdef __sun__
# include <sys/sendfile.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __sun__
# ifndef INADDR_NONE
#  define INADDR_NONE -1
# endif
#endif

#ifndef MAXNAMLEN
# ifdef NAME_MAX
#  define MAXNAMLEN NAME_MAX
# else
#  define MAXNAMLEN   255
# endif
#endif

#if defined(O_EXCL) && !defined(O_EXLOCK)
# define O_EXLOCK O_EXCL
#endif

#ifndef __printflike
# ifdef __GNUC__
/* [->] borrowed from FreeBSD's src/sys/sys/cdefs.h,v 1.102.2.2.2.1 */
#  define __printflike(fmtarg, firstvararg) \
             __attribute__((__format__(__printf__, fmtarg, firstvararg)))
/* [<-] */
# else
#  define __printflike(fmtarg, firstvararg)
# endif
#endif

/* [->] borrowed from FreeBSD's src/sys/sys/systm.h,v 1.276.2.7.4.1 */
#ifndef CTASSERT                /* Allow lint to override */
# define CTASSERT(x)             _CTASSERT(x, __LINE__)
# define _CTASSERT(x, y)         __CTASSERT(x, y)
# define __CTASSERT(x, y)        typedef char __assert ## y[(x) ? 1 : -1]
#endif
/* [<-] */

CTASSERT(sizeof(unsigned long long) >= sizeof(off_t));
#define llu(x) ((unsigned long long)(x))

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__linux)
# include <err.h>
#else
/* err - prints "error: format: strerror(errno)" to stderr and exit()s with
 * the given code.
 */
static void err(const int code, const char *format, ...) __printflike(2, 3);
static void err(const int code, const char *format, ...) {
    va_list va;

    va_start(va, format);
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, va);
    fprintf(stderr, ": %s\n", strerror(errno));
    va_end(va);
    exit(code);
}

/* errx - err() without the strerror */
static void errx(const int code, const char *format, ...) __printflike(2, 3);
static void errx(const int code, const char *format, ...) {
    va_list va;

    va_start(va, format);
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, va);
    fprintf(stderr, "\n");
    va_end(va);
    exit(code);
}

/* warn - err() without the exit */
static void warn(const char *format, ...) __printflike(1, 2);
static void warn(const char *format, ...) {
    va_list va;

    va_start(va, format);
    fprintf(stderr, "warning: ");
    vfprintf(stderr, format, va);
    fprintf(stderr, ": %s\n", strerror(errno));
    va_end(va);
}
#endif

/* [->] LIST_* macros taken from FreeBSD's src/sys/sys/queue.h,v 1.56
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Under a BSD license.
 */
#define LIST_HEAD(name, type)                                           \
struct name {                                                           \
        struct type *lh_first;  /* first element */                     \
}

#define LIST_HEAD_INITIALIZER(head)                                     \
        { NULL }

#define LIST_ENTRY(type)                                                \
struct {                                                                \
        struct type *le_next;   /* next element */                      \
        struct type **le_prev;  /* address of previous next element */  \
}

#define LIST_FIRST(head)        ((head)->lh_first)

#define LIST_FOREACH_SAFE(var, head, field, tvar)                       \
    for ((var) = LIST_FIRST((head));                                    \
        (var) && ((tvar) = LIST_NEXT((var), field), 1);                 \
        (var) = (tvar))

#define LIST_INSERT_HEAD(head, elm, field) do {                         \
        if ((LIST_NEXT((elm), field) = LIST_FIRST((head))) != NULL)     \
                LIST_FIRST((head))->field.le_prev = &LIST_NEXT((elm), field);\
        LIST_FIRST((head)) = (elm);                                     \
        (elm)->field.le_prev = &LIST_FIRST((head));                     \
} while (0)

#define LIST_NEXT(elm, field)   ((elm)->field.le_next)

#define LIST_REMOVE(elm, field) do {                                    \
        if (LIST_NEXT((elm), field) != NULL)                            \
                LIST_NEXT((elm), field)->field.le_prev =                \
                    (elm)->field.le_prev;                               \
        *(elm)->field.le_prev = LIST_NEXT((elm), field);                \
} while (0)
/* [<-] */

static LIST_HEAD(conn_list_head, connection) connlist =
    LIST_HEAD_INITIALIZER(conn_list_head);

struct connection {
    LIST_ENTRY(connection) entries;

    int socket;
    in_addr_t client;
    time_t last_active;
    enum {
        RECV_REQUEST,   /* receiving request */
        SEND_HEADER,    /* sending generated header */
        SEND_REPLY,     /* sending reply */
        DONE            /* connection closed, need to remove from queue */
    } state;

    /* char request[request_length+1] is null-terminated */
    char *request;
    size_t request_length;

    /* request fields */
    char *method, *url, *referer, *user_agent;
    off_t range_begin, range_end;
    off_t range_begin_given, range_end_given;

    char *header;
    size_t header_length, header_sent;
    int header_dont_free, header_only, http_code, conn_close;

    enum { REPLY_GENERATED, REPLY_FROMFILE } reply_type;
    char *reply;
    int reply_dont_free;
    int reply_fd;
    off_t reply_start, reply_length, reply_sent,
          total_sent; /* header + body = total, for logging */
};

struct web_forward_record {
    const char *host;
    const char *target;
    struct web_forward_record *next;
};

struct web_forward_record *web_forward = NULL;

struct mime_mapping {
    char *extension, *mimetype;
};

static struct mime_mapping *mime_map = NULL;
static size_t mime_map_size = 0;
static size_t longest_ext = 0;

/* If a connection is idle for idletime seconds or more, it gets closed and
 * removed from the connlist.  Set to 0 to remove the timeout
 * functionality.
 */
static int idletime = 60;
static char *keep_alive_field = NULL;

/* Time is cached in the event loop to avoid making an excessive number of
 * gettimeofday() calls.
 */
static time_t now;

/* To prevent a malformed request from eating up too much memory, die once the
 * request exceeds this many bytes:
 */
#define MAX_REQUEST_LENGTH 4000

/* Defaults can be overridden on the command-line */
static in_addr_t bindaddr = INADDR_ANY;
static uint16_t bindport = 8080; /* or 80 if running as root */
static int max_connections = -1;        /* kern.ipc.somaxconn */
static const char *index_name = "index.html";

static int sockin = -1;             /* socket to accept connections from */
static char *wwwroot = NULL;        /* a path name */
static char *logfile_name = NULL;   /* NULL = no logging */
static FILE *logfile = NULL;
static char *pidfile_name = NULL;   /* NULL = no pidfile */
static int want_chroot = 0, want_daemon = 0, want_accf = 0,
           want_keepalive = 1;
static uint64_t num_requests = 0, total_in = 0, total_out = 0;

static int running = 1; /* signal handler sets this to false */

#define INVALID_UID ((uid_t) -1)
#define INVALID_GID ((gid_t) -1)

static uid_t drop_uid = INVALID_UID;
static gid_t drop_gid = INVALID_GID;

/* Default mimetype mappings - make sure this array is NULL terminated. */
static const char *default_extension_map[] = {
    "application/ogg"      " ogg",
    "application/pdf"      " pdf",
    "application/xml"      " xsl xml",
    "application/xml-dtd"  " dtd",
    "application/xslt+xml" " xslt",
    "application/zip"      " zip",
    "audio/mpeg"           " mp2 mp3 mpga",
    "image/gif"            " gif",
    "image/jpeg"           " jpeg jpe jpg",
    "image/png"            " png",
    "text/css"             " css",
    "text/html"            " html htm",
    "text/javascript"      " js",
    "text/plain"           " txt asc",
    "video/mpeg"           " mpeg mpe mpg",
    "video/quicktime"      " qt mov",
    "video/x-msvideo"      " avi",
    NULL
};

static const char default_mimetype[] = "application/octet-stream";

/* Prototypes. */
static void poll_recv_request(struct connection *conn);
static void poll_send_header(struct connection *conn);
static void poll_send_reply(struct connection *conn);

/* close() that dies on error.  */
static void xclose(const int fd) {
    if (close(fd) == -1)
        err(1, "close()");
}

/* malloc that dies if it can't allocate. */
static void *xmalloc(const size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL)
        errx(1, "can't allocate %zu bytes", size);
    return ptr;
}

/* realloc() that dies if it can't reallocate. */
static void *xrealloc(void *original, const size_t size) {
    void *ptr = realloc(original, size);
    if (ptr == NULL)
        errx(1, "can't reallocate %zu bytes", size);
    return ptr;
}

/* strdup() that dies if it can't allocate.
 * Implement this ourselves since regular strdup() isn't C89.
 */
static char *xstrdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *dest = xmalloc(len);
    memcpy(dest, src, len);
    return dest;
}

#ifdef __sun /* unimpressed by Solaris */
static int vasprintf(char **strp, const char *fmt, va_list ap) {
    char tmp;
    int result = vsnprintf(&tmp, 1, fmt, ap);
    *strp = xmalloc(result+1);
    result = vsnprintf(*strp, result+1, fmt, ap);
    return result;
}
#endif

/* vasprintf() that dies if it fails. */
static unsigned int xvasprintf(char **ret, const char *format, va_list ap)
    __printflike(2,0);
static unsigned int xvasprintf(char **ret, const char *format, va_list ap) {
    int len = vasprintf(ret, format, ap);
    if (ret == NULL || len == -1)
        errx(1, "out of memory in vasprintf()");
    return (unsigned int)len;
}

/* asprintf() that dies if it fails. */
static unsigned int xasprintf(char **ret, const char *format, ...)
    __printflike(2,3);
static unsigned int xasprintf(char **ret, const char *format, ...) {
    va_list va;
    unsigned int len;

    va_start(va, format);
    len = xvasprintf(ret, format, va);
    va_end(va);
    return len;
}

/* Append buffer code.  A somewhat efficient string buffer with pool-based
 * reallocation.
 */
#define APBUF_INIT 4096
#define APBUF_GROW APBUF_INIT
struct apbuf {
    size_t length, pool;
    char *str;
};

static struct apbuf *make_apbuf(void) {
    struct apbuf *buf = xmalloc(sizeof(struct apbuf));
    buf->length = 0;
    buf->pool = APBUF_INIT;
    buf->str = xmalloc(buf->pool);
    return buf;
}

/* Append s (of length len) to buf. */
static void appendl(struct apbuf *buf, const char *s, const size_t len) {
    size_t need = buf->length + len;
    if (buf->pool < need) {
        /* pool has dried up */
        while (buf->pool < need)
            buf->pool += APBUF_GROW;
        buf->str = xrealloc(buf->str, buf->pool);
    }
    memcpy(buf->str + buf->length, s, len);
    buf->length += len;
}

#ifdef __GNUC__
#define append(buf, s) appendl(buf, s, \
    (__builtin_constant_p(s) ? sizeof(s)-1 : strlen(s)) )
#else
static void append(struct apbuf *buf, const char *s) {
    appendl(buf, s, strlen(s));
}
#endif

static void appendf(struct apbuf *buf, const char *format, ...)
    __printflike(2, 3);
static void appendf(struct apbuf *buf, const char *format, ...) {
    char *tmp;
    va_list va;
    size_t len;

    va_start(va, format);
    len = xvasprintf(&tmp, format, va);
    va_end(va);
    appendl(buf, tmp, len);
    free(tmp);
}

/* Make the specified socket non-blocking. */
static void nonblock_socket(const int sock) {
    int flags = fcntl(sock, F_GETFL, NULL);

    if (flags == -1)
        err(1, "fcntl(F_GETFL)");
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) == -1)
        err(1, "fcntl() to set O_NONBLOCK");
}

/* Split string out of src with range [left:right-1] */
static char *split_string(const char *src,
        const size_t left, const size_t right) {
    char *dest;
    assert(left <= right);
    assert(left < strlen(src));   /* [left means must be smaller */
    assert(right <= strlen(src)); /* right) means can be equal or smaller */

    dest = xmalloc(right - left + 1);
    memcpy(dest, src+left, right-left);
    dest[right-left] = '\0';
    return dest;
}

/* Consolidate slashes in-place by shifting parts of the string over repeated
 * slashes.
 */
static void consolidate_slashes(char *s) {
    size_t left = 0, right = 0;
    int saw_slash = 0;

    assert(s != NULL);
    while (s[right] != '\0') {
        if (saw_slash) {
            if (s[right] == '/')
                right++;
            else {
                saw_slash = 0;
                s[left++] = s[right++];
            }
        } else {
            if (s[right] == '/')
                saw_slash++;
            s[left++] = s[right++];
        }
    }
    s[left] = '\0';
}

/* Resolve /./ and /../ in a URL, in-place.  Returns NULL if the URL is
 * invalid/unsafe, or the original buffer if successful.
 */
static char *make_safe_url(char *url) {
    struct {
        char *start;
        size_t len;
    } *chunks;
    unsigned int num_slashes, num_chunks;
    size_t urllen, i, j, pos;
    int ends_in_slash;

    assert(url != NULL);
    if (url[0] != '/')
        return NULL;
    consolidate_slashes(url);
    urllen = strlen(url);
    if (urllen > 0)
        ends_in_slash = (url[urllen-1] == '/');
    else
        ends_in_slash = 1;

    /* count the slashes */
    for (i=0, num_slashes=0; i<urllen; i++)
        if (url[i] == '/')
            num_slashes++;

    /* make an array for the URL elements */
    chunks = xmalloc(sizeof(*chunks) * num_slashes);

    /* split by slashes and build chunks array */
    num_chunks = 0;
    for (i=1; i<urllen;) {
        /* look for the next slash */
        for (j=i; j<urllen && url[j] != '/'; j++)
            ;

        /* process url[i,j) */
        if ((j == i+1) && (url[i] == '.'))
            /* "." */;
        else if ((j == i+2) && (url[i] == '.') && (url[i+1] == '.')) {
            /* ".." */
            if (num_chunks == 0) {
                /* unsafe string so free chunks */
                free(chunks);
                return (NULL);
            } else
                num_chunks--;
        } else {
            chunks[num_chunks].start = url+i;
            chunks[num_chunks].len = j-i;
            num_chunks++;
        }

        i = j + 1; /* url[j] is a slash - move along one */
    }

    /* reassemble in-place */
    pos = 0;
    for (i=0; i<num_chunks; i++) {
        assert(pos <= urllen);
        url[pos++] = '/';

        assert(pos + chunks[i].len <= urllen);
        assert(url + pos <= chunks[i].start);

        if (url+pos < chunks[i].start)
            memmove(url+pos, chunks[i].start, chunks[i].len);
        pos += chunks[i].len;
    }
    free(chunks);

    if ((num_chunks == 0) || ends_in_slash)
        url[pos++] = '/';
    assert(pos <= urllen);
    url[pos] = '\0';
    return url;
}

static void add_web_forward(const char * const host, const char * const target) {
    if (debug)
        printf("add web forward %s -> %s\n",
               host, target);

    struct web_forward_record **ref_ptr = &web_forward;
    while(*ref_ptr) {
        ref_ptr = &((*ref_ptr)->next);
    }
    (*ref_ptr) = xmalloc(sizeof(struct web_forward_record));
    (*ref_ptr)->host = host;
    (*ref_ptr)->target = target;
    (*ref_ptr)->next = NULL;
}

/* Associates an extension with a mimetype in the mime_map.  Entries are in
 * unsorted order.  Makes copies of extension and mimetype strings.
 */
static void add_mime_mapping(const char *extension, const char *mimetype) {
    size_t i;
    assert(strlen(extension) > 0);
    assert(strlen(mimetype) > 0);

    /* update longest_ext */
    i = strlen(extension);
    if (i > longest_ext)
        longest_ext = i;

    /* look through list and replace an existing entry if possible */
    for (i = 0; i < mime_map_size; i++)
        if (strcmp(mime_map[i].extension, extension) == 0) {
            free(mime_map[i].mimetype);
            mime_map[i].mimetype = xstrdup(mimetype);
            return;
        }

    /* no replacement - add a new entry */
    mime_map_size++;
    mime_map = xrealloc(mime_map,
        sizeof(struct mime_mapping) * mime_map_size);
    mime_map[mime_map_size - 1].extension = xstrdup(extension);
    mime_map[mime_map_size - 1].mimetype = xstrdup(mimetype);
}

/* qsort() the mime_map.  The map must be sorted before it can be
 * binary-searched.
 */
static int mime_mapping_cmp(const void *a, const void *b) {
    return strcmp(((const struct mime_mapping *)a)->extension,
                  ((const struct mime_mapping *)b)->extension);
}

static void sort_mime_map(void) {
    qsort(mime_map, mime_map_size, sizeof(struct mime_mapping),
        mime_mapping_cmp);
}

/* Parses a mime.types line and adds the parsed data to the mime_map. */
static void parse_mimetype_line(const char *line) {
    unsigned int pad, bound1, lbound, rbound;

    /* parse mimetype */
    for (pad=0; (line[pad] == ' ') || (line[pad] == '\t'); pad++)
        ;
    if (line[pad] == '\0' || /* empty line */
        line[pad] == '#')    /* comment */
        return;

    for (bound1=pad+1;
        (line[bound1] != ' ') &&
        (line[bound1] != '\t');
        bound1++) {
        if (line[bound1] == '\0')
            return; /* malformed line */
    }

    lbound = bound1;
    for (;;) {
        char *mimetype, *extension;

        /* find beginning of extension */
        for (; (line[lbound] == ' ') || (line[lbound] == '\t'); lbound++)
            ;
        if (line[lbound] == '\0')
            return; /* end of line */

        /* find end of extension */
        for (rbound = lbound;
            line[rbound] != ' ' &&
            line[rbound] != '\t' &&
            line[rbound] != '\0';
            rbound++)
            ;

        mimetype = split_string(line, pad, bound1);
        extension = split_string(line, lbound, rbound);
        add_mime_mapping(extension, mimetype);
        free(mimetype);
        free(extension);

        if (line[rbound] == '\0')
            return; /* end of line */
        else
            lbound = rbound + 1;
    }
}

/* Adds contents of default_extension_map[] to mime_map list.  The array must
 * be NULL terminated.
 */
static void parse_default_extension_map(void) {
    size_t i;

    for (i = 0; default_extension_map[i] != NULL; i++)
        parse_mimetype_line(default_extension_map[i]);
}

/* read a line from fp, return its contents in a dynamically allocated buffer,
 * not including the line ending.
 *
 * Handles CR, CRLF and LF line endings, as well as NOEOL correctly.  If
 * already at EOF, returns NULL.  Will err() or errx() in case of
 * unexpected file error or running out of memory.
 */
static char *read_line(FILE *fp) {
    char *buf;
    long startpos, endpos;
    size_t linelen, numread;
    int c;

    startpos = ftell(fp);
    if (startpos == -1)
        err(1, "ftell()");

    /* find end of line (or file) */
    linelen = 0;
    for (;;) {
        c = fgetc(fp);
        if ((c == EOF) || (c == (int)'\n') || (c == (int)'\r'))
            break;
        linelen++;
    }

    /* return NULL on EOF (and empty line) */
    if (linelen == 0 && c == EOF)
        return NULL;

    endpos = ftell(fp);
    if (endpos == -1)
        err(1, "ftell()");

    /* skip CRLF */
    if ((c == (int)'\r') && (fgetc(fp) == (int)'\n'))
        endpos++;

    buf = xmalloc(linelen + 1);

    /* rewind file to where the line stared and load the line */
    if (fseek(fp, startpos, SEEK_SET) == -1)
        err(1, "fseek()");
    numread = fread(buf, 1, linelen, fp);
    if (numread != linelen)
        errx(1, "fread() %zu bytes, expecting %zu bytes", numread, linelen);

    /* terminate buffer */
    buf[linelen] = 0;

    /* advance file pointer over the endline */
    if (fseek(fp, endpos, SEEK_SET) == -1)
        err(1, "fseek()");

    return buf;
}

/* Removes the ending newline in a string, if there is one. */
static void chomp(char *str) {
    size_t len = strlen(str);
    if (len == 0)
        return;
    if (str[len - 1] == '\n')
        str[len - 1] = '\0';
}

/* ---------------------------------------------------------------------------
 * Adds contents of specified file to mime_map list.
 */
static void parse_extension_map_file(const char *filename) {
    char *buf;
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
        err(1, "fopen(\"%s\")", filename);
    while ((buf = read_line(fp)) != NULL) {
        chomp(buf);
        parse_mimetype_line(buf);
        free(buf);
    }
    fclose(fp);
}

/* Uses the mime_map to determine a Content-Type: for a requested URL.  This
 * bsearch()es mime_map, so make sure it's sorted first.
 */
static int mime_mapping_cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a,
                 ((const struct mime_mapping *)b)->extension);
}

static const char *url_content_type(const char *url) {
    int period, urllen = (int)strlen(url);

    for (period = urllen - 1;
         (period > 0) && (url[period] != '.') &&
         (urllen - period - 1 <= (int)longest_ext);
         period--)
            ;

    if ((period >= 0) && (url[period] == '.')) {
        struct mime_mapping *result =
            bsearch((url + period + 1), mime_map, mime_map_size,
                    sizeof(struct mime_mapping), mime_mapping_cmp_str);
        if (result != NULL) {
            assert(strcmp(url + period + 1, result->extension) == 0);
            return result->mimetype;
        }
    }
    /* else no period found in the string */
    return default_mimetype;
}

/* Initialize the sockin global.  This is the socket that we accept
 * connections from.
 */
static void init_sockin(void) {
    struct sockaddr_in addrin;
    int sockopt;

    /* create incoming socket */
    sockin = socket(PF_INET, SOCK_STREAM, 0);
    if (sockin == -1)
        err(1, "socket()");

    /* reuse address */
    sockopt = 1;
    if (setsockopt(sockin, SOL_SOCKET, SO_REUSEADDR,
                   &sockopt, sizeof(sockopt)) == -1)
        err(1, "setsockopt(SO_REUSEADDR)");

#if 0
    /* disable Nagle since we buffer everything ourselves */
    sockopt = 1;
    if (setsockopt(sockin, IPPROTO_TCP, TCP_NODELAY,
            &sockopt, sizeof(sockopt)) == -1)
        err(1, "setsockopt(TCP_NODELAY)");
#endif

#ifdef TORTURE
    /* torture: cripple the kernel-side send buffer so we can only squeeze out
     * one byte at a time (this is for debugging)
     */
    sockopt = 1;
    if (setsockopt(sockin, SOL_SOCKET, SO_SNDBUF,
            &sockopt, sizeof(sockopt)) == -1)
        err(1, "setsockopt(SO_SNDBUF)");
#endif

    /* bind socket */
    addrin.sin_family = (u_char)PF_INET;
    addrin.sin_port = htons(bindport);
    addrin.sin_addr.s_addr = bindaddr;
    memset(&(addrin.sin_zero), 0, 8);
    if (bind(sockin, (struct sockaddr *)&addrin,
             sizeof(struct sockaddr)) == -1)
        err(1, "bind(port %u)", bindport);

    printf("listening on: http://%s:%u/\n",
        inet_ntoa(addrin.sin_addr), bindport);

    /* listen on socket */
    if (listen(sockin, max_connections) == -1)
        err(1, "listen()");

    /* enable acceptfilter (this is only available on FreeBSD) */
    if (want_accf) {
#if defined(__FreeBSD__)
        struct accept_filter_arg filt = {"httpready", ""};
        if (setsockopt(sockin, SOL_SOCKET, SO_ACCEPTFILTER,
                       &filt, sizeof(filt)) == -1)
            fprintf(stderr, "cannot enable acceptfilter: %s\n",
                strerror(errno));
        else
            printf("enabled acceptfilter\n");
#else
        printf("this platform doesn't support acceptfilter\n");
#endif
    }
}

static void usage(const char *argv0) {
    printf("usage:\t%s /path/to/wwwroot [flags]\n\n", argv0);
    printf("flags:\t--port number (default: %u, or 80 if running as root)\n"
    "\t\tSpecifies which port to listen on for connections.\n\n", bindport);
    printf("\t--addr ip (default: all)\n"
    "\t\tIf multiple interfaces are present, specifies\n"
    "\t\twhich one to bind the listening port to.\n\n");
    printf("\t--maxconn number (default: system maximum)\n"
    "\t\tSpecifies how many concurrent connections to accept.\n\n");
    printf("\t--log filename (default: stdout)\n"
    "\t\tSpecifies which file to append the request log to.\n\n");
    printf("\t--chroot (default: don't chroot)\n"
    "\t\tLocks server into wwwroot directory for added security.\n\n");
    printf("\t--daemon (default: don't daemonize)\n"
    "\t\tDetach from the controlling terminal and run in the background.\n\n");
    printf("\t--index filename (default: %s)\n"
    "\t\tDefault file to serve when a directory is requested.\n\n",
        index_name);
    printf("\t--mimetypes filename (optional)\n"
    "\t\tParses specified file for extension-MIME associations.\n\n");
    printf("\t--uid uid/uname, --gid gid/gname (default: don't privdrop)\n"
    "\t\tDrops privileges to given uid:gid after initialization.\n\n");
    printf("\t--pidfile filename (default: no pidfile)\n"
    "\t\tWrite PID to the specified file.  Note that if you are\n"
    "\t\tusing --chroot, then the pidfile must be relative to,\n"
    "\t\tand inside the wwwroot.\n\n");
    printf("\t--no-keepalive\n"
    "\t\tDisables HTTP Keep-Alive functionality.\n\n");
#ifdef __FreeBSD__
    printf("\t--accf (default: don't use acceptfilter)\n"
    "\t\tUse acceptfilter.  Needs the accf_http module loaded.\n\n");
#endif
    printf("\t--forward host url (default: don't forward)\n"
    "\t\tWeb forward (301 redirect).\n"
    "\t\tRequests to the host are redirected to the corresponding url.\n"
    "\t\tThe option may be specified multiple times. In the case\n"
    "\t\tthe host is matched in the order of appearance.\n\n");
}

/* Returns 1 if string is a number, 0 otherwise.  Set num to NULL if
 * disinterested in value.
 */
static int str_to_num(const char *str, int *num) {
    char *endptr;

    long l = strtol(str, &endptr, 10);
    if (*endptr != '\0')
        return 0;
    if (num != NULL)
        *num = (int)l;
    return 1;
}

static void parse_commandline(const int argc, char *argv[]) {
    int i;
    size_t len;

    if ((argc < 2) || (argc == 2 && strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]); /* no wwwroot given */
        exit(EXIT_FAILURE);
    }

    if (getuid() == 0)
        bindport = 80;

    wwwroot = xstrdup(argv[1]);
    /* Strip ending slash. */
    len = strlen(wwwroot);
    if (len > 0)
        if (wwwroot[len - 1] == '/')
            wwwroot[len - 1] = '\0';

    /* walk through the remainder of the arguments (if any) */
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (++i >= argc)
                errx(1, "missing number after --port");
            bindport = (uint16_t)atoi(argv[i]);
        }
        else if (strcmp(argv[i], "--addr") == 0) {
            if (++i >= argc)
                errx(1, "missing ip after --addr");
            bindaddr = inet_addr(argv[i]);
            if (bindaddr == (in_addr_t)INADDR_NONE)
                errx(1, "malformed --addr argument");
        }
        else if (strcmp(argv[i], "--maxconn") == 0) {
            if (++i >= argc)
                errx(1, "missing number after --maxconn");
            max_connections = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "--log") == 0) {
            if (++i >= argc)
                errx(1, "missing filename after --log");
            logfile_name = argv[i];
        }
        else if (strcmp(argv[i], "--chroot") == 0) {
            want_chroot = 1;
        }
        else if (strcmp(argv[i], "--daemon") == 0) {
            want_daemon = 1;
        }
        else if (strcmp(argv[i], "--index") == 0) {
            if (++i >= argc)
                errx(1, "missing filename after --index");
            index_name = argv[i];
        }
        else if (strcmp(argv[i], "--mimetypes") == 0) {
            if (++i >= argc)
                errx(1, "missing filename after --mimetypes");
            parse_extension_map_file(argv[i]);
        }
        else if (strcmp(argv[i], "--uid") == 0) {
            struct passwd *p;
            int num;
            if (++i >= argc)
                errx(1, "missing uid after --uid");
            p = getpwnam(argv[i]);
            if ((p == NULL) && (str_to_num(argv[i], &num)))
                p = getpwuid( (uid_t)num );

            if (p == NULL)
                errx(1, "no such uid: `%s'", argv[i]);
            drop_uid = p->pw_uid;
        }
        else if (strcmp(argv[i], "--gid") == 0) {
            struct group *g;
            int num;
            if (++i >= argc)
                errx(1, "missing gid after --gid");
            g = getgrnam(argv[i]);
            if ((g == NULL) && (str_to_num(argv[i], &num)))
                g = getgrgid((gid_t)num);
            if (g == NULL)
                errx(1, "no such gid: `%s'", argv[i]);
            drop_gid = g->gr_gid;
        }
        else if (strcmp(argv[i], "--pidfile") == 0) {
            if (++i >= argc)
                errx(1, "missing filename after --pidfile");
            pidfile_name = argv[i];
        }
        else if (strcmp(argv[i], "--no-keepalive") == 0) {
            want_keepalive = 0;
        }
        else if (strcmp(argv[i], "--accf") == 0) {
            want_accf = 1;
        }
        else if (strcmp(argv[i], "--forward") == 0) {
            if (++i >= argc)
                errx(1, "missing host after --forward");
            const char * const host = argv[i];
            if (++i >= argc)
                errx(1, "missing url after --forward");
            const char * const url = argv[i];
            add_web_forward(host, url);
        }
        else
            errx(1, "unknown argument `%s'", argv[i]);
    }
}

/* Allocate and initialize an empty connection. */
static struct connection *new_connection(void) {
    struct connection *conn = xmalloc(sizeof(struct connection));

    conn->socket = -1;
    conn->client = INADDR_ANY;
    conn->last_active = now;
    conn->request = NULL;
    conn->request_length = 0;
    conn->method = NULL;
    conn->url = NULL;
    conn->referer = NULL;
    conn->user_agent = NULL;
    conn->range_begin = 0;
    conn->range_end = 0;
    conn->range_begin_given = 0;
    conn->range_end_given = 0;
    conn->header = NULL;
    conn->header_length = 0;
    conn->header_sent = 0;
    conn->header_dont_free = 0;
    conn->header_only = 0;
    conn->http_code = 0;
    conn->conn_close = 1;
    conn->reply = NULL;
    conn->reply_dont_free = 0;
    conn->reply_fd = -1;
    conn->reply_start = 0;
    conn->reply_length = 0;
    conn->reply_sent = 0;
    conn->total_sent = 0;

    /* Make it harmless so it gets garbage-collected if it should, for some
     * reason, fail to be correctly filled out.
     */
    conn->state = DONE;

    return conn;
}

/* Accept a connection from sockin and add it to the connection queue. */
static void accept_connection(void) {
    struct sockaddr_in addrin;
    socklen_t sin_size;
    struct connection *conn;

    /* allocate and initialise struct connection */
    conn = new_connection();

    sin_size = sizeof(addrin);
    memset(&addrin, 0, sin_size);
    conn->socket = accept(sockin, (struct sockaddr *)&addrin, &sin_size);
    if (conn->socket == -1)
        err(1, "accept()");

    nonblock_socket(conn->socket);

    conn->state = RECV_REQUEST;
    conn->client = addrin.sin_addr.s_addr;
    LIST_INSERT_HEAD(&connlist, conn, entries);

    if (debug)
        printf("accepted connection from %s:%u\n",
               inet_ntoa(addrin.sin_addr), ntohs(addrin.sin_port));

    /* Try to read straight away rather than going through another iteration
     * of the select() loop.
     */
    poll_recv_request(conn);
}

/* Should this character be logencoded?
 */
static int needs_logencoding(const unsigned char c) {
    return ((c <= 0x1F) || (c >= 0x7F) || (c == '"'));
}

/* Encode string for logging.
 */
static void logencode(const char *src, char *dest) {
    static const char hex[] = "0123456789ABCDEF";
    int i, j;

    for (i = j = 0; src[i] != '\0'; i++) {
        if (needs_logencoding((unsigned char)src[i])) {
            dest[j++] = '%';
            dest[j++] = hex[(src[i] >> 4) & 0xF];
            dest[j++] = hex[ src[i]       & 0xF];
        }
        else
            dest[j++] = src[i];
    }
    dest[j] = '\0';
}

/* Add a connection's details to the logfile. */
static void log_connection(const struct connection *conn) {
    struct in_addr inaddr;
    char *safe_method, *safe_url, *safe_referer, *safe_user_agent;

    if (logfile == NULL)
        return;
    if (conn->http_code == 0)
        return; /* invalid - died in request */
    if (conn->method == NULL)
        return; /* invalid - didn't parse - maybe too long */

    inaddr.s_addr = conn->client;

#define make_safe(x) \
    if (conn->x) { \
        safe_##x = xmalloc(strlen(conn->x)*3 + 1); \
        logencode(conn->x, safe_##x); \
    } else { \
        safe_##x = NULL; \
    }

    make_safe(method);
    make_safe(url);
    make_safe(referer);
    make_safe(user_agent);

#define use_safe(x) safe_##x ? safe_##x : ""

    fprintf(logfile, "%lu %s \"%s %s\" %d %llu \"%s\" \"%s\"\n",
        (unsigned long int)now,
        inet_ntoa(inaddr),
        use_safe(method),
        use_safe(url),
        conn->http_code,
        llu(conn->total_sent),
        use_safe(referer),
        use_safe(user_agent)
        );
    fflush(logfile);

#define free_safe(x) if (safe_##x) free(safe_##x);

    free_safe(method);
    free_safe(url);
    free_safe(referer);
    free_safe(user_agent);

#undef make_safe
#undef use_safe
#undef free_safe
}

/* Log a connection, then cleanly deallocate its internals. */
static void free_connection(struct connection *conn) {
    if (debug) printf("free_connection(%d)\n", conn->socket);
    log_connection(conn);
    if (conn->socket != -1) xclose(conn->socket);
    if (conn->request != NULL) free(conn->request);
    if (conn->method != NULL) free(conn->method);
    if (conn->url != NULL) free(conn->url);
    if (conn->referer != NULL) free(conn->referer);
    if (conn->user_agent != NULL) free(conn->user_agent);
    if (conn->header != NULL && !conn->header_dont_free) free(conn->header);
    if (conn->reply != NULL && !conn->reply_dont_free) free(conn->reply);
    if (conn->reply_fd != -1) xclose(conn->reply_fd);
}

/* Recycle a finished connection for HTTP/1.1 Keep-Alive. */
static void recycle_connection(struct connection *conn) {
    int socket_tmp = conn->socket;
    if (debug)
        printf("recycle_connection(%d)\n", socket_tmp);
    conn->socket = -1; /* so free_connection() doesn't close it */
    free_connection(conn);
    conn->socket = socket_tmp;

    /* don't reset conn->client */
    conn->request = NULL;
    conn->request_length = 0;
    conn->method = NULL;
    conn->url = NULL;
    conn->referer = NULL;
    conn->user_agent = NULL;
    conn->range_begin = 0;
    conn->range_end = 0;
    conn->range_begin_given = 0;
    conn->range_end_given = 0;
    conn->header = NULL;
    conn->header_length = 0;
    conn->header_sent = 0;
    conn->header_dont_free = 0;
    conn->header_only = 0;
    conn->http_code = 0;
    conn->conn_close = 1;
    conn->reply = NULL;
    conn->reply_dont_free = 0;
    conn->reply_fd = -1;
    conn->reply_start = 0;
    conn->reply_length = 0;
    conn->reply_sent = 0;
    conn->total_sent = 0;

    conn->state = RECV_REQUEST; /* ready for another */
}

/* Uppercasify all characters in a string of given length. */
static void strntoupper(char *str, const size_t length) {
    size_t i;

    for (i = 0; i < length; i++)
        str[i] = (char)toupper(str[i]);
}

/* If a connection has been idle for more than idletime seconds, it will be
 * marked as DONE and killed off in httpd_poll()
 */
static void poll_check_timeout(struct connection *conn) {
    if (idletime > 0) { /* optimised away by compiler */
        if (now - conn->last_active >= idletime) {
            if (debug)
                printf("poll_check_timeout(%d) caused closure\n",
                       conn->socket);
            conn->conn_close = 1;
            conn->state = DONE;
        }
    }
}

/* Format [when] as an RFC1123 date, stored in the specified buffer.  The same
 * buffer is returned for convenience.
 */
#define DATE_LEN 30 /* strlen("Fri, 28 Feb 2003 00:02:08 GMT")+1 */
static char *rfc1123_date(char *dest, const time_t when) {
    time_t when_copy = when;
    if (strftime(dest, DATE_LEN,
                 "%a, %d %b %Y %H:%M:%S GMT", gmtime(&when_copy)) == 0)
        errx(1, "strftime() failed [%s]", dest);
    return dest;
}

/* Decode URL by converting %XX (where XX are hexadecimal digits) to the
 * character it represents.  Don't forget to free the return value.
 */
static char *urldecode(const char *url) {
    size_t i, pos, len = strlen(url);
    char *out = xmalloc(len+1);

    for (i = 0, pos = 0; i < len; i++) {
        if ((url[i] == '%') && (i+2 < len) &&
            isxdigit(url[i+1]) && isxdigit(url[i+2])) {
            /* decode %XX */
#define HEX_TO_DIGIT(hex) ( \
    ((hex) >= 'A' && (hex) <= 'F') ? ((hex)-'A'+10): \
    ((hex) >= 'a' && (hex) <= 'f') ? ((hex)-'a'+10): \
    ((hex)-'0') )

            out[pos++] = HEX_TO_DIGIT(url[i+1]) * 16 +
                         HEX_TO_DIGIT(url[i+2]);
            i += 2;
#undef HEX_TO_DIGIT
        } else {
            /* straight copy */
            out[pos++] = url[i];
        }
    }
    out[pos] = '\0';
    return out;
}

/* Returns Connection or Keep-Alive header, depending on conn_close. */
static const char *keep_alive(const struct connection *conn)
{
    return (conn->conn_close ? "Connection: close\r\n" : keep_alive_field);
}

/* A default reply for any (erroneous) occasion. */
static void default_reply(struct connection *conn,
        const int errcode, const char *errname, const char *format, ...)
        __printflike(4, 5);
static void default_reply(struct connection *conn,
        const int errcode, const char *errname, const char *format, ...) {
    char *reason, date[DATE_LEN];
    va_list va;

    va_start(va, format);
    xvasprintf(&reason, format, va);
    va_end(va);

    /* Only really need to calculate the date once. */
    rfc1123_date(date, now);

    conn->reply_length = xasprintf(&(conn->reply),
     "<html><head><title>%d %s</title></head><body>\n"
     "<h1>%s</h1>\n" /* errname */
     "%s\n" /* reason */
     "<hr>\n"
     "Generated by %s on %s\n"
     "</body></html>\n",
     errcode, errname, errname, reason, pkgname, date);
    free(reason);

    conn->header_length = xasprintf(&(conn->header),
     "HTTP/1.1 %d %s\r\n"
     "Date: %s\r\n"
     "Server: %s\r\n"
     "Accept-Ranges: bytes\r\n"
     "%s" /* keep-alive */
     "Content-Length: %llu\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     errcode, errname, date, pkgname, keep_alive(conn),
     llu(conn->reply_length));

    conn->reply_type = REPLY_GENERATED;
    conn->http_code = errcode;
}

static void redirect(struct connection *conn, const char *format, ...)
    __printflike(2, 3);
static void redirect(struct connection *conn, const char *format, ...) {
    char *where, date[DATE_LEN];
    va_list va;

    va_start(va, format);
    xvasprintf(&where, format, va);
    va_end(va);

    /* Only really need to calculate the date once. */
    rfc1123_date(date, now);

    conn->reply_length = xasprintf(&(conn->reply),
     "<html><head><title>301 Moved Permanently</title></head><body>\n"
     "<h1>Moved Permanently</h1>\n"
     "Moved to: <a href=\"%s\">%s</a>\n" /* where x 2 */
     "<hr>\n"
     "Generated by %s on %s\n"
     "</body></html>\n",
     where, where, pkgname, date);

    conn->header_length = xasprintf(&(conn->header),
     "HTTP/1.1 301 Moved Permanently\r\n"
     "Date: %s\r\n"
     "Server: %s\r\n"
     /* "Accept-Ranges: bytes\r\n" - not relevant here */
     "Location: %s\r\n"
     "%s" /* keep-alive */
     "Content-Length: %llu\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     date, pkgname, where, keep_alive(conn), llu(conn->reply_length));

    free(where);
    conn->reply_type = REPLY_GENERATED;
    conn->http_code = 301;
}

/* Parses a single HTTP request field.  Returns string from end of [field] to
 * first \r, \n or end of request string.  Returns NULL if [field] can't be
 * matched.
 *
 * You need to remember to deallocate the result.
 * example: parse_field(conn, "Referer: ");
 */
static char *parse_field(const struct connection *conn, const char *field) {
    size_t bound1, bound2;
    char *pos;

    /* find start */
    pos = strstr(conn->request, field);
    if (pos == NULL)
        return NULL;
    assert(pos >= conn->request);
    bound1 = (size_t)(pos - conn->request) + strlen(field);

    /* find end */
    for (bound2 = bound1;
         ((conn->request[bound2] != '\r') &&
          (conn->request[bound2] != '\n') &&
          (bound2 < conn->request_length));
         bound2++)
            ;

    /* copy to buffer */
    return split_string(conn->request, bound1, bound2);
}

/* Parse a Range: field into range_begin and range_end.  Only handles the
 * first range if a list is given.  Sets range_{begin,end}_given to 1 if
 * either part of the range is given.
 */
static void parse_range_field(struct connection *conn) {
    size_t bound1, bound2, len;
    char *range;

    range = parse_field(conn, "Range: bytes=");
    if (range == NULL)
        return;
    len = strlen(range);

    do {
        /* parse number up to hyphen */
        bound1 = 0;
        for (bound2=0;
            isdigit((int)range[bound2]) && (bound2 < len);
            bound2++)
                ;

        if ((bound2 == len) || (range[bound2] != '-'))
            break; /* there must be a hyphen here */

        if (bound1 != bound2) {
            conn->range_begin_given = 1;
            conn->range_begin = (off_t)strtoll(range+bound1, NULL, 10);
        }

        /* parse number after hyphen */
        bound2++;
        for (bound1=bound2;
            isdigit((int)range[bound2]) && (bound2 < len);
            bound2++)
                ;

        if ((bound2 != len) && (range[bound2] != ','))
            break; /* must be end of string or a list to be valid */

        if (bound1 != bound2) {
            conn->range_end_given = 1;
            conn->range_end = (off_t)strtoll(range+bound1, NULL, 10);
        }
    } while(0);
    free(range);
}

/* Parse an HTTP request like "GET / HTTP/1.1" to get the method (GET), the
 * url (/), the referer (if given) and the user-agent (if given).  Remember to
 * deallocate all these buffers.  The method will be returned in uppercase.
 */
static int parse_request(struct connection *conn) {
    size_t bound1, bound2;
    char *tmp;
    assert(conn->request_length == strlen(conn->request));

    /* parse method */
    for (bound1 = 0;
        (bound1 < conn->request_length) &&
        (conn->request[bound1] != ' ');
        bound1++)
            ;

    conn->method = split_string(conn->request, 0, bound1);
    strntoupper(conn->method, bound1);

    /* parse url */
    for (;
        (bound1 < conn->request_length) &&
        (conn->request[bound1] == ' ');
        bound1++)
            ;

    if (bound1 == conn->request_length)
        return 0; /* fail */

    for (bound2 = bound1 + 1;
        (bound2 < conn->request_length) &&
        (conn->request[bound2] != ' ') &&
        (conn->request[bound2] != '\r') &&
        (conn->request[bound2] != '\n');
        bound2++)
            ;

    conn->url = split_string(conn->request, bound1, bound2);

    /* parse protocol to determine conn_close */
    if (conn->request[bound2] == ' ') {
        char *proto;
        for (bound1 = bound2;
            (bound1 < conn->request_length) &&
            (conn->request[bound1] == ' ');
            bound1++)
                ;

        for (bound2 = bound1 + 1;
            (bound2 < conn->request_length) &&
            (conn->request[bound2] != ' ') &&
            (conn->request[bound2] != '\r');
            bound2++)
                ;

        proto = split_string(conn->request, bound1, bound2);
        if (strcasecmp(proto, "HTTP/1.1") == 0)
            conn->conn_close = 0;
        free(proto);
    }

    /* parse connection field */
    tmp = parse_field(conn, "Connection: ");
    if (tmp != NULL) {
        if (strcasecmp(tmp, "close") == 0)
            conn->conn_close = 1;
        else if (strcasecmp(tmp, "keep-alive") == 0)
            conn->conn_close = 0;
        free(tmp);
    }

    /* cmdline flag can be used to deny keep-alive */
    if (!want_keepalive)
        conn->conn_close = 1;

    /* parse important fields */
    conn->referer = parse_field(conn, "Referer: ");
    conn->user_agent = parse_field(conn, "User-Agent: ");
    parse_range_field(conn);
    return 1;
}

static int file_exists(const char *path) {
    struct stat filestat;
    if ((stat(path, &filestat) == -1) && (errno = ENOENT))
        return 0;
    else
        return 1;
}

struct dlent {
    char *name;
    int is_dir;
    off_t size;
};

static int dlent_cmp(const void *a, const void *b) {
    return strcmp((*((const struct dlent * const *)a))->name,
                  (*((const struct dlent * const *)b))->name);
}

/* Make sorted list of files in a directory.  Returns number of entries, or -1
 * if error occurs.
 */
static ssize_t make_sorted_dirlist(const char *path, struct dlent ***output) {
    DIR *dir;
    struct dirent *ent;
    size_t entries = 0;
    size_t pool = 128;
    char *currname;
    struct dlent **list = NULL;

    dir = opendir(path);
    if (dir == NULL)
        return -1;

    currname = xmalloc(strlen(path) + MAXNAMLEN + 1);
    list = xmalloc(sizeof(struct dlent*) * pool);

    /* construct list */
    while ((ent = readdir(dir)) != NULL) {
        struct stat s;

        if ((ent->d_name[0] == '.') && (ent->d_name[1] == '\0'))
            continue; /* skip "." */
        assert(strlen(ent->d_name) <= MAXNAMLEN);
        sprintf(currname, "%s%s", path, ent->d_name);
        if (stat(currname, &s) == -1)
            continue; /* skip un-stat-able files */
        if (entries == pool) {
            pool *= 2;
            list = xrealloc(list, sizeof(struct dlent*) * pool);
        }
        list[entries] = xmalloc(sizeof(struct dlent));
        list[entries]->name = xstrdup(ent->d_name);
        list[entries]->is_dir = S_ISDIR(s.st_mode);
        list[entries]->size = s.st_size;
        entries++;
    }
    closedir(dir);
    free(currname);
    qsort(list, entries, sizeof(struct dlent*), dlent_cmp);
    *output = list;
    return (ssize_t)entries;
}

/* Cleanly deallocate a sorted list of directory files. */
static void cleanup_sorted_dirlist(struct dlent **list, const ssize_t size) {
    ssize_t i;

    for (i = 0; i < size; i++) {
        free(list[i]->name);
        free(list[i]);
    }
}

/* Should this character be urlencoded? (according to RFC1738)
 * Contributed by nf.
 */
static int needs_urlencoding(const unsigned char c) {
    unsigned int i;
    static const char bad[] = "<>\"%{}|^~[]`\\;:/?@#=&";

    /* Non-US-ASCII characters */
    if ((c <= 0x1F) || (c >= 0x7F))
        return 1;

    for (i = 0; i < sizeof(bad) - 1; i++)
        if (c == bad[i])
            return 1;

    return 0;
}

/* Encode string to be an RFC1738-compliant URL part.
 * Contributed by nf.
 */
static void urlencode(const char *src, char *dest) {
    static const char hex[] = "0123456789ABCDEF";
    int i, j;

    for (i = j = 0; src[i] != '\0'; i++) {
        if (needs_urlencoding((unsigned char)src[i])) {
            dest[j++] = '%';
            dest[j++] = hex[(src[i] >> 4) & 0xF];
            dest[j++] = hex[ src[i]       & 0xF];
        }
        else
            dest[j++] = src[i];
    }
    dest[j] = '\0';
}

static void generate_dir_listing(struct connection *conn, const char *path) {
    char date[DATE_LEN], *spaces;
    struct dlent **list;
    ssize_t listsize;
    size_t maxlen = 2; /* There has to be ".." */
    int i;
    struct apbuf *listing;

    listsize = make_sorted_dirlist(path, &list);
    if (listsize == -1) {
        default_reply(conn, 500, "Internal Server Error",
                      "Couldn't list directory: %s", strerror(errno));
        return;
    }

    for (i=0; i<listsize; i++) {
        size_t tmp = strlen(list[i]->name);
        if (maxlen < tmp)
            maxlen = tmp;
    }

    listing = make_apbuf();
    append(listing, "<html>\n<head>\n <title>");
    append(listing, conn->url);
    append(listing, "</title>\n</head>\n<body>\n<h1>");
    append(listing, conn->url);
    append(listing, "</h1>\n<tt><pre>\n");

    spaces = xmalloc(maxlen);
    memset(spaces, ' ', maxlen);

    for (i=0; i<listsize; i++) {
        /* If a filename is made up of entirely unsafe chars,
         * the url would be three times its original length.
         */
        char safe_url[MAXNAMLEN*3 + 1];

        urlencode(list[i]->name, safe_url);

        append(listing, "<a href=\"");
        append(listing, safe_url);
        append(listing, "\">");
        append(listing, list[i]->name);
        append(listing, "</a>");

        if (list[i]->is_dir)
            append(listing, "/\n");
        else {
            appendl(listing, spaces, maxlen-strlen(list[i]->name));
            appendf(listing, "%10llu\n", llu(list[i]->size));
        }
    }

    cleanup_sorted_dirlist(list, listsize);
    free(list);
    free(spaces);

    rfc1123_date(date, now);
    append(listing,
     "</pre></tt>\n"
     "<hr>\n"
     "Generated by ");
    append(listing, pkgname);
    append(listing, " on ");
    append(listing, date);
    append(listing, "\n</body>\n</html>\n");

    conn->reply = listing->str;
    conn->reply_length = (off_t)listing->length;
    free(listing); /* don't free inside of listing */

    conn->header_length = xasprintf(&(conn->header),
     "HTTP/1.1 200 OK\r\n"
     "Date: %s\r\n"
     "Server: %s\r\n"
     "Accept-Ranges: bytes\r\n"
     "%s" /* keep-alive */
     "Content-Length: %llu\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     date, pkgname, keep_alive(conn), llu(conn->reply_length));

    conn->reply_type = REPLY_GENERATED;
    conn->http_code = 200;
}

/* Process a GET/HEAD request. */
static void process_get(struct connection *conn) {
    char *decoded_url, *target, *if_mod_since;
    char date[DATE_LEN], lastmod[DATE_LEN];
    const char *mimetype = NULL;
    struct stat filestat;

    /* work out path of file being requested */
    decoded_url = urldecode(conn->url);

    /* make sure it's safe */
    if (make_safe_url(decoded_url) == NULL) {
        default_reply(conn, 400, "Bad Request",
                      "You requested an invalid URL: %s", conn->url);
        free(decoded_url);
        return;
    }

    /* test the host against web forward options */
    {
        char *host = parse_field(conn, "Host: ");
        if(host) {
            if (debug)
                printf("host=\"%s\"\n", host);

            struct web_forward_record *ptr = web_forward;
            for(; ptr; ptr=ptr->next) {
                if (debug)
                    printf("test for web forward record \"%s\" -> \"%s\"\n", ptr->host, ptr->target);

                if(!strcasecmp(ptr->host, host)) {

                    redirect(conn, "%s%s", ptr->target, decoded_url);

                    free(decoded_url);
                    free(host);
                    return;
                    
                }
            }
            free(host);
        }
    }

    /* does it end in a slash? serve up url/index_name */
    if (decoded_url[strlen(decoded_url)-1] == '/') {
        xasprintf(&target, "%s%s%s", wwwroot, decoded_url, index_name);
        if (!file_exists(target)) {
            free(target);
            xasprintf(&target, "%s%s", wwwroot, decoded_url);
            generate_dir_listing(conn, target);
            free(target);
            free(decoded_url);
            return;
        }
        mimetype = url_content_type(index_name);
    }
    else {
        /* points to a file */
        xasprintf(&target, "%s%s", wwwroot, decoded_url);
        mimetype = url_content_type(decoded_url);
    }
    free(decoded_url);
    if (debug)
        printf("url=\"%s\", target=\"%s\", content-type=\"%s\"\n",
               conn->url, target, mimetype);

    /* open file */
    conn->reply_fd = open(target, O_RDONLY | O_NONBLOCK);
    free(target);

    if (conn->reply_fd == -1) {
        /* open() failed */
        if (errno == EACCES)
            default_reply(conn, 403, "Forbidden",
                "You don't have permission to access (%s).", conn->url);
        else if (errno == ENOENT)
            default_reply(conn, 404, "Not Found",
                "The URL you requested (%s) was not found.", conn->url);
        else
            default_reply(conn, 500, "Internal Server Error",
                "The URL you requested (%s) cannot be returned: %s.",
                conn->url, strerror(errno));

        return;
    }

    /* stat the file */
    if (fstat(conn->reply_fd, &filestat) == -1) {
        default_reply(conn, 500, "Internal Server Error",
            "fstat() failed: %s.", strerror(errno));
        return;
    }

    /* make sure it's a regular file */
    if (S_ISDIR(filestat.st_mode)) {
        redirect(conn, "%s/", conn->url);
        return;
    }
    else if (!S_ISREG(filestat.st_mode)) {
        default_reply(conn, 403, "Forbidden", "Not a regular file.");
        return;
    }

    conn->reply_type = REPLY_FROMFILE;
    rfc1123_date(lastmod, filestat.st_mtime);

    /* check for If-Modified-Since, may not have to send */
    if_mod_since = parse_field(conn, "If-Modified-Since: ");
    if ((if_mod_since != NULL) &&
            (strcmp(if_mod_since, lastmod) == 0)) {
        if (debug)
            printf("not modified since %s\n", if_mod_since);
        conn->http_code = 304;
        conn->header_length = xasprintf(&(conn->header),
         "HTTP/1.1 304 Not Modified\r\n"
         "Date: %s\r\n"
         "Server: %s\r\n"
         "Accept-Ranges: bytes\r\n"
         "%s" /* keep-alive */
         "\r\n",
         rfc1123_date(date, now), pkgname, keep_alive(conn));
        conn->reply_length = 0;
        conn->reply_type = REPLY_GENERATED;
        conn->header_only = 1;

        free(if_mod_since);
        return;
    }
    free(if_mod_since);

    if (conn->range_begin_given || conn->range_end_given) {
        off_t from, to;

        if (conn->range_begin_given && conn->range_end_given) {
            /* 100-200 */
            from = conn->range_begin;
            to = conn->range_end;

            /* clamp end to filestat.st_size-1 */
            if (to > (filestat.st_size - 1))
                to = filestat.st_size - 1;
        }
        else if (conn->range_begin_given && !conn->range_end_given) {
            /* 100- :: yields 100 to end */
            from = conn->range_begin;
            to = filestat.st_size - 1;
        }
        else if (!conn->range_begin_given && conn->range_end_given) {
            /* -200 :: yields last 200 */
            to = filestat.st_size - 1;
            from = to - conn->range_end + 1;

            /* clamp start */
            if (from < 0)
                from = 0;
        }
        else
            errx(1, "internal error - from/to mismatch");

        if (from >= filestat.st_size) {
            default_reply(conn, 416, "Requested Range Not Satisfiable",
                "You requested a range outside of the file.");
            return;
        }

        if (to < from) {
            default_reply(conn, 416, "Requested Range Not Satisfiable",
                "You requested a backward range.");
            return;
        }

        conn->reply_start = from;
        conn->reply_length = to - from + 1;

        conn->header_length = xasprintf(&(conn->header),
            "HTTP/1.1 206 Partial Content\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "%s" /* keep-alive */
            "Content-Length: %llu\r\n"
            "Content-Range: bytes %llu-%llu/%llu\r\n"
            "Content-Type: %s\r\n"
            "Last-Modified: %s\r\n"
            "\r\n"
            ,
            rfc1123_date(date, now), pkgname, keep_alive(conn),
            llu(conn->reply_length), llu(from), llu(to),
            llu(filestat.st_size), mimetype, lastmod
        );
        conn->http_code = 206;
        if (debug)
            printf("sending %llu-%llu/%llu\n",
                   llu(from), llu(to), llu(filestat.st_size));
    }
    else {
        /* no range stuff */
        conn->reply_length = filestat.st_size;
        conn->header_length = xasprintf(&(conn->header),
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "%s" /* keep-alive */
            "Content-Length: %llu\r\n"
            "Content-Type: %s\r\n"
            "Last-Modified: %s\r\n"
            "\r\n"
            ,
            rfc1123_date(date, now), pkgname, keep_alive(conn),
            llu(conn->reply_length), mimetype, lastmod
        );
        conn->http_code = 200;
    }
}

/* Process a request: build the header and reply, advance state. */
static void process_request(struct connection *conn) {
    num_requests++;
    if (!parse_request(conn)) {
        default_reply(conn, 400, "Bad Request",
            "You sent a request that the server couldn't understand.");
    }
    else if (strcmp(conn->method, "GET") == 0) {
        process_get(conn);
    }
    else if (strcmp(conn->method, "HEAD") == 0) {
        process_get(conn);
        conn->header_only = 1;
    }
    else if ((strcmp(conn->method, "OPTIONS") == 0) ||
             (strcmp(conn->method, "POST") == 0) ||
             (strcmp(conn->method, "PUT") == 0) ||
             (strcmp(conn->method, "DELETE") == 0) ||
             (strcmp(conn->method, "TRACE") == 0) ||
             (strcmp(conn->method, "CONNECT") == 0)) {
        default_reply(conn, 501, "Not Implemented",
                      "The method you specified (%s) is not implemented.",
                      conn->method);
    }
    else {
        default_reply(conn, 400, "Bad Request",
                      "%s is not a valid HTTP/1.1 method.", conn->method);
    }

    /* advance state */
    conn->state = SEND_HEADER;

    /* request not needed anymore */
    free(conn->request);
    conn->request = NULL; /* important: don't free it again later */
}

/* Receiving request. */
static void poll_recv_request(struct connection *conn) {
    char buf[1<<15];
    ssize_t recvd;

    assert(conn->state == RECV_REQUEST);
    recvd = recv(conn->socket, buf, sizeof(buf), 0);
    if (debug)
        printf("poll_recv_request(%d) got %d bytes\n",
               conn->socket, (int)recvd);
    if (recvd < 1) {
        if (recvd == -1) {
            if (errno == EAGAIN) {
                if (debug) printf("poll_recv_request would have blocked\n");
                return;
            }
            if (debug) printf("recv(%d) error: %s\n",
                conn->socket, strerror(errno));
        }
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    conn->last_active = now;

    /* append to conn->request */
    assert(recvd > 0);
    conn->request = xrealloc(
        conn->request, conn->request_length + (size_t)recvd + 1);
    memcpy(conn->request+conn->request_length, buf, (size_t)recvd);
    conn->request_length += (size_t)recvd;
    conn->request[conn->request_length] = 0;
    total_in += (size_t)recvd;

    /* process request if we have all of it */
    if ((conn->request_length > 2) &&
        (memcmp(conn->request+conn->request_length-2, "\n\n", 2) == 0))
            process_request(conn);
    else if ((conn->request_length > 4) &&
        (memcmp(conn->request+conn->request_length-4, "\r\n\r\n", 4) == 0))
            process_request(conn);

    /* die if it's too large */
    if (conn->request_length > MAX_REQUEST_LENGTH) {
        default_reply(conn, 413, "Request Entity Too Large",
                      "Your request was dropped because it was too long.");
        conn->state = SEND_HEADER;
    }

    /* if we've moved on to the next state, try to send right away, instead of
     * going through another iteration of the select() loop.
     */
    if (conn->state == SEND_HEADER)
        poll_send_header(conn);
}

/* Sending header.  Assumes conn->header is not NULL. */
static void poll_send_header(struct connection *conn) {
    ssize_t sent;

    assert(conn->state == SEND_HEADER);
    assert(conn->header_length == strlen(conn->header));

    sent = send(conn->socket,
                conn->header + conn->header_sent,
                conn->header_length - conn->header_sent,
                0);
    conn->last_active = now;
    if (debug)
        printf("poll_send_header(%d) sent %d bytes\n",
               conn->socket, (int)sent);

    /* handle any errors (-1) or closure (0) in send() */
    if (sent < 1) {
        if ((sent == -1) && (errno == EAGAIN)) {
            if (debug) printf("poll_send_header would have blocked\n");
            return;
        }
        if (debug && (sent == -1))
            printf("send(%d) error: %s\n", conn->socket, strerror(errno));
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    assert(sent > 0);
    conn->header_sent += (size_t)sent;
    conn->total_sent += (size_t)sent;
    total_out += (size_t)sent;

    /* check if we're done sending header */
    if (conn->header_sent == conn->header_length) {
        if (conn->header_only)
            conn->state = DONE;
        else {
            conn->state = SEND_REPLY;
            /* go straight on to body, don't go through another iteration of
             * the select() loop.
             */
            poll_send_reply(conn);
        }
    }
}

/* Send chunk on socket <s> from FILE *fp, starting at <ofs> and of size
 * <size>.  Use sendfile() if possible since it's zero-copy on some platforms.
 * Returns the number of bytes sent, 0 on closure, -1 if send() failed, -2 if
 * read error.
 */
static ssize_t send_from_file(const int s, const int fd,
        off_t ofs, size_t size) {
#ifdef __FreeBSD__
    off_t sent;
    int ret = sendfile(fd, s, ofs, size, NULL, &sent, 0);

    /* It is possible for sendfile to send zero bytes due to a blocking
     * condition.  Handle this correctly.
     */
    if (ret == -1)
        if (errno == EAGAIN)
            if (sent == 0)
                return -1;
            else
                return sent;
        else
            return -1;
    else
        return size;
#else
#if defined(__linux) || defined(__sun__)
    /* Limit truly ridiculous (LARGEFILE) requests. */
    if (size > 1<<20)
        size = 1<<20;
    return sendfile(s, fd, &ofs, size);
#else
    /* Fake sendfile() with read(). */
# ifndef min
#  define min(a,b) ( ((a)<(b)) ? (a) : (b) )
# endif
    char buf[1<<15];
    size_t amount = min(sizeof(buf), size);
    ssize_t numread;

    if (lseek(fd, ofs, SEEK_SET) == -1)
        err(1, "fseek(%d)", (int)ofs);
    numread = read(fd, buf, amount);
    if (numread == 0) {
        fprintf(stderr, "premature eof on fd %d\n", fd);
        return -1;
    }
    else if (numread == -1) {
        fprintf(stderr, "error reading on fd %d: %s", fd, strerror(errno));
        return -1;
    }
    else if ((size_t)numread != amount) {
        fprintf(stderr, "read %d bytes, expecting %u bytes on fd %d\n",
            numread, amount, fd);
        return -1;
    }
    else
        return send(s, buf, amount, 0);
#endif
#endif
}

/* Sending reply. */
static void poll_send_reply(struct connection *conn)
{
    ssize_t sent;

    assert(conn->state == SEND_REPLY);
    assert(!conn->header_only);
    if (conn->reply_type == REPLY_GENERATED) {
        assert(conn->reply_length >= conn->reply_sent);
        sent = send(conn->socket,
            conn->reply + conn->reply_start + conn->reply_sent,
            (size_t)(conn->reply_length - conn->reply_sent), 0);
    }
    else {
        errno = 0;
        assert(conn->reply_length >= conn->reply_sent);
        sent = send_from_file(conn->socket, conn->reply_fd,
            conn->reply_start + conn->reply_sent,
            (size_t)(conn->reply_length - conn->reply_sent));
        if (debug && (sent < 1))
            printf("send_from_file returned %lld (errno=%d %s)\n",
                (long long)sent, errno, strerror(errno));
    }
    conn->last_active = now;
    if (debug)
        printf("poll_send_reply(%d) sent %d: %lld+[%lld-%lld] of %lld\n",
               conn->socket, (int)sent, llu(conn->reply_start),
               llu(conn->reply_sent), llu(conn->reply_sent + sent - 1),
               llu(conn->reply_length));

    /* handle any errors (-1) or closure (0) in send() */
    if (sent < 1) {
        if (sent == -1) {
            if (errno == EAGAIN) {
                if (debug)
                    printf("poll_send_reply would have blocked\n");
                return;
            }
            if (debug)
                printf("send(%d) error: %s\n", conn->socket, strerror(errno));
        }
        else if (sent == 0) {
            if (debug)
                printf("send(%d) closure\n", conn->socket);
        }
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    conn->reply_sent += sent;
    conn->total_sent += (size_t)sent;
    total_out += (size_t)sent;

    /* check if we're done sending */
    if (conn->reply_sent == conn->reply_length)
        conn->state = DONE;
}

/* Main loop of the httpd - a select() and then delegation to accept
 * connections, handle receiving of requests, and sending of replies.
 */
static void httpd_poll(void) {
    fd_set recv_set, send_set;
    int max_fd, select_ret;
    struct connection *conn, *next;
    int bother_with_timeout = 0;
    struct timeval timeout;

    timeout.tv_sec = idletime;
    timeout.tv_usec = 0;

    FD_ZERO(&recv_set);
    FD_ZERO(&send_set);
    max_fd = 0;

    /* set recv/send fd_sets */
#define MAX_FD_SET(sock, fdset) { FD_SET(sock,fdset); \
                                max_fd = (max_fd<sock) ? sock : max_fd; }
    MAX_FD_SET(sockin, &recv_set);

    LIST_FOREACH_SAFE(conn, &connlist, entries, next) {
        poll_check_timeout(conn);
        switch (conn->state) {
        case DONE:
            /* do nothing */
            break;

        case RECV_REQUEST:
            MAX_FD_SET(conn->socket, &recv_set);
            bother_with_timeout = 1;
            break;

        case SEND_HEADER:
        case SEND_REPLY:
            MAX_FD_SET(conn->socket, &send_set);
            bother_with_timeout = 1;
            break;
        }
    }
#undef MAX_FD_SET

    /* -select- */
    select_ret = select(max_fd + 1, &recv_set, &send_set, NULL,
        (bother_with_timeout) ? &timeout : NULL);
    if (select_ret == 0) {
        if (!bother_with_timeout)
            errx(1, "select() timed out");
        else
            return;
    }
    if (select_ret == -1) {
        if (errno == EINTR)
            return; /* interrupted by signal */
        else
            err(1, "select() failed");
    }

    /* update time */
    now = time(NULL);

    /* poll connections that select() says need attention */
    if (FD_ISSET(sockin, &recv_set))
        accept_connection();

    LIST_FOREACH_SAFE(conn, &connlist, entries, next) {
        switch (conn->state) {
        case RECV_REQUEST:
            if (FD_ISSET(conn->socket, &recv_set)) poll_recv_request(conn);
            break;

        case SEND_HEADER:
            if (FD_ISSET(conn->socket, &send_set)) poll_send_header(conn);
            break;

        case SEND_REPLY:
            if (FD_ISSET(conn->socket, &send_set)) poll_send_reply(conn);
            break;

        case DONE:
            /* (handled later; ignore for now as it's a valid state) */
            break;
        }

        if (conn->state == DONE) {
            /* clean out finished connection */
            if (conn->conn_close) {
                LIST_REMOVE(conn, entries);
                free_connection(conn);
                free(conn);
            } else {
                recycle_connection(conn);
                /* and go right back to recv_request without going through
                 * select() again.
                 */
                poll_recv_request(conn);
            }
        }
    }
}

/* Daemonize helpers. */
#define PATH_DEVNULL "/dev/null"
static int lifeline[2] = { -1, -1 };
static int fd_null = -1;

static void daemonize_start(void) {
    pid_t f, w;

    if (pipe(lifeline) == -1)
        err(1, "pipe(lifeline)");

    fd_null = open(PATH_DEVNULL, O_RDWR, 0);
    if (fd_null == -1)
        err(1, "open(" PATH_DEVNULL ")");

    f = fork();
    if (f == -1)
        err(1, "fork");
    else if (f != 0) {
        /* parent: wait for child */
        char tmp[1];
        int status;

        if (close(lifeline[1]) == -1)
            warn("close lifeline in parent");
        if (read(lifeline[0], tmp, sizeof(tmp)) == -1)
            warn("read lifeline in parent");
        w = waitpid(f, &status, WNOHANG);
        if (w == -1)
            err(1, "waitpid");
        else if (w == 0)
            /* child is running happily */
            exit(EXIT_SUCCESS);
        else
            /* child init failed, pass on its exit status */
            exit(WEXITSTATUS(status));
    }
    /* else we are the child: continue initializing */
}

static void daemonize_finish(void) {
    if (fd_null == -1)
        return; /* didn't daemonize_start() so we're not daemonizing */

    if (setsid() == -1)
        err(1, "setsid");
    if (close(lifeline[0]) == -1)
        warn("close read end of lifeline in child");
    if (close(lifeline[1]) == -1)
        warn("couldn't cut the lifeline");

    /* close all our std fds */
    if (dup2(fd_null, STDIN_FILENO) == -1)
        warn("dup2(stdin)");
    if (dup2(fd_null, STDOUT_FILENO) == -1)
        warn("dup2(stdout)");
    if (dup2(fd_null, STDERR_FILENO) == -1)
        warn("dup2(stderr)");
    if (fd_null > 2)
        close(fd_null);
}

/* [->] pidfile helpers, based on FreeBSD src/lib/libutil/pidfile.c,v 1.3
 * Original was copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 */
static int pidfile_fd = -1;
#define PIDFILE_MODE 0600

static void pidfile_remove(void) {
    if (unlink(pidfile_name) == -1)
        err(1, "unlink(pidfile) failed");
 /* if (flock(pidfile_fd, LOCK_UN) == -1)
        err(1, "unlock(pidfile) failed"); */
    xclose(pidfile_fd);
    pidfile_fd = -1;
}

static int pidfile_read(void) {
    char buf[16], *endptr;
    int fd, i, pid;

    fd = open(pidfile_name, O_RDONLY);
    if (fd == -1)
        err(1, " after create failed");

    i = (int)read(fd, buf, sizeof(buf) - 1);
    if (i == -1)
        err(1, "read from pidfile failed");
    xclose(fd);
    buf[i] = '\0';

    pid = (int)strtoul(buf, &endptr, 10);
    if (endptr != &buf[i])
        err(1, "invalid pidfile contents: \"%s\"", buf);
    return (pid);
}

static void pidfile_create(void) {
    int error, fd;
    char pidstr[16];

    /* Open the PID file and obtain exclusive lock. */
    fd = open(pidfile_name,
        O_WRONLY | O_CREAT | O_EXLOCK | O_TRUNC | O_NONBLOCK, PIDFILE_MODE);
    if (fd == -1) {
        if ((errno == EWOULDBLOCK) || (errno == EEXIST))
            errx(1, "daemon already running with PID %d", pidfile_read());
        else
            err(1, "can't create pidfile %s", pidfile_name);
    }
    pidfile_fd = fd;

    if (ftruncate(fd, 0) == -1) {
        error = errno;
        pidfile_remove();
        errno = error;
        err(1, "ftruncate() failed");
    }

    snprintf(pidstr, sizeof(pidstr), "%u", getpid());
    if (pwrite(fd, pidstr, strlen(pidstr), 0) != (ssize_t)strlen(pidstr)) {
        error = errno;
        pidfile_remove();
        errno = error;
        err(1, "pwrite() failed");
    }
}
/* [<-] end of pidfile helpers. */

/* Close all sockets and FILEs and exit. */
static void stop_running(int sig) {
    running = 0;
    fprintf(stderr, "\ncaught signal %s, stopping\n", strsignal(sig));
}

/* Execution starts here. */
int main(int argc, char **argv) {
    printf("%s, %s.\n", pkgname, copyright);
    parse_default_extension_map();
    parse_commandline(argc, argv);
    /* parse_commandline() might override parts of the extension map by
     * parsing a user-specified file.
     */
    sort_mime_map();
    xasprintf(&keep_alive_field, "Keep-Alive: timeout=%d\r\n", idletime);
    init_sockin();

    /* open logfile */
    if (logfile_name == NULL)
        logfile = stdout;
    else {
        logfile = fopen(logfile_name, "ab");
        if (logfile == NULL)
            err(1, "opening logfile: fopen(\"%s\")", logfile_name);
    }

    if (want_daemon)
        daemonize_start();

    /* signals */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        err(1, "signal(ignore SIGPIPE)");
    if (signal(SIGINT, stop_running) == SIG_ERR)
        err(1, "signal(SIGINT)");
    if (signal(SIGTERM, stop_running) == SIG_ERR)
        err(1, "signal(SIGTERM)");

    /* security */
    if (want_chroot) {
        tzset(); /* read /etc/localtime before we chroot */
        if (chdir(wwwroot) == -1)
            err(1, "chdir(%s)", wwwroot);
        if (chroot(wwwroot) == -1)
            err(1, "chroot(%s)", wwwroot);
        printf("chrooted to `%s'\n", wwwroot);
        wwwroot[0] = '\0'; /* empty string */
    }
    if (drop_gid != INVALID_GID) {
        if (setgid(drop_gid) == -1)
            err(1, "setgid(%d)", drop_gid);
        printf("set gid to %d\n", drop_gid);
    }
    if (drop_uid != INVALID_UID) {
        if (setuid(drop_uid) == -1)
            err(1, "setuid(%d)", drop_uid);
        printf("set uid to %d\n", drop_uid);
    }

    /* create pidfile */
    if (pidfile_name) pidfile_create();

    if (want_daemon) daemonize_finish();

    /* main loop */
    while (running) httpd_poll();

    /* clean exit */
    xclose(sockin);
    if (logfile != NULL) fclose(logfile);
    if (pidfile_name) pidfile_remove();

    /* close and free connections */
    {
        struct connection *conn, *next;

        LIST_FOREACH_SAFE(conn, &connlist, entries, next) {
            LIST_REMOVE(conn, entries);
            free_connection(conn);
            free(conn);
        }
    }

    /* free the mallocs */
    {
        size_t i;

        for (i=0; i<mime_map_size; i++) {
            free(mime_map[i].extension);
            free(mime_map[i].mimetype);
        }
        free(mime_map);
        free(keep_alive_field);
        free(wwwroot);
    }

    /* usage stats */
    {
        struct rusage r;

        getrusage(RUSAGE_SELF, &r);
        printf("CPU time used: %u.%02u user, %u.%02u system\n",
            (unsigned int)r.ru_utime.tv_sec,
                (unsigned int)(r.ru_utime.tv_usec/10000),
            (unsigned int)r.ru_stime.tv_sec,
                (unsigned int)(r.ru_stime.tv_usec/10000)
        );
        printf("Requests: %llu\n", llu(num_requests));
        printf("Bytes: %llu in, %llu out\n", llu(total_in), llu(total_out));
    }

    return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
