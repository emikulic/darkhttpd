/* +----------------------------------------------------------------------- *\
   | */ static const char pkgname[]   = "darkhttpd/0.1";                    /*
   | */ static const char copyright[] = "copyright (c) 2003 Emil Mikulic";  /*
   +----------------------------------------------------------------------- */

/*
 * $Id$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* for easy defusal */
#define debugf printf

LIST_HEAD(conn_list_head, connection) connlist =
    LIST_HEAD_INITIALIZER(conn_list_head);

struct connection
{
    LIST_ENTRY(connection) entries;

    int socket;
    enum {
        RECV_REQUEST,   /* receiving request */
        SEND_HEADER,    /* sending generated header */
        SEND_REPLY,     /* sending reply */
        DONE            /* connection closed, need to remove from queue */
        } state;

    char *request;
    unsigned int request_length;

    char *header;
    unsigned int header_sent, header_length;

    enum { REPLY_GENERATED, REPLY_FROMFILE } reply_type;
    char *reply;
    FILE *reply_file;
    unsigned int reply_sent, reply_length;
};



/* defaults can be overridden on the command-line */
static in_addr_t bindaddr = INADDR_ANY;
static u_int16_t bindport = 80;
static int max_connections = -1; /* kern.ipc.somaxconn */

static int sockin;  /* socket to accept connections from */
/*@null@*/ 
static char *wwwroot = NULL;    /* a path name */
/*@null@*/ 
static char *logfile_name = NULL;   /* NULL = no logging */
static int want_chroot = 0;



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

    debugf("listening on %s:%u\n", inet_ntoa(addrin.sin_addr), bindport);

    /* listen on socket */
    if (listen(sockin, max_connections) == -1)
        err(1, "listen()");
}



/* ---------------------------------------------------------------------------
 * Prints a usage statement.
 */
static void usage(void)
{
    printf("\n  usage: darkhttpd /path/to/wwwroot [options]\n\n"
    "options:\n"
    "\t--port number (default: %u)\n" /* DEFAULT_PORT */
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
    "\t\tSpecifies which file to log requests to.\n"
    "\n"
    "\t--chroot (default: don't chroot)\n"
    "\t\tLocks server into wwwroot directory for added security.\n"
    "\n"
    /* "\t--uid blah, --gid blah\n" FIXME */
    , bindport);
    exit(EXIT_FAILURE);
}



/* ---------------------------------------------------------------------------
 * Parses commandline options.
 */
static void parse_commandline(const int argc, char *argv[])
{
    int i;

    if (argc < 2) usage(); /* no wwwroot given */
    wwwroot = argv[1];

    /* walk through the remainder of the arguments (if any) */
    for (i=2; i<argc; i++)
    {
        if (strcmp(argv[i], "--port") == 0)
        {
            if (++i >= argc) errx(1, "missing number after --port");
            bindport = (u_int16_t)atoi(argv[i]);
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
        else
            errx(1, "unknown argument `%s'", argv[i]);
    }
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
 * Accept a connection from sockin and add it to the connection queue.
 */
static void accept_connection(void)
{
    struct sockaddr_in addrin;
    socklen_t sin_size;
    struct connection *conn;

    /* allocate and initialise struct connection */
    conn = (struct connection *)xmalloc(sizeof(struct connection));
    conn->socket = -1;
    conn->request = NULL;
    conn->request_length = 0;
    conn->header = NULL;
    conn->header_sent = conn->header_length = 0;
    conn->reply = NULL;
    conn->reply_file = NULL;

    sin_size = (socklen_t)sizeof(struct sockaddr);
    conn->socket = accept(sockin, (struct sockaddr *)&addrin,
            &sin_size);
    if (conn->socket == -1) err(1, "accept()");

    conn->state = RECV_REQUEST;
    LIST_INSERT_HEAD(&connlist, conn, entries);

    debugf("accepted connection from %s:%u\n",
        inet_ntoa(addrin.sin_addr),
        ntohs(addrin.sin_port) );
}



/* ---------------------------------------------------------------------------
 * Cleanly deallocate the internals of a struct connection
 */
static void free_connection(struct connection *conn)
{
    if (conn->socket != -1) close(conn->socket);
    if (conn->request != NULL) free(conn->request);
    if (conn->header != NULL) free(conn->header);
    if (conn->reply != NULL) free(conn->reply);
    if (conn->reply_file != NULL) fclose(conn->reply_file);
}


/* ---------------------------------------------------------------------------
 * realloc that errx()s if it can't allocate.
 */
static void *xrealloc(void *original, const size_t size)
{
    void *ptr = realloc(original, size);
    if (ptr == NULL) errx(1, "can't reallocate %u bytes", size);
    return ptr;
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
    if (recvd == -1) err(1, "recv()");
    if (recvd == 0)
    {
        /* socket closed on us */
        conn->state = DONE;
        return;
    }

    /* append to conn->request */
    conn->request = xrealloc(conn->request, conn->request_length + recvd);
    memcpy(conn->request+conn->request_length, buf, recvd);
    conn->request_length += recvd;

    #if 1
    { int i;
    printf("recvd %d: ", recvd);
    for (i=0; i<recvd; i++) printf("%02x ",
    conn->request[conn->request_length-recvd+i]);
    printf("\n");
    }
    #endif

    #undef BUFSIZE
}



static void poll_send_header(struct connection *conn)
{}

static void poll_send_reply(struct connection *conn)
{}



/* ---------------------------------------------------------------------------
 * Main loop of the httpd - a select() and then delegation to accept
 * connections, handle receiving of requests and sending of replies.
 */
static void httpd_poll(void)
{
    fd_set recv_set, send_set;
    int max_fd, select_ret;
    struct connection *conn;

    FD_ZERO(&recv_set);
    FD_ZERO(&send_set);
    max_fd = 0;

    /* set recv/send fd_sets */
    #define MAX_FD_SET(sock, fdset) FD_SET(sock,fdset), \
                                    max_fd = (max_fd<sock) ? sock : max_fd

    MAX_FD_SET(sockin, &recv_set);

    LIST_FOREACH(conn, &connlist, entries)
    switch (conn->state)
    {
    case RECV_REQUEST:
        MAX_FD_SET(conn->socket, &recv_set);
        break;

    case SEND_HEADER:
    case SEND_REPLY:
        MAX_FD_SET(conn->socket, &send_set);
        break;

    case DONE:
        /* clean out stale connections while we're at it */
        LIST_REMOVE(conn, entries);
        free_connection(conn);
        free(conn);
        break;

    default: errx(1, "invalid state");
    }
    #undef MAX_FD_SET

    debugf("select("), fflush(stdout);
    select_ret = select(max_fd + 1, &recv_set, &send_set, NULL, NULL);
    if (select_ret == 0) errx(1, "select() timed out");
    if (select_ret == -1) err(1, "select()");
    debugf(")\n");

    /* poll connections that select() says need attention */
    if (FD_ISSET(sockin, &recv_set)) accept_connection();

    LIST_FOREACH(conn, &connlist, entries)
    switch (conn->state)
    {
    case RECV_REQUEST:
        if (FD_ISSET(conn->socket, &recv_set))
            poll_recv_request(conn);
        break;

    case SEND_HEADER:
        if (FD_ISSET(conn->socket, &send_set))
            poll_send_header(conn);
        break;

    case SEND_REPLY:
        if (FD_ISSET(conn->socket, &send_set))
            poll_send_reply(conn);
        break;

    default: errx(1, "invalid state");
    }
}



int main(int argc, char *argv[])
{
    printf("%s, %s.\n", pkgname, copyright);
    parse_commandline(argc, argv);
    init_sockin();

    for (;;) httpd_poll();

    (void) close(sockin); /* unreachable =/ fix later */
    return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
