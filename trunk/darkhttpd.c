/*
 * darkhttpd
 * copyright (c) 2003 Emil Mikulic.
 * $Id$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>



/*
 * Defaults - these can be overridden by various command-line arguments.
 * Run `darkhttpd --help' for help.
 */
#define DEFAULT_PORT        80
#define DEFAULT_BINDADDR    INADDR_ANY
#define DEFAULT_MAXCONN     -1 /* kern.ipc.somaxconn */

/*@unused@*/
static const char server_version[] = "darkhttpd/0.1";



static int sockin;

/*int main(int argc, char *argv[])*/
int main()
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
    addrin.sin_port = htons(DEFAULT_PORT);
    addrin.sin_addr.s_addr = DEFAULT_BINDADDR;
    memset(&(addrin.sin_zero), 0, 8);
    if (bind(sockin, (struct sockaddr *)&addrin,
            sizeof(struct sockaddr)) == -1)
        err(1, "bind()");

    /* listen on socket */
    if (listen(sockin, DEFAULT_MAXCONN) == -1)
        err(1, "listen()");

    /* --- do stuff here --- */

    (void) close(sockin);
    return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
