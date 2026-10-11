/*
 * The SSH agent client for WinSCP's PuTTY on Linux, in place of its
 * windows/agent-client.c.
 *
 * The Windows unit talks to Pageant through a named pipe as a HANDLE, with
 * ReadFile/WriteFile for a synchronous query and the handle-io layer for an
 * asynchronous one. The agent on POSIX is the Unix-domain socket named by
 * $SSH_AUTH_SOCK, and a connected socket is a descriptor -- which is what a
 * HANDLE is in the POSIX layer -- so the same machinery serves: this is
 * windows/agent-client.c's named-pipe path with the connection made to that
 * socket instead. There is no Pageant window to fall back to.
 *
 * The functions and their behaviour are WinSCP's (its callback_set argument
 * included); the code is new, after that unit and PuTTY's unix/agent-client.c.
 */
#include "putty.h"
#include "pageant.h" /* for AGENT_MAX_MSGLEN */
#include "puttyexp.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* WinSCP's choice between Pageant and OpenSSH's agent, which core stores
 * and restores. Both are $SSH_AUTH_SOCK here -- OpenSSH's agent and PuTTY's
 * Unix Pageant alike listen there -- so it is kept but changes nothing. The
 * default is Windows'. */
AuthAgentImplementation auth_agent_implementation = AAI_PAGEANT;

/* The agent's socket path, or NULL if there is none to try. */
static const char *agent_socket_path(void)
{
    const char *path = getenv("SSH_AUTH_SOCK");
    return (path && *path) ? path : NULL;
}

/* A connected, blocking, close-on-exec socket as a HANDLE, or
 * INVALID_HANDLE_VALUE. */
static HANDLE connect_to_agent(void)
{
    const char *path = agent_socket_path();
    struct sockaddr_un addr;
    int fd;

    if (!path || strlen(path) >= sizeof(addr.sun_path))
        return INVALID_HANDLE_VALUE;
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return INVALID_HANDLE_VALUE;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)(intptr_t)fd;
}

Socket *agent_connect(Plug *plug)
{
    HANDLE h = connect_to_agent();
    if (h == INVALID_HANDLE_VALUE)
        return new_error_socket_fmt(
            plug, "unable to connect to SSH agent at '%s'",
            agent_socket_path() ? agent_socket_path() : "(SSH_AUTH_SOCK unset)");
    return make_handle_socket(h, h, NULL, NULL, 0, plug, false);
}

bool agent_exists(void)
{
    HANDLE h = connect_to_agent();
    if (h == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(h);
    return true;
}

struct agent_pending_query {
    struct handle *handle;
    HANDLE os_handle;
    strbuf *response;
    void (*callback)(void *, void *, int);
    void *callback_ctx;
};

/* As the Windows unit: the reply is a 32-bit big-endian length and that
 * many bytes. Returns the whole message's length once it has all arrived,
 * 0 while more is due, -1 for a malformed length. */
static int agent_accumulate_response(strbuf *sb, const void *data, size_t len)
{
    put_data(sb, data, len);
    if (sb->len >= 4) {
        uint32_t length_field = GET_32BIT_MSB_FIRST(sb->u);
        if (length_field > AGENT_MAX_MSGLEN)
            return -1;
        int overall_length = length_field + 4;
        if (sb->len >= overall_length)
            return overall_length;
    }
    return 0;
}

static size_t agent_gotdata(struct handle *h, const void *data, size_t len, int err)
{
    agent_pending_query *pq = handle_get_privdata(h);

    if (err || len == 0) {
        pq->callback(pq->callback_ctx, NULL, 0);
        agent_cancel_query(pq);
        return 0;
    }

    int status = agent_accumulate_response(pq->response, data, len);
    if (status == -1) {
        pq->callback(pq->callback_ctx, NULL, 0);
        agent_cancel_query(pq);
    } else if (status > 0) {
        void *response_buf = strbuf_to_str(pq->response);
        pq->response = NULL;
        pq->callback(pq->callback_ctx, response_buf, status);
        agent_cancel_query(pq);
    }
    return 0;
}

void agent_cancel_query(agent_pending_query *pq)
{
    handle_free(pq->handle);
    CloseHandle(pq->os_handle);
    if (pq->response)
        strbuf_free(pq->response);
    sfree(pq);
}

agent_pending_query *agent_query(
    strbuf *query, void **out, int *outlen,
    void (*callback)(void *, void *, int), void *callback_ctx,
    struct callback_set *callback_set)
{
    agent_pending_query *pq = NULL;
    strbuf *sb = NULL;
    HANDLE h = connect_to_agent();

    if (h == INVALID_HANDLE_VALUE)
        goto failure;

    strbuf_finalise_agent_query(query);

    for (DWORD done = 0; done < query->len;) {
        DWORD nwritten;
        if (!WriteFile(h, query->s + done, query->len - done, &nwritten, NULL))
            goto failure;
        done += nwritten;
    }

    if (!callback) {
        int status;

        sb = strbuf_new_nm();
        do {
            char buf[1024];
            DWORD nread;
            if (!ReadFile(h, buf, sizeof(buf), &nread, NULL))
                goto failure;
            status = agent_accumulate_response(sb, buf, nread);
        } while (status == 0);

        if (status == -1)
            goto failure;

        *out = strbuf_to_str(sb);
        *outlen = status;
        sb = NULL;
        pq = NULL;
        goto out;
    }

    pq = snew(agent_pending_query);
    pq->handle = handle_input_new(callback_set, h, agent_gotdata, pq, 0);
    pq->os_handle = h;
    h = INVALID_HANDLE_VALUE;  /* now owned by the query */
    pq->response = strbuf_new_nm();
    pq->callback = callback;
    pq->callback_ctx = callback_ctx;
    goto out;

  failure:
    *out = NULL;
    *outlen = 0;
    pq = NULL;

  out:
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (sb)
        strbuf_free(sb);
    return pq;
}
