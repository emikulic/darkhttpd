/* -darkhttpd-
 * Copyright (c) 2003, Emil Mikulic.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

static const char pkgname[]   = "darkhttpd/1.0";
static const char copyright[] = "copyright (c) 2003 Emil Mikulic";
static const char rcsid[]     =
    "$Id$";

#ifdef __linux
#define _GNU_SOURCE /* for strsignal() and vasprintf() */
#include <sys/sendfile.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
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

#ifdef NDEBUG
 #define safefree free
 static void debugf(const char *format, ...) { }
#else
 #define safefree(x) do { free(x); x = NULL; } while(0)
 #define debugf printf
#endif

#ifndef min
#define min(a,b) ( ((a)<(b)) ? (a) : (b) )
#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif



#if defined(__FreeBSD__) || defined(__linux)
#include <err.h>
#else
/* ---------------------------------------------------------------------------
 * errx - prints "error: [...]\n" to stderr and exit()s with [code]
 *
 * Replacement for the BSD errx() which is usually in <err.h>
 */
static void errx(const int code, const char *format, ...)
{
   va_list va;

   va_start(va, format);
   fprintf(stderr, "error: ");
   vfprintf(stderr, format, va);
   fprintf(stderr, "\n");
   va_end(va);

   exit(code);
}



/* ---------------------------------------------------------------------------
 * err - prints "error: [...]: strerror\n" to stderr and exit()s with [code]
 *
 * Replacement for the BSD err() which is usually in <err.h>
 */
void err(const int code, const char *format, ...)
{
   va_list va;

   va_start(va, format);
   fprintf(stderr, "error: ");
   vfprintf(stderr, format, va);
   fprintf(stderr, ": %s\n", strerror(errno));
   va_end(va);

   exit(code);
}
#endif



/* ---------------------------------------------------------------------------
 * LIST_* macros taken from FreeBSD's src/sys/sys/queue.h,v 1.56
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

#define LIST_FOREACH(var, head, field)                                  \
        for ((var) = LIST_FIRST((head));                                \
            (var);                                                      \
            (var) = LIST_NEXT((var), field))

#define LIST_FOREACH_SAFE(var, head, field, tvar)                       \
    for ((var) = LIST_FIRST((head));                                    \
        (var) && ((tvar) = LIST_NEXT((var), field), 1);                 \
        (var) = (tvar))

#define LIST_INIT(head) do {                                            \
        LIST_FIRST((head)) = NULL;                                      \
} while (0)

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
/* ------------------------------------------------------------------------ */



LIST_HEAD(conn_list_head, connection) connlist =
    LIST_HEAD_INITIALIZER(conn_list_head);

struct connection
{
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
    char *method, *uri, *referer, *user_agent;
    size_t range_begin, range_end;
    int range_begin_given, range_end_given;

    char *header;
    size_t header_length, header_sent;
    int header_dont_free, header_only, http_code, conn_close;

    enum { REPLY_GENERATED, REPLY_FROMFILE } reply_type;
    char *reply;
    int reply_dont_free;
    int reply_fd;
    size_t reply_start, reply_length, reply_sent;

    unsigned int total_sent; /* header + body = total, for logging */
};



struct mime_mapping
{
    char *extension, *mimetype;
};

struct mime_mapping *mime_map = NULL;
size_t mime_map_size = 0;
size_t longest_ext = 0;



/* If a connection is idle for idletime seconds or more, it gets closed and
 * removed from the connlist.  Set to 0 to remove the timeout
 * functionality.
 */
static int idletime = 60;
static char *keep_alive_field = NULL;



/* To prevent a malformed request from eating up too much memory, die once the
 * request exceeds this many bytes:
 */
#define MAX_REQUEST_LENGTH 4000



/* Defaults can be overridden on the command-line */
static in_addr_t bindaddr = INADDR_ANY;
static unsigned short bindport = 80;
static int max_connections = -1;        /* kern.ipc.somaxconn */
static const char *index_name = "index.html";

static int sockin = -1;             /* socket to accept connections from */
static char *wwwroot = NULL;        /* a path name */
static char *logfile_name = NULL;   /* NULL = no logging */
static FILE *logfile = NULL;
static int want_chroot = 0, want_accf = 0;

#define INVALID_UID ((uid_t) -1)
#define INVALID_GID ((gid_t) -1)

static uid_t drop_uid = INVALID_UID;
static gid_t drop_gid = INVALID_GID;

/* Default mimetype mappings - make sure this array is NULL terminated. */
static const char *default_extension_map[] = {
    "text/html          html htm",
    "image/png          png",
    "image/jpeg         jpeg jpe jpg",
    "image/gif          gif",
    "audio/mpeg         mp2 mp3 mpga",
    "application/ogg    ogg",
    "text/css           css",
    "text/plain         txt asc",
    "text/xml           xml",
    "video/mpeg         mpeg mpe mpg",
    "video/x-msvideo    avi",
    NULL
};

static const char default_mimetype[] = "application/octet-stream";



/* Connection or Keep-Alive field, depending on conn_close. */
#define keep_alive(conn) ((conn)->conn_close ? \
    "Connection: close\r\n" : keep_alive_field)



/* ---------------------------------------------------------------------------
 * close that dies on error.
 */
static void xclose(const int fd)
{
    if (close(fd) == -1) err(1, "close()");
}



/* ---------------------------------------------------------------------------
 * malloc that errx()s if it can't allocate.
 */
static void *xmalloc(const size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) errx(1, "can't allocate %u bytes", size);
    return ptr;
}



/* ---------------------------------------------------------------------------
 * realloc() that errx()s if it can't allocate.
 */
static void *xrealloc(void *original, const size_t size)
{
    void *ptr = realloc(original, size);
    if (ptr == NULL) errx(1, "can't reallocate %u bytes", size);
    return ptr;
}



/* ---------------------------------------------------------------------------
 * strdup() that errx()s if it can't allocate.
 */
static char *xstrdup(const char *src)
{
    size_t len = strlen(src) + 1;
    char *dest = xmalloc(len);
    memcpy(dest, src, len);
    return dest;
}



#ifdef __sun__ /* unimpressed by Solaris */
static int vasprintf(char **strp, const char *fmt, va_list ap)
{
    char tmp;
    int result = vsnprintf(&tmp, 1, fmt, ap);
    *strp = xmalloc(result+1);
    result = vsnprintf(*strp, result+1, fmt, ap);
    return result;
}
#endif



/* ---------------------------------------------------------------------------
 * asprintf() that errx()s if it fails.
 */
static unsigned int xvasprintf(char **ret, const char *format, va_list ap)
{
    int len = vasprintf(ret, format, ap);
    if (ret == NULL || len == -1) errx(1, "out of memory in vasprintf()");
    return (unsigned int)len;
}



/* ---------------------------------------------------------------------------
 * asprintf() that errx()s if it fails.
 */
static unsigned int xasprintf(char **ret, const char *format, ...)
{
    va_list va;
    unsigned int len;

    va_start(va, format);
    len = xvasprintf(ret, format, va);
    va_end(va);
    return len;
}



/* ---------------------------------------------------------------------------
 * Append buffer code.  A somewhat efficient string buffer with pool-based
 * reallocation.
 */
#define APBUF_INIT 4096
#define APBUF_GROW APBUF_INIT
struct apbuf
{
    size_t length, pool;
    char *str;
};



static struct apbuf *make_apbuf(void)
{
    struct apbuf *buf = xmalloc(sizeof(struct apbuf));
    buf->length = 0;
    buf->pool = APBUF_INIT;
    buf->str = xmalloc(buf->pool);
    return buf;
}



static void appendl(struct apbuf *buf, const char *s, const size_t len)
{
    if (buf->pool <= buf->length + len)
    {
        /* pool has dried up */
        while (buf->pool <= buf->length + len) buf->pool += APBUF_GROW;
        buf->str = xrealloc(buf->str, buf->pool);
    }

    memcpy(buf->str + buf->length, s, len+1);
    buf->length += len;
}



static void append(struct apbuf *buf, const char *s)
{
    size_t len = strlen(s);
    appendl(buf, s, len);
}



static void appendf(struct apbuf *buf, const char *format, ...)
{
    char *tmp;
    va_list va;
    size_t len;

    va_start(va, format);
    len = xvasprintf(&tmp, format, va);
    va_end(va);

    appendl(buf, tmp, len);
    safefree(tmp);
}



/* ---------------------------------------------------------------------------
 * Make the specified socket non-blocking.
 */
static void nonblock_socket(const int sock)
{
    assert(sock != -1);
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
        err(1, "fcntl() to set O_NONBLOCK");
}



/* ---------------------------------------------------------------------------
 * Enable acceptfilter on the specified socket.  (This is only available on
 * FreeBSD)
 */
static void acceptfilter_socket(const int sock)
{
    if (want_accf)
    {
#if defined(__FreeBSD__)
        struct accept_filter_arg filt = {"httpready", ""};
        if (setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER,
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



/* ---------------------------------------------------------------------------
 * Split string out of src with range [left:right-1]
 */
static char *split_string(const char *src,
    const size_t left, const size_t right)
{
    char *dest;
    assert(left <= right);
    assert(left < strlen(src));   /* [left means must be smaller */
    assert(right <= strlen(src)); /* right) means can be equal or smaller */

    dest = xmalloc(right - left + 1);
    memcpy(dest, src+left, right-left);
    dest[right-left] = '\0';
    return dest;
}



/* ---------------------------------------------------------------------------
 * Consolidate slashes in-place by shifting parts of the string over repeated
 * slashes.
 */
static void consolidate_slashes(char *s)
{
    size_t left = 0, right = 0;
    int saw_slash = 0;

    assert(s != NULL);

    while (s[right] != '\0')
    {
        if (saw_slash)
        {
            if (s[right] == '/') right++;
            else
            {
                saw_slash = 0;
                s[left++] = s[right++];
            }
        }
        else
        {
            if (s[right] == '/') saw_slash++;
            s[left++] = s[right++];
        }
    }
    s[left] = '\0';
}



/* ---------------------------------------------------------------------------
 * Resolve /./ and /../ in a URI, returing a new, safe URI, or NULL if the URI
 * is invalid/unsafe.  Returned buffer needs to be deallocated.
 */
static char *make_safe_uri(char *uri)
{
    char **elem, *out;
    unsigned int slashes = 0, elements = 0;
    size_t urilen, i, j, pos;

    assert(uri != NULL);
    if (uri[0] != '/') return NULL;
    consolidate_slashes(uri);
    urilen = strlen(uri);

    /* count the slashes */
    for (i=0, slashes=0; i<urilen; i++)
        if (uri[i] == '/') slashes++;

    /* make an array for the URI elements */
    elem = xmalloc(sizeof(char*) * slashes);
    for (i=0; i<slashes; i++) elem[i] = NULL;

    /* split by slashes and build elem[] array */
    for (i=1; i<urilen;)
    {
        /* look for the next slash */
        for (j=i; j<urilen && uri[j] != '/'; j++)
            ;

        /* process uri[i,j) */
        if ((j == i+1) && (uri[i] == '.'))
            /* "." */;
        else if ((j == i+2) && (uri[i] == '.') && (uri[i+1] == '.'))
        {
            /* ".." */
            if (elements == 0)
            {
                /* unsafe string so free elem[]; all its elements are free at
                 * this point.
                 */
                safefree(elem);
                return NULL;
            }
            else
            {
                elements--;
                safefree(elem[elements]);
            }
        }
        else elem[elements++] = split_string(uri, i, j);

        i = j + 1; /* uri[j] is a slash - move along one */
    }

    /* reassemble */
    out = xmalloc(urilen+1); /* it won't expand */
    pos = 0;
    for (i=0; i<elements; i++)
    {
        size_t delta = strlen(elem[i]);

        assert(pos <= urilen);
        out[pos++] = '/';

        assert(pos+delta <= urilen);
        memcpy(out+pos, elem[i], delta);
        safefree(elem[i]);
        pos += delta;
    }
    free(elem);

    if ((elements == 0) || (uri[urilen-1] == '/')) out[pos++] = '/';
    assert(pos <= urilen);
    out[pos] = '\0';

    /* shorten buffer if necessary */
    if (pos != urilen) out = xrealloc(out, strlen(out)+1);

    return out;
}



/* ---------------------------------------------------------------------------
 * Associates an extension with a mimetype in the mime_map.  Entries are in
 * unsorted order.  Makes copies of extension and mimetype strings.
 */
static void add_mime_mapping(const char *extension, const char *mimetype)
{
    size_t i;
    assert(strlen(extension) > 0);
    assert(strlen(mimetype) > 0);

    /* update longest_ext */
    i = strlen(extension);
    if (i > longest_ext) longest_ext = i;

    /* look through list and replace an existing entry if possible */
    for (i=0; i<mime_map_size; i++)
        if (strcmp(mime_map[i].extension, extension) == 0)
        {
            safefree(mime_map[i].mimetype);
            mime_map[i].mimetype = xstrdup(mimetype);
            return;
        }

    /* no replacement - add a new entry */
    mime_map_size++;
    mime_map = xrealloc(mime_map,
        sizeof(struct mime_mapping) * mime_map_size);
    mime_map[mime_map_size-1].extension = xstrdup(extension);
    mime_map[mime_map_size-1].mimetype = xstrdup(mimetype);
}



/* ---------------------------------------------------------------------------
 * qsort() the mime_map.  The map must be sorted before it can be searched
 * through.
 */
static int mime_mapping_cmp(const void *a, const void *b)
{
    return strcmp( ((const struct mime_mapping *)a)->extension,
                   ((const struct mime_mapping *)b)->extension );
}

static void sort_mime_map(void)
{
    qsort(mime_map, mime_map_size, sizeof(struct mime_mapping),
        mime_mapping_cmp);
}



/* ---------------------------------------------------------------------------
 * Parses a mime.types line and adds the parsed data to the mime_map.
 */
static void parse_mimetype_line(const char *line)
{
    unsigned int pad, bound1, lbound, rbound;

    /* parse mimetype */
    for (pad=0; line[pad] == ' ' || line[pad] == '\t'; pad++);
    if (line[pad] == '\0' || /* empty line */
        line[pad] == '#')    /* comment */
        return;

    for (bound1=pad+1;
        line[bound1] != ' ' &&
        line[bound1] != '\t';
        bound1++)
    {
        if (line[bound1] == '\0') return; /* malformed line */
    }

    lbound = bound1;
    for (;;)
    {
        char *mimetype, *extension;

        /* find beginning of extension */
        for (; line[lbound] == ' ' || line[lbound] == '\t'; lbound++);
        if (line[lbound] == '\0') return; /* end of line */

        /* find end of extension */
        for (rbound = lbound;
            line[rbound] != ' ' &&
            line[rbound] != '\t' &&
            line[rbound] != '\0';
            rbound++);

        mimetype = split_string(line, pad, bound1);
        extension = split_string(line, lbound, rbound);
        add_mime_mapping(extension, mimetype);
        safefree(mimetype);
        safefree(extension);

        if (line[rbound] == '\0') return; /* end of line */
        else lbound = rbound + 1;
    }
}



/* ---------------------------------------------------------------------------
 * Adds contents of default_extension_map[] to mime_map list.  The array must
 * be NULL terminated.
 */
static void parse_default_extension_map(void)
{
    int i;

    for (i=0; default_extension_map[i] != NULL; i++)
        parse_mimetype_line(default_extension_map[i]);
}



/* ---------------------------------------------------------------------------
 * read_line - read a line from [fp], return its contents in a
 * dynamically allocated buffer, not including the line ending.
 *
 * Handles CR, CRLF and LF line endings, as well as NOEOL correctly.  If
 * already at EOF, returns NULL.  Will err() or errx() in case of
 * unexpected file error or running out of memory.
 */
static char *read_line(FILE *fp)
{
   char *buf;
   long startpos, endpos;
   size_t linelen, numread;
   int c;

   startpos = ftell(fp);
   if (startpos == -1) err(1, "ftell()");

   /* find end of line (or file) */
   linelen = 0;
   for (;;)
   {
      c = fgetc(fp);
      if (c == EOF || c == (int)'\n' || c == (int)'\r') break;
      linelen++;
   }

   /* return NULL on EOF (and empty line) */
   if (linelen == 0 && c == EOF) return NULL;

   endpos = ftell(fp);
   if (endpos == -1) err(1, "ftell()");

   /* skip CRLF */
   if (c == (int)'\r' && fgetc(fp) == (int)'\n') endpos++;

   buf = (char*)xmalloc(linelen + 1);

   /* rewind file to where the line stared and load the line */
   if (fseek(fp, startpos, SEEK_SET) == -1) err(1, "fseek()");
   numread = fread(buf, 1, linelen, fp);
   if (numread != linelen)
      errx(1, "fread() %u bytes, expecting %u bytes", numread, linelen);

   /* terminate buffer */
   buf[linelen] = 0;

   /* advance file pointer over the endline */
   if (fseek(fp, endpos, SEEK_SET) == -1) err(1, "fseek()");

   return buf;
}



/* ---------------------------------------------------------------------------
 * Removes the ending newline in a string, if there is one.
 */
static void chomp(char *str)
{
   size_t pos = strlen(str) - 1;
   if ((pos >= 0) && (str[pos] == '\n')) str[pos] = '\0';
}



/* ---------------------------------------------------------------------------
 * Adds contents of specified file to mime_map list.
 */
static void parse_extension_map_file(const char *filename)
{
    char *buf;
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) err(1, "fopen(\"%s\")", filename);

    while ( (buf = read_line(fp)) != NULL )
    {
        chomp(buf);
        parse_mimetype_line(buf);
        safefree(buf);
    }

    fclose(fp);
}



/* ---------------------------------------------------------------------------
 * Uses the mime_map to determine a Content-Type: for a requested URI.  This
 * bsearch()es mime_map, so make sure it's sorted first.
 */
static int mime_mapping_cmp_str(const void *a, const void *b)
{
    return strcmp(
        (const char *)a,
        ((const struct mime_mapping *)b)->extension
    );
}

static const char *uri_content_type(const char *uri)
{
    size_t period, urilen = strlen(uri);

    for (period=urilen-1;
        period > 0 &&
        uri[period] != '.' &&
        (urilen-period-1) <= longest_ext;
        period--)
            ;

    if (uri[period] == '.')
    {
        struct mime_mapping *result =
            bsearch((uri+period+1), mime_map, mime_map_size,
            sizeof(struct mime_mapping), mime_mapping_cmp_str);

        if (result != NULL)
        {
            assert(strcmp(uri+period+1, result->extension) == 0);
            return result->mimetype;
        }
    }
    /* else no period found in the string */
    return default_mimetype;
}



/* ---------------------------------------------------------------------------
 * Initialize the sockin global.  This is the socket that we accept
 * connections from.
 */
static void init_sockin(void)
{
    struct sockaddr_in addrin;
    int sockopt;

    /* create incoming socket */
    sockin = socket(PF_INET, SOCK_STREAM, 0);
    if (sockin == -1) err(1, "socket()");

    /* reuse address */
    sockopt = 1;
    if (setsockopt(sockin, SOL_SOCKET, SO_REUSEADDR,
            &sockopt, sizeof(sockopt)) == -1)
        err(1, "setsockopt(SO_REUSEADDR)");

    /* bind socket */
    addrin.sin_family = (u_char)PF_INET;
    addrin.sin_port = htons(bindport);
    addrin.sin_addr.s_addr = bindaddr;
    memset(&(addrin.sin_zero), 0, 8);
    if (bind(sockin, (struct sockaddr *)&addrin,
            sizeof(struct sockaddr)) == -1)
        err(1, "bind(port %u)", bindport);

    printf("listening on %s:%u\n", inet_ntoa(addrin.sin_addr), bindport);

    /* listen on socket */
    if (listen(sockin, max_connections) == -1)
        err(1, "listen()");

    acceptfilter_socket(sockin);
}



/* ---------------------------------------------------------------------------
 * Prints a usage statement.
 */
static void usage(void)
{
    printf("\n  usage: darkhttpd /path/to/wwwroot [options]\n\n"
    "options:\n\n"
    "\t--port number (default: %u)\n" /* bindport */
    "\t\tSpecifies which port to listen on for connections.\n"
    "\n"
    "\t--addr ip (default: all)\n"
    "\t\tIf multiple interfaces are present, specifies\n"
    "\t\twhich one to bind the listening port to.\n"
    "\n"
    "\t--maxconn number (default: system maximum)\n"
    "\t\tSpecifies how many concurrent connections to accept.\n"
    "\n"
    "\t--log filename (default: no logging)\n"
    "\t\tSpecifies which file to append the request log to.\n"
    "\n"
    "\t--chroot (default: don't chroot)\n"
    "\t\tLocks server into wwwroot directory for added security.\n"
    "\n"
    "\t--index filename (default: %s)\n" /* index_name */
    "\t\tDefault file to serve when a directory is requested.\n"
    "\n"
    "\t--mimetypes filename (optional)\n"
    "\t\tParses specified file for extension-MIME associations.\n"
    "\n"
    "\t--uid uid, --gid gid\n"
    "\t\tDrops privileges to given uid:gid after initialization.\n"
#ifdef __FreeBSD__
    "\n"
    "\t--accf\n"
    "\t\tUse acceptfilter.\n"
#endif
    "\n",
    bindport, index_name);
}



/* ---------------------------------------------------------------------------
 * Returns 1 if string is a number, 0 otherwise.  Set num to NULL if
 * disinterested in value.
 */
static int str_to_num(const char *str, int *num)
{
    char *endptr;
    long l = strtol(str, &endptr, 10);
    if (*endptr != '\0') return 0;

    if (num != NULL) *num = (int)l;
    return 1;
}



/* ---------------------------------------------------------------------------
 * Parses commandline options.
 */
static void parse_commandline(const int argc, char *argv[])
{
    int i;

    if ((argc < 2) || (argc == 2 && strcmp(argv[1], "--help") == 0))
    {
        usage(); /* no wwwroot given */
        exit(EXIT_FAILURE);
    }
    if (strcmp(argv[1], "--version") == 0)
    {
        printf("%s "
#ifdef NDEBUG
        "NDEBUG"
#else
        "!NDEBUG"
#endif
        "\n", rcsid);
        exit(EXIT_FAILURE);
    }

    wwwroot = argv[1];
    /* Strip ending slash. */
    if (wwwroot[strlen(wwwroot)-1] == '/') wwwroot[strlen(wwwroot)-1] = '\0';

    /* walk through the remainder of the arguments (if any) */
    for (i=2; i<argc; i++)
    {
        if (strcmp(argv[i], "--port") == 0)
        {
            if (++i >= argc) errx(1, "missing number after --port");
            bindport = (unsigned short)atoi(argv[i]);
        }
        else if (strcmp(argv[i], "--addr") == 0)
        {
            if (++i >= argc) errx(1, "missing ip after --addr");
            bindaddr = inet_addr(argv[i]);
            if (bindaddr == (in_addr_t)INADDR_NONE)
                errx(1, "malformed --addr argument");
        }
        else if (strcmp(argv[i], "--maxconn") == 0)
        {
            if (++i >= argc) errx(1, "missing number after --maxconn");
            max_connections = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "--log") == 0)
        {
            if (++i >= argc) errx(1, "missing filename after --log");
            logfile_name = argv[i];
        }
        else if (strcmp(argv[i], "--chroot") == 0)
        {
            want_chroot = 1;
        }
        else if (strcmp(argv[i], "--index") == 0)
        {
            if (++i >= argc) errx(1, "missing filename after --index");
            index_name = argv[i];
        }
        else if (strcmp(argv[i], "--mimetypes") == 0)
        {
            if (++i >= argc) errx(1, "missing filename after --mimetypes");
            parse_extension_map_file(argv[i]);
        }
        else if (strcmp(argv[i], "--uid") == 0)
        {
            struct passwd *p;
            int num;
            if (++i >= argc) errx(1, "missing uid after --uid");
            p = getpwnam(argv[i]);
            if ((p == NULL) && (str_to_num(argv[i], &num)))
                p = getpwuid( (uid_t)num );

            if (p == NULL) errx(1, "no such uid: `%s'", argv[i]);
            drop_uid = p->pw_uid;
        }
        else if (strcmp(argv[i], "--gid") == 0)
        {
            struct group *g;
            int num;
            if (++i >= argc) errx(1, "missing gid after --gid");
            g = getgrnam(argv[i]);
            if ((g == NULL) && (str_to_num(argv[i], &num)))
                g = getgrgid( (gid_t)num );

            if (g == NULL) errx(1, "no such gid: `%s'", argv[i]);
            drop_gid = g->gr_gid;
        }
        else if (strcmp(argv[i], "--accf") == 0)
        {
            want_accf = 1;
        }
        else
            errx(1, "unknown argument `%s'", argv[i]);
    }
}



/* ---------------------------------------------------------------------------
 * Allocate and initialize an empty connection.
 */
static struct connection *new_connection(void)
{
    struct connection *conn = xmalloc(sizeof(struct connection));

    conn->socket = -1;
    conn->client = INADDR_ANY;
    conn->last_active = time(NULL);
    conn->request = NULL;
    conn->request_length = 0;
    conn->method = NULL;
    conn->uri = NULL;
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



/* ---------------------------------------------------------------------------
 * Accept a connection from sockin and add it to the connection queue.
 */
static void accept_connection(void)
{
    struct sockaddr_in addrin;
    socklen_t sin_size;
    struct connection *conn;

    /* allocate and initialise struct connection */
    conn = new_connection();

    sin_size = (socklen_t)sizeof(struct sockaddr);
    conn->socket = accept(sockin, (struct sockaddr *)&addrin,
            &sin_size);
    if (conn->socket == -1) err(1, "accept()");

    nonblock_socket(conn->socket);

    conn->state = RECV_REQUEST;
    conn->client = addrin.sin_addr.s_addr;
    LIST_INSERT_HEAD(&connlist, conn, entries);

    debugf("accepted connection from %s:%u\n",
        inet_ntoa(addrin.sin_addr),
        ntohs(addrin.sin_port) );
}



static void log_connection(const struct connection *conn);

/* ---------------------------------------------------------------------------
 * Log a connection, then cleanly deallocate its internals.
 */
static void free_connection(struct connection *conn)
{
    debugf("free_connection(%d)\n", conn->socket);
    log_connection(conn);
    if (conn->socket != -1) xclose(conn->socket);
    if (conn->request != NULL) safefree(conn->request);
    if (conn->method != NULL) safefree(conn->method);
    if (conn->uri != NULL) safefree(conn->uri);
    if (conn->referer != NULL) safefree(conn->referer);
    if (conn->user_agent != NULL) safefree(conn->user_agent);
    if (conn->header != NULL && !conn->header_dont_free)
        safefree(conn->header);
    if (conn->reply != NULL && !conn->reply_dont_free) safefree(conn->reply);
    if (conn->reply_fd != -1) xclose(conn->reply_fd);
}



/* ---------------------------------------------------------------------------
 * Recycle a finished connection for HTTP/1.1 Keep-Alive.
 */
static void recycle_connection(struct connection *conn)
{
    int socket_tmp = conn->socket;
    debugf("recycle_connection(%d)\n", socket_tmp);
    conn->socket = -1; /* so free_connection() doesn't close it */
    free_connection(conn);
    conn->socket = socket_tmp;

    /* don't reset conn->client */
    conn->request = NULL;
    conn->request_length = 0;
    conn->method = NULL;
    conn->uri = NULL;
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



/* ---------------------------------------------------------------------------
 * Uppercasify all characters in a string of given length.
 */
static void strntoupper(char *str, const size_t length)
{
    size_t i;
    for (i=0; i<length; i++)
        str[i] = toupper(str[i]);
}



/* ---------------------------------------------------------------------------
 * If a connection has been idle for more than idletime seconds, it will be
 * marked as DONE and killed off in httpd_poll()
 */
static void poll_check_timeout(struct connection *conn)
{
    if (idletime > 0) /* optimised away by compiler */
    {
        if (time(NULL) - conn->last_active >= idletime)
        {
            debugf("poll_check_timeout(%d) caused closure\n", conn->socket);
            conn->conn_close = 1;
            conn->state = DONE;
        }
    }
}



/* ---------------------------------------------------------------------------
 * Format [when] as an RFC1123 date, stored in the specified buffer.  The same
 * buffer is returned for convenience.
 */
#define DATE_LEN 30 /* strlen("Fri, 28 Feb 2003 00:02:08 GMT")+1 */
static char *rfc1123_date(char *dest, const time_t when)
{
    time_t now = when;
    if (strftime(dest, DATE_LEN,
        "%a, %d %b %Y %H:%M:%S %Z", gmtime(&now) ) == 0)
            errx(1, "strftime() failed [%s]", dest);
    return dest;
}



/* ---------------------------------------------------------------------------
 * Decode URL by converting %XX (where XX are hexadecimal digits) to the
 * character it represents.  Don't forget to free the return value.
 */
static char *urldecode(const char *url)
{
    size_t i, len = strlen(url);
    char *out = xmalloc(len+1);
    int pos;

    for (i=0, pos=0; i<len; i++)
    {
        if (url[i] == '%' && i+2 < len &&
            isxdigit(url[i+1]) && isxdigit(url[i+2]))
        {
            /* decode %XX */
            #define HEX_TO_DIGIT(hex) ( \
                ((hex) >= 'A' && (hex) <= 'F') ? ((hex)-'A'+10): \
                ((hex) >= 'a' && (hex) <= 'f') ? ((hex)-'a'+10): \
                ((hex)-'0') )

            out[pos++] = HEX_TO_DIGIT(url[i+1]) * 16 +
                         HEX_TO_DIGIT(url[i+2]);
            i += 2;

            #undef HEX_TO_DIGIT
        }
        else
        {
            /* straight copy */
            out[pos++] = url[i];
        }
    }
    out[pos] = 0;

    out = xrealloc(out, strlen(out)+1);  /* dealloc what we don't need */
    return out;
}



/* ---------------------------------------------------------------------------
 * A default reply for any (erroneous) occasion.
 */
static void default_reply(struct connection *conn,
    const int errcode, const char *errname, const char *format, ...)
{
    char *reason, date[DATE_LEN];
    va_list va;

    va_start(va, format);
    xvasprintf(&reason, format, va);
    va_end(va);

    /* Only really need to calculate the date once. */
    (void)rfc1123_date(date, time(NULL));

    conn->reply_length = xasprintf(&(conn->reply),
     "<html><head><title>%d %s</title></head><body>\n"
     "<h1>%s</h1>\n" /* errname */
     "%s\n" /* reason */
     "<hr>\n"
     "Generated by %s on %s\n"
     "</body></html>\n",
     errcode, errname, errname, reason, pkgname, date);
    safefree(reason);

    conn->header_length = xasprintf(&(conn->header),
     "HTTP/1.1 %d %s\r\n"
     "Date: %s\r\n"
     "Server: %s\r\n"
     "%s" /* keep-alive */
     "Content-Length: %d\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     errcode, errname, date, pkgname, keep_alive(conn),
     conn->reply_length);

    conn->reply_type = REPLY_GENERATED;
    conn->http_code = errcode;
}



/* ---------------------------------------------------------------------------
 * Redirection.
 */
static void redirect(struct connection *conn, const char *format, ...)
{
    char *where, date[DATE_LEN];
    va_list va;

    va_start(va, format);
    xvasprintf(&where, format, va);
    va_end(va);

    /* Only really need to calculate the date once. */
    (void)rfc1123_date(date, time(NULL));

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
     "Location: %s\r\n"
     "%s" /* keep-alive */
     "Content-Length: %d\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     date, pkgname, where, keep_alive(conn), conn->reply_length);

    safefree(where);
    conn->reply_type = REPLY_GENERATED;
    conn->http_code = 301;
}



/* ---------------------------------------------------------------------------
 * Parses a single HTTP request field.  Returns string from end of [field] to
 * first \r, \n or end of request string.  Returns NULL if [field] can't be
 * matched.
 *
 * You need to remember to deallocate the result.
 * example: parse_field(conn, "Referer: ");
 */
static char *parse_field(const struct connection *conn, const char *field)
{
    size_t bound1, bound2;
    char *pos;

    /* find start */
    pos = strstr(conn->request, field);
    if (pos == NULL) return NULL;
    bound1 = pos - conn->request + strlen(field);

    /* find end */
    for (bound2 = bound1;
        conn->request[bound2] != '\r' &&
        bound2 < conn->request_length; bound2++)
            ;

    /* copy to buffer */
    return split_string(conn->request, bound1, bound2);
}



/* ---------------------------------------------------------------------------
 * Parse a Range: field into range_begin and range_end.  Only handles the
 * first range if a list is given.  Sets range_{begin,end}_given to 1 if
 * either part of the range is given.
 */
static void parse_range_field(struct connection *conn)
{
    size_t bound1, bound2, len;
    char *range;

    range = parse_field(conn, "Range: bytes=");
    if (range == NULL) return;
    len = strlen(range);

    do /* break handling */
    {
        /* parse number up to hyphen */
        bound1 = 0;
        for (bound2=0;
            isdigit( (int)range[bound2] ) && bound2 < len;
            bound2++)
                ;

        if (bound2 == len || range[bound2] != '-')
            break; /* there must be a hyphen here */

        if (bound1 != bound2)
        {
            conn->range_begin_given = 1;
            conn->range_begin = (size_t)strtol(range+bound1, NULL, 10);

        }

        /* parse number after hyphen */
        bound2++;
        for (bound1=bound2;
            isdigit( (int)range[bound2] ) && bound2 < len;
            bound2++)
                ;

        if (bound2 != len && range[bound2] != ',')
            break; /* must be end of string or a list to be valid */

        if (bound1 != bound2)
        {
            conn->range_end_given = 1;
            conn->range_end = (size_t)strtol(range+bound1, NULL, 10);
        }
    }
    while(0); /* break handling */
    safefree(range);

    /* sanity check: begin <= end */
    if (conn->range_begin_given && conn->range_end_given &&
        (conn->range_begin > conn->range_end))
    {
        conn->range_begin_given = conn->range_end_given = 0;
    }
}



/* ---------------------------------------------------------------------------
 * Parse an HTTP request like "GET / HTTP/1.1" to get the method (GET), the
 * url (/), the referer (if given) and the user-agent (if given).  Remember to
 * deallocate all these buffers.  The method will be returned in uppercase.
 */
static int parse_request(struct connection *conn)
{
    size_t bound1, bound2;
    char *tmp;
    assert(conn->request_length == strlen(conn->request));

    /* parse method */
    for (bound1 = 0; bound1 < conn->request_length &&
        conn->request[bound1] != ' '; bound1++)
            ;

    conn->method = split_string(conn->request, 0, bound1);
    strntoupper(conn->method, bound1);

    /* parse uri */
    for (; bound1 < conn->request_length &&
        conn->request[bound1] == ' '; bound1++)
            ;

    if (bound1 == conn->request_length) return 0; /* fail */

    for (bound2=bound1+1; bound2 < conn->request_length &&
        conn->request[bound2] != ' ' &&
        conn->request[bound2] != '\r'; bound2++)
            ;

    conn->uri = split_string(conn->request, bound1, bound2);

    /* parse protocol to determine conn_close */
    if (conn->request[bound2] == ' ')
    {
        char *proto;
        for (bound1 = bound2; bound1 < conn->request_length &&
            conn->request[bound1] == ' '; bound1++)
                ;

        for (bound2=bound1+1; bound2 < conn->request_length &&
            conn->request[bound2] != ' ' &&
            conn->request[bound2] != '\r'; bound2++)
                ;

        proto = split_string(conn->request, bound1, bound2);
        if (strcasecmp(proto, "HTTP/1.1") == 0) conn->conn_close = 0;
        safefree(proto);
    }

    /* parse connection field */
    tmp = parse_field(conn, "Connection: ");
    if (tmp != NULL)
    {
        if (strcasecmp(tmp, "close") == 0) conn->conn_close = 1;
        else if (strcasecmp(tmp, "keep-alive") == 0) conn->conn_close = 0;
        safefree(tmp);
    }

    /* parse important fields */
    conn->referer = parse_field(conn, "Referer: ");
    conn->user_agent = parse_field(conn, "User-Agent: ");
    parse_range_field(conn);
    return 1;
}



/* ---------------------------------------------------------------------------
 * Check if a file exists.
 */
static int file_exists(const char *path)
{
    struct stat filestat;
    if ((stat(path, &filestat) == -1) && (errno = ENOENT))
        return 0;
    else
        return 1;
}



/* ---------------------------------------------------------------------------
 * Make sorted list of files in a directory.  Returns number of entries, or -1
 * if error occurs.
 */
struct dlent
{
    char *name;
    int is_dir;
    off_t size;
};

static int dlent_cmp(const void *a, const void *b)
{
    return strcmp( (*((const struct dlent * const *)a))->name,
                   (*((const struct dlent * const *)b))->name );
}

static ssize_t make_sorted_dirlist(const char *path, struct dlent ***output)
{
    DIR *dir;
    struct dirent *ent;
    size_t entries = 0, pool = 0;
    #define POOL_INCR 100
    char *currname;
    struct dlent **list = NULL;

    dir = opendir(path);
    if (dir == NULL) return -1;

    currname = xmalloc(strlen(path) + MAXNAMLEN + 1);

    /* construct list */
    while ((ent = readdir(dir)) != NULL)
    {
        struct stat s;

        if (ent->d_name[0] == '.' && ent->d_name[1] == '\0')
            continue; /* skip "." */
        assert(strlen(ent->d_name) <= MAXNAMLEN);
        sprintf(currname, "%s%s", path, ent->d_name);
        if (stat(currname, &s) == -1)
            continue; /* skip un-stat-able files */

        if (entries == pool)
        {
            pool += POOL_INCR;
            list = xrealloc(list, sizeof(struct dlent*) * pool);
        }

        list[entries] = xmalloc(sizeof(struct dlent));
        list[entries]->name = xstrdup(ent->d_name);
        list[entries]->is_dir = S_ISDIR(s.st_mode);
        list[entries]->size = s.st_size;
        entries++;
    }

    (void)closedir(dir); /* can't error out if opendir() succeeded */

    safefree(currname);
    qsort(list, entries, sizeof(struct dlent*), dlent_cmp);
    *output = xrealloc(list, sizeof(struct dlent*) * entries);
    return entries;
    #undef POOL_INCR
}



/* ---------------------------------------------------------------------------
 * Cleanly deallocate a sorted list of directory files.
 */
static void cleanup_sorted_dirlist(struct dlent **list, const ssize_t size)
{
    ssize_t i;
    for (i=0; i<size; i++)
    {
        safefree(list[i]->name);
        safefree(list[i]);
    }
}



/* ---------------------------------------------------------------------------
 * Generate directory listing.
 */
static void generate_dir_listing(struct connection *conn, const char *path)
{
    char date[DATE_LEN];
    struct dlent **list;
    ssize_t listsize;
    size_t maxlen = 0;
    int i;
    struct apbuf *listing = make_apbuf();

    listsize = make_sorted_dirlist(path, &list);
    if (listsize == -1)
    {
        default_reply(conn, 500, "Internal Server Error",
            "Couldn't list directory: %s", strerror(errno));
        return;
    }

    for (i=0; i<listsize; i++)
    {
        size_t tmp = strlen(list[i]->name);
        if (maxlen < tmp) maxlen = tmp;
    }

    appendf(listing, 
     "<html><head><title>%s</title></head><body>\n"
     "<h1>%s</h1>\n"
     "<tt><pre>\n",
     conn->uri, conn->uri);

    for (i=0; i<listsize; i++)
    {
        appendf(listing, "<a href=\"%s\">%s</a>",
            list[i]->name, list[i]->name);

        if (list[i]->is_dir)
            append(listing, "/\n");
        else
        {
            int j;
            for (j=strlen(list[i]->name); j<maxlen; j++)
                append(listing, " ");

            appendf(listing, "%10d\n", list[i]->size);
        }
    }

    cleanup_sorted_dirlist(list, listsize);
    safefree(list);

    (void)rfc1123_date(date, time(NULL));
    appendf(listing,
     "</pre></tt>\n"
     "<hr>\n"
     "Generated by %s on %s\n"
     "</body></html>\n",
     pkgname, date);

    conn->reply = listing->str;
    conn->reply_length = listing->length;
    safefree(listing); /* don't free inside of listing */

    conn->header_length = xasprintf(&(conn->header),
     "HTTP/1.1 200 OK\r\n"
     "Date: %s\r\n"
     "Server: %s\r\n"
     "%s" /* keep-alive */
     "Content-Length: %d\r\n"
     "Content-Type: text/html\r\n"
     "\r\n",
     date, pkgname, keep_alive(conn), conn->reply_length);

    conn->reply_type = REPLY_GENERATED;
    conn->http_code = 200;
}



/* ---------------------------------------------------------------------------
 * Process a GET/HEAD request
 */
static void process_get(struct connection *conn)
{
    char *decoded_url, *safe_url, *target, *if_mod_since;
    char date[DATE_LEN], lastmod[DATE_LEN];
    const char *mimetype = NULL;
    struct stat filestat;

    /* work out path of file being requested */
    decoded_url = urldecode(conn->uri);

    /* make sure it's safe */
    safe_url = make_safe_uri(decoded_url);
    safefree(decoded_url);
    if (safe_url == NULL)
    {
        default_reply(conn, 400, "Bad Request",
            "You requested an invalid URI: %s", conn->uri);
        return;
    }

    /* does it end in a slash? serve up url/index_name */
    if (safe_url[strlen(safe_url)-1] == '/')
    {
        xasprintf(&target, "%s%s%s", wwwroot, safe_url, index_name);
        if (!file_exists(target))
        {
            safefree(target);
            xasprintf(&target, "%s%s", wwwroot, safe_url);
            generate_dir_listing(conn, target);
            safefree(target);
            safefree(safe_url);
            return;
        }
        mimetype = uri_content_type(index_name);
    }
    else /* points to a file */
    {
        xasprintf(&target, "%s%s", wwwroot, safe_url);
        mimetype = uri_content_type(safe_url);
    }
    safefree(safe_url);
    debugf("uri=%s, target=%s, content-type=%s\n",
        conn->uri, target, mimetype);

    /* open file */
    conn->reply_fd = open(target, O_RDONLY | O_NONBLOCK);
    safefree(target);

    if (conn->reply_fd == -1)
    {
        /* open() failed */
        if (errno == EACCES)
            default_reply(conn, 403, "Forbidden",
                "You don't have permission to access (%s).", conn->uri);
        else if (errno == ENOENT)
            default_reply(conn, 404, "Not Found",
                "The URI you requested (%s) was not found.", conn->uri);
        else
            default_reply(conn, 500, "Internal Server Error",
                "The URI you requested (%s) cannot be returned: %s.",
                conn->uri, strerror(errno));

        return;
    }

    /* stat the file */
    if (fstat(conn->reply_fd, &filestat) == -1)
    {
        default_reply(conn, 500, "Internal Server Error",
            "fstat() failed: %s.", strerror(errno));
        return;
    }

    /* make sure it's a regular file */
    if (S_ISDIR(filestat.st_mode))
    {
        redirect(conn, "%s/", conn->uri);
        return;
    }
    else if (!S_ISREG(filestat.st_mode))
    {
        default_reply(conn, 403, "Forbidden", "Not a regular file.");
        return;
    }

    conn->reply_type = REPLY_FROMFILE;
    (void) rfc1123_date(lastmod, filestat.st_mtime);

    /* check for If-Modified-Since, may not have to send */
    if_mod_since = parse_field(conn, "If-Modified-Since: ");
    if (if_mod_since != NULL &&
        strcmp(if_mod_since, lastmod) == 0)
    {
        debugf("not modified since %s\n", if_mod_since);
        default_reply(conn, 304, "Not Modified", "");
        conn->header_only = 1;
        safefree(if_mod_since);
        return;
    }
    safefree(if_mod_since);

    if (conn->range_begin_given || conn->range_end_given)
    {
        size_t from, to;

        if (conn->range_begin_given && conn->range_end_given)
        {
            /* 100-200 */
            from = conn->range_begin;
            to = conn->range_end;

            /* clamp [to] to filestat.st_size-1 */
            if (to > (filestat.st_size-1)) to = filestat.st_size-1;
        }
        else if (conn->range_begin_given && !conn->range_end_given)
        {
            /* 100- :: yields 100 to end */
            from = conn->range_begin;
            to = filestat.st_size-1;
        }
        else if (!conn->range_begin_given && conn->range_end_given)
        {
            /* -200 :: yields last 200 */
            to = filestat.st_size-1;
            from = to - conn->range_end + 1;

            /* check for wrapping */
            if (from < 0 || from > to) from = 0;
        }
        else errx(1, "internal error - from/to mismatch");

        conn->reply_start = from;
        conn->reply_length = to - from + 1;

        conn->header_length = xasprintf(&(conn->header),
            "HTTP/1.1 206 Partial Content\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "%s" /* keep-alive */
            "Content-Length: %d\r\n"
            "Content-Range: bytes %d-%d/%d\r\n"
            "Content-Type: %s\r\n"
            "Last-Modified: %s\r\n"
            "\r\n"
            ,
            rfc1123_date(date, time(NULL)), pkgname, keep_alive(conn),
            conn->reply_length, from, to, filestat.st_size,
            mimetype, lastmod
        );
        conn->http_code = 206;
        debugf("sending %u-%u/%u\n", (unsigned int)from, (unsigned int)to, 
            (unsigned int)filestat.st_size);
    }
    else /* no range stuff */
    {
        conn->reply_length = filestat.st_size;

        conn->header_length = xasprintf(&(conn->header),
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "%s" /* keep-alive */
            "Content-Length: %d\r\n"
            "Content-Type: %s\r\n"
            "Last-Modified: %s\r\n"
            "\r\n"
            ,
            rfc1123_date(date, time(NULL)), pkgname, keep_alive(conn),
            conn->reply_length, mimetype, lastmod
        );
        conn->http_code = 200;
    }
}



/* ---------------------------------------------------------------------------
 * Process a request: build the header and reply, advance state.
 */
static void process_request(struct connection *conn)
{
    if (!parse_request(conn))
    {
        default_reply(conn, 400, "Bad Request",
            "You sent a request that the server couldn't understand.");
    }
    else if (strcmp(conn->method, "GET") == 0)
    {
        process_get(conn);
    }
    else if (strcmp(conn->method, "HEAD") == 0)
    {
        process_get(conn);
        conn->header_only = 1;
    }
    else if (strcmp(conn->method, "OPTIONS") == 0 ||
             strcmp(conn->method, "POST") == 0 ||
             strcmp(conn->method, "PUT") == 0 ||
             strcmp(conn->method, "DELETE") == 0 ||
             strcmp(conn->method, "TRACE") == 0 ||
             strcmp(conn->method, "CONNECT") == 0)
    {
        default_reply(conn, 501, "Not Implemented",
            "The method you specified (%s) is not implemented.",
            conn->method);
    }
    else
    {
        default_reply(conn, 400, "Bad Request",
            "%s is not a valid HTTP/1.1 method.", conn->method);
    }

    /* advance state */
    conn->state = SEND_HEADER;

    /* request not needed anymore */
    free(conn->request);
    conn->request = NULL; /* important: don't free it again later */
}



/* ---------------------------------------------------------------------------
 * Receiving request.
 */
static void poll_recv_request(struct connection *conn)
{
    #define BUFSIZE 65536
    char buf[BUFSIZE];
    ssize_t recvd;

    recvd = recv(conn->socket, buf, BUFSIZE, 0);
    debugf("poll_recv_request(%d) got %d bytes\n", conn->socket, (int)recvd);
    if (recvd <= 0)
    {
        if (recvd == -1)
            debugf("recv(%d) error: %s\n", conn->socket, strerror(errno));
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    conn->last_active = time(NULL);
    #undef BUFSIZE

    /* append to conn->request */
    conn->request = xrealloc(conn->request, conn->request_length+recvd+1);
    memcpy(conn->request+conn->request_length, buf, (size_t)recvd);
    conn->request_length += recvd;
    conn->request[conn->request_length] = 0;

    /* process request if we have all of it */
    if (conn->request_length > 4 &&
        memcmp(conn->request+conn->request_length-4, "\r\n\r\n", 4) == 0)
        process_request(conn);

    /* die if it's too long */
    if (conn->request_length > MAX_REQUEST_LENGTH)
    {
        default_reply(conn, 413, "Request Entity Too Large",
            "Your request was dropped because it was too long.");
        conn->state = SEND_HEADER;
    }
}



/* ---------------------------------------------------------------------------
 * Sending header.  Assumes conn->header is not NULL.
 */
static void poll_send_header(struct connection *conn)
{
    ssize_t sent;

    assert(conn->header_length == strlen(conn->header));

    sent = send(conn->socket, conn->header + conn->header_sent,
        conn->header_length - conn->header_sent, 0);
    conn->last_active = time(NULL);
    debugf("poll_send_header(%d) sent %d bytes\n", conn->socket, (int)sent);

    /* handle any errors (-1) or closure (0) in send() */
    if (sent < 1)
    {
        if (sent == -1)
            debugf("send(%d) error: %s\n", conn->socket, strerror(errno));
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    conn->header_sent += (unsigned int)sent;
    conn->total_sent += (unsigned int)sent;

    /* check if we're done sending */
    if (conn->header_sent == conn->header_length)
    {
        if (conn->header_only)
            conn->state = DONE;
        else
            conn->state = SEND_REPLY;
    }
}



/* ---------------------------------------------------------------------------
 * Send chunk on socket <s> from FILE *fp, starting at <ofs> and of size
 * <size>.  Use sendfile() is possible since it's zero-copy on some platforms.
 * Returns the number of bytes sent, 0 on closure, -1 if send() failed, -2 if
 * read error.
 */
static ssize_t send_from_file(const int s, const int fd,
    off_t ofs, const size_t size)
{
#ifdef __FreeBSD__
    off_t sent;
    if (sendfile(fd, s, ofs, size, NULL, &sent, 0) == -1)
    {
        if (errno == EAGAIN)
            return sent;
        else
            return -1;
    }
    else return size;
#else
#ifdef __linux
    return sendfile(s, fd, &ofs, size);
#else
    #define BUFSIZE 20000
    char buf[BUFSIZE];
    size_t amount = min((size_t)BUFSIZE, size);
    ssize_t numread;
    #undef BUFSIZE

    if (lseek(fd, ofs, SEEK_SET) == -1) err(1, "fseek(%d)", (int)ofs);
    numread = read(fd, buf, amount);
    if (numread != amount)
    {
        if (numread == 0)
            fprintf(stderr, "premature eof on fd %d\n", fd);
        else if (numread != -1)
            fprintf(stderr, "read %d bytes, expecting %u bytes on fd %d\n",
                numread, amount, fd);
        return -1;
    }
    return send(s, buf, amount, 0);
#endif
#endif
}



/* ---------------------------------------------------------------------------
 * Sending reply.
 */
static void poll_send_reply(struct connection *conn)
{
    ssize_t sent;

    assert( (conn->reply_type == REPLY_GENERATED && 
        conn->reply_length == strlen(conn->reply)) ||
        conn->reply_type == REPLY_FROMFILE);

    if (conn->reply_type == REPLY_GENERATED)
    {
        sent = send(conn->socket,
            conn->reply + conn->reply_start + conn->reply_sent,
            conn->reply_length - conn->reply_sent, 0);
    }
    else
    {
        sent = send_from_file(conn->socket, conn->reply_fd,
            (off_t)(conn->reply_start + conn->reply_sent),
            conn->reply_length - conn->reply_sent);
    }
    conn->last_active = time(NULL);
    debugf("poll_send_reply(%d) sent %d: %d+[%d-%d] of %d\n",
        conn->socket, (int)sent, (int)conn->reply_start,
        (int)conn->reply_sent,
        (int)(conn->reply_sent + sent - 1),
        (int)conn->reply_length);

    /* handle any errors (-1) or closure (0) in send() */
    if (sent < 1)
    {
        if (sent == -1)
            debugf("send(%d) error: %s\n", conn->socket, strerror(errno));
        else if (sent == 0)
            debugf("send(%d) closure\n", conn->socket);
        conn->conn_close = 1;
        conn->state = DONE;
        return;
    }
    conn->reply_sent += (unsigned int)sent;
    conn->total_sent += (unsigned int)sent;

    /* check if we're done sending */
    if (conn->reply_sent == conn->reply_length) conn->state = DONE;
}



/* ---------------------------------------------------------------------------
 * Add a connection's details to the logfile.
 */
static void log_connection(const struct connection *conn)
{
    struct in_addr inaddr;

    if (logfile == NULL) return;
    if (conn->http_code == 0) return; /* invalid - died in request */

    /* Separated by tabs:
     * time client_ip method uri http_code bytes_sent "referer" "user-agent"
     */

    inaddr.s_addr = conn->client;

    fprintf(logfile, "%lu\t%s\t%s\t%s\t%d\t%u\t\"%s\"\t\"%s\"\n",
        time(NULL), inet_ntoa(inaddr), conn->method, conn->uri,
        conn->http_code, conn->total_sent,
        (conn->referer == NULL)?"":conn->referer,
        (conn->user_agent == NULL)?"":conn->user_agent
        );
    fflush(logfile);
}



/* ---------------------------------------------------------------------------
 * Main loop of the httpd - a select() and then delegation to accept
 * connections, handle receiving of requests, and sending of replies.
 */
static void httpd_poll(void)
{
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

    LIST_FOREACH_SAFE(conn, &connlist, entries, next)
    {
        poll_check_timeout(conn);
        switch (conn->state)
        {
        case RECV_REQUEST:
        recv_request:
            MAX_FD_SET(conn->socket, &recv_set);
            bother_with_timeout = 1;
            break;

        case SEND_HEADER:
        case SEND_REPLY:
            MAX_FD_SET(conn->socket, &send_set);
            bother_with_timeout = 1;
            break;

        case DONE:
            /* clean out stale connections while we're at it */
            if (conn->conn_close)
            {
                LIST_REMOVE(conn, entries);
                free_connection(conn);
                safefree(conn);
            }
            else
            {
                recycle_connection(conn);
                /* And enqueue as RECV_REQUEST. */
                goto recv_request;
            }
            break;

        default: errx(1, "invalid state");
        }
    }
    #undef MAX_FD_SET

    /* -select- */
    select_ret = select(max_fd + 1, &recv_set, &send_set, NULL,
        (bother_with_timeout) ? &timeout : NULL);
    if (select_ret == 0)
    {
        if (!bother_with_timeout)
            errx(1, "select() timed out");
        else
            return;
    }
    if (select_ret == -1) err(1, "select()");

    /* poll connections that select() says need attention */
    if (FD_ISSET(sockin, &recv_set)) accept_connection();

    LIST_FOREACH(conn, &connlist, entries)
    switch (conn->state)
    {
    case RECV_REQUEST:
        if (FD_ISSET(conn->socket, &recv_set)) poll_recv_request(conn);
        break;

    case SEND_HEADER:
        if (FD_ISSET(conn->socket, &send_set)) poll_send_header(conn);
        break;

    case SEND_REPLY:
        if (FD_ISSET(conn->socket, &send_set)) poll_send_reply(conn);
        break;

    default: errx(1, "invalid state");
    }
}



/* ---------------------------------------------------------------------------
 * Close all sockets and FILEs and exit.
 */
static void exit_quickly(int sig)
{
    struct connection *conn, *next;
    size_t i;

    printf("\ncaught %s, cleaning up...", strsignal(sig)); fflush(stdout);
    /* close and free connections */
    LIST_FOREACH_SAFE(conn, &connlist, entries, next)
    {
        LIST_REMOVE(conn, entries);
        free_connection(conn);
        free(conn);
    }
    xclose(sockin);
    if (logfile != NULL) fclose(logfile);

    /* free mime_map */
    for (i=0; i<mime_map_size; i++)
    {
        free(mime_map[i].extension);
        free(mime_map[i].mimetype);
    }
    free(mime_map);
    free(keep_alive_field); 
    printf("done!\n");
    exit(EXIT_SUCCESS);
}



/* ---------------------------------------------------------------------------
 * Execution starts here.
 */
int main(int argc, char *argv[])
{
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
    if (logfile_name != NULL)
    {
        logfile = fopen(logfile_name, "ab");
        if (logfile == NULL) err(1, "fopen(\"%s\")", logfile_name);
    }

    /* signals */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        err(1, "signal(ignore SIGPIPE)");
    if (signal(SIGINT, exit_quickly) == SIG_ERR)
        err(1, "signal(SIGINT)");
    if (signal(SIGQUIT, exit_quickly) == SIG_ERR)
        err(1, "signal(SIGQUIT)");

    /* security */
    if (want_chroot)
    {
        if (chroot(wwwroot) == -1) err(1, "chroot(\"%s\")", wwwroot);
        printf("chrooted to `%s'\n", wwwroot);
        wwwroot[0] = '\0'; /* empty string */
    }
    if (drop_gid != INVALID_GID)
    {
        if (setgid(drop_gid) == -1) err(1, "setgid(%d)", drop_gid);
        printf("set gid to %d\n", drop_gid);
    }
    if (drop_uid != INVALID_UID)
    {
        if (setuid(drop_uid) == -1) err(1, "setuid(%d)", drop_uid);
        printf("set uid to %d\n", drop_uid);
    }

    for (;;) httpd_poll();

    return EXIT_FAILURE; /* unreachable */
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
