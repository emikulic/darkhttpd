/* +----------------------------------------------------------------------- *\
   | */ static const char pkgname[]   = "darkhttpd/0.1";                    /*
   | */ static const char copyright[] = "copyright (c) 2003 Emil Mikulic";  /*
   +----------------------------------------------------------------------- */

/*
 * $Id$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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

    printf("listening on %s:%u\n", inet_ntoa(addrin.sin_addr), bindport);

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



int main(int argc, char *argv[])
{
    printf("%s, %s.\n", pkgname, copyright);
    parse_commandline(argc, argv);
    init_sockin();
    (void) close(sockin);
    return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
