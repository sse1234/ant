/*
 * ant-server — phase 1: remote command executor.
 *
 * Protocol (one TCP connection, many commands):
 *   server:  "ANT/1 ready\n"
 *   client:  <command line>\n
 *   server:  "ANT1 rc=<n> len=<n>\n" followed by exactly len output bytes
 *   ... repeat; client closes the connection when done.
 *
 * Commands run synchronously via System() under the user shell (so shell
 * internals like Echo work), stdin = NIL:, stdout/stderr captured to a
 * T: temp file that is then streamed back. No output streaming during
 * execution — that's phase 2's console handler. Needs T: assigned
 * (standard on a normal boot; the dev rig's startup-sequence does it).
 *
 * Compiles with bebbo m68k-amigaos-gcc (crosstools image).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>

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
#define CMDMAX 1024
#define CHUNK 1024
#define CMD_STACK 16384 /* default 4k stacks are a classic source of flaky */

struct Library *SocketBase;

static char cmdbuf[CMDMAX];
static char iobuf[CHUNK];
static char tmp_t[40];        /* T:... — normal boots */
static char tmp_ram[40];      /* RAM:... — bare-Kickstart rig has no T: */
static const char *tmpname;

static LONG send_all(LONG s, const char *p, LONG len)
{
    while (len > 0) {
        LONG n = send(s, (char *)p, len, 0);
        if (n <= 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

/* Read one \n-terminated line, strip \r, NUL-terminate. -1 on EOF/error. */
static LONG recv_line(LONG s, char *dst, LONG max)
{
    LONG len = 0;
    char c;

    for (;;) {
        LONG n = recv(s, &c, 1, 0);
        if (n <= 0)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r' && len < max - 1)
            dst[len++] = c;
    }
    dst[len] = '\0';
    return len;
}

/* Run cmd, send header + captured output. Connection errors → -1. */
static LONG run_command(LONG cli, const char *cmd)
{
    BPTR in = 0, out = 0;
    LONG rc = -1, size = 0;
    char hdr[48];

    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    tmpname = tmp_t;
    out = Open((STRPTR)tmpname, MODE_NEWFILE);
    if (!out) {
        tmpname = tmp_ram;
        out = Open((STRPTR)tmpname, MODE_NEWFILE);
    }

    if (in && out) {
        rc = SystemTags((STRPTR)cmd,
                        SYS_Input, in,
                        SYS_Output, out,
                        SYS_Asynch, FALSE,
                        NP_StackSize, CMD_STACK,
                        TAG_END);
    }
    /* Synchronous System() leaves the handles to us. */
    if (in)
        Close(in);
    if (out)
        Close(out);
    out = 0;

    if (rc != -1) {
        out = Open((STRPTR)tmpname, MODE_OLDFILE);
        if (out) {
            Seek(out, 0, OFFSET_END);
            size = Seek(out, 0, OFFSET_BEGINING);
            if (size < 0)
                size = 0;
        }
    }

    sprintf(hdr, "ANT1 rc=%ld len=%ld\n", (long)rc, (long)size);
    if (send_all(cli, hdr, strlen(hdr)) < 0)
        goto drop;

    while (size > 0) {
        LONG n = Read(out, iobuf, size < CHUNK ? size : CHUNK);
        if (n <= 0)
            break;
        if (send_all(cli, iobuf, n) < 0)
            goto drop;
        size -= n;
    }

    if (out)
        Close(out);
    DeleteFile((STRPTR)tmpname);
    return 0;

drop:
    if (out)
        Close(out);
    DeleteFile((STRPTR)tmpname);
    return -1;
}

int main(int argc, char **argv)
{
    LONG port = (argc > 1) ? atol(argv[1]) : DEFAULT_PORT;
    LONG srv = -1, cli = -1;
    struct sockaddr_in sa;
    socklen_t salen;
    LONG on = 1;
    int rc = RETURN_FAIL;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR oldwin = me->pr_WindowPtr;

    /* A daemon must never raise "Please insert volume..." requesters —
     * they block forever with nobody at the screen. -1 = auto-fail. */
    me->pr_WindowPtr = (APTR)-1;

    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) {
        fprintf(stderr,
                "ant-server: no bsdsocket.library v4 — TCP/IP stack not running?\n");
        return RETURN_FAIL;
    }

    /* Blocking socket calls return EINTR on Ctrl-C instead of hanging. */
    SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), SIGBREAKF_CTRL_C, TAG_END);

    sprintf(tmp_t, "T:ant-%08lx.out", (unsigned long)FindTask(NULL));
    sprintf(tmp_ram, "RAM:ant-%08lx.out", (unsigned long)FindTask(NULL));

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

    printf("ant-server/1: listening on :%ld (Ctrl-C to stop)\n", (long)port);

    for (;;) {
        salen = sizeof(sa);
        cli = accept(srv, (struct sockaddr *)&sa, &salen);
        if (cli < 0)
            break; /* EINTR via break mask, or stack went away */

        printf("ant-server: connection from %s\n", Inet_NtoA(sa.sin_addr.s_addr));
        if (send_all(cli, "ANT/1 ready\n", 12) == 0) {
            while (recv_line(cli, cmdbuf, CMDMAX) >= 0) {
                if (run_command(cli, cmdbuf) < 0)
                    break;
            }
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
    me->pr_WindowPtr = oldwin;
    return rc;
}
