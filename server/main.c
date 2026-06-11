/*
 * ant-server — phase 0: TCP echo daemon.
 *
 * Proves toolchain + bsdsocket.library + network path (FS-UAE emulated or
 * Roadshow on hardware) before any terminal logic lands. Listens on the
 * given port (default 6860), echoes lines back, "quit" closes the
 * connection, Ctrl-C at the shell stops the daemon.
 *
 * UNTESTED SKELETON until the cross toolchain is set up — expect to touch
 * the includes (socketbasetags.h location varies between AmiTCP-era and
 * Roadshow-era SDKs).
 */

#include <exec/types.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <proto/bsdsocket.h>
#include <bsdsocket/socketbasetags.h> /* older SDKs: <amitcp/socketbasetags.h> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 6860
#define BUFSIZE 1024

struct Library *SocketBase;

static const char greeting[] =
    "ant-server phase 0 (echo). Lines come back at you; 'quit' disconnects.\r\n";

int main(int argc, char **argv)
{
    LONG port = (argc > 1) ? atol(argv[1]) : DEFAULT_PORT;
    LONG srv = -1, cli = -1;
    struct sockaddr_in sa;
    socklen_t salen;
    char buf[BUFSIZE];
    LONG n, on = 1;
    int rc = RETURN_FAIL;

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) {
        fprintf(stderr,
                "ant-server: no bsdsocket.library v4 — TCP/IP stack not running?\n");
        return RETURN_FAIL;
    }

    /* Blocking socket calls return EINTR on Ctrl-C instead of hanging. */
    SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), SIGBREAKF_CTRL_C, TAG_END);

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        fprintf(stderr, "ant-server: socket() failed, errno %ld\n", (long)Errno());
        goto out;
    }
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((UWORD)port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        fprintf(stderr, "ant-server: bind(:%ld) failed, errno %ld\n",
                (long)port, (long)Errno());
        goto out;
    }
    if (listen(srv, 1) < 0) {
        fprintf(stderr, "ant-server: listen() failed, errno %ld\n", (long)Errno());
        goto out;
    }

    printf("ant-server: listening on :%ld (Ctrl-C to stop)\n", (long)port);

    for (;;) {
        salen = sizeof(sa);
        cli = accept(srv, (struct sockaddr *)&sa, &salen);
        if (cli < 0)
            break; /* EINTR via break mask, or stack went away */

        printf("ant-server: connection from %s\n", Inet_NtoA(sa.sin_addr.s_addr));
        send(cli, greeting, sizeof(greeting) - 1, 0);

        for (;;) {
            n = recv(cli, buf, sizeof(buf), 0);
            if (n <= 0)
                break;
            if (n >= 4 && strncmp(buf, "quit", 4) == 0)
                break;
            if (send(cli, buf, n, 0) != n)
                break;
        }

        CloseSocket(cli);
        cli = -1;
        printf("ant-server: connection closed\n");

        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
            break;
    }

    rc = RETURN_OK;
out:
    if (cli >= 0)
        CloseSocket(cli);
    if (srv >= 0)
        CloseSocket(srv);
    CloseLibrary(SocketBase);
    return rc;
}
