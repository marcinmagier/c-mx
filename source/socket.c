
#include "mx/socket.h"
#include "mx/net.h"
#include "mx/log.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <netdb.h>


#define SOCKET_INVALID      (-1)





/**
 * Open socket
 *
 */
int socket_open(int family, int type)
{
    int sock = socket(family, type, 0);
    if (sock == SOCKET_INVALID) {
        RESET("Cannot create socket");
    }
    return sock;
}


/**
 * Close socket
 *
 */
int socket_close(int sock)
{
    close(sock);

    return SOCKET_INVALID;
}


int socket_get_addr_family(int sock)
{
    int addr_family = 0;
    socklen_t addr_family_len = sizeof(addr_family);
    int ret = getsockopt(sock, SOL_SOCKET, SO_DOMAIN, &addr_family, &addr_family_len);
    if (ret == -1) {
        ERROR("Cannot get socket address family");
    }
    return addr_family;

}


/**
 * Configure socket regarding reusing address
 *
 */
int socket_set_reuse_addr(int sock, int on)
{
    int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret == -1) {
        ERROR("Cannot configure socket - SO_REUSEADDR");
    }
    return ret;
}


/**
 * Configure socket regarding reusing port
 *
 */
int socket_set_reuse_port(int sock, int on)
{
    int ret = 0;
#ifdef SO_REUSEPORT
    ret =setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    if (ret == -1) {
        ERROR("Cannot configure socket - SO_REUSEPORT");
    }
#else
    UNUSED(sock);
    UNUSED(on);
#endif
    return ret;

}


/**
 * Configure socket regarding Nagle buffering algorithm
 *
 */
int socket_set_tcp_no_delay(int sock, int on)
{
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret == -1) {
        ERROR("Cannot configure socket - TCP_NODELAY");
    }
    return ret;
}


/**
 * Configure socket regarding blocking/non-blocking
 *
 */
int socket_set_non_blocking(int sock, int on)
{
    int ret = fcntl(sock, F_GETFL, 0);
    if (ret != -1) {
        if (on == 1)
            ret = fcntl(sock, F_SETFL, ret | O_NONBLOCK);
        else
            ret = fcntl(sock, F_SETFL, ret & (~O_NONBLOCK));
    }

    if (ret == -1) {
        ERROR("Cannot configure socket - NONBLOCK");
    }
    return ret;
}


/**
 * Configure socket regarding timeouts
 *
 */
int socket_set_timeout(int sock, unsigned int timeout)
{
    int ret;
    struct timeval tv;

    tv.tv_sec = timeout;
    ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret == -1) {
        ERROR("Cannot configure socket - SO_RCVTIMEO");
    }
    return ret;
}


/**
 * Configure socket time to live value
 *
 */
int socket_set_ttl(int sock, unsigned char value)
{
    int ret = setsockopt(sock, IPPROTO_IP, IP_TTL, &value, sizeof(value));
    if (ret == -1) {
        ERROR("Cannot configure socket - IP_TTL");
    }

    return ret;
}


/**
 * Connect inet socket
 *
 */
int socket_connect_inet(int sock_family, const char *addr, unsigned int port)
{
    int sock = -1;

    struct addrinfo hints, *result;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = sock_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    int ret = getaddrinfo(addr, NULL, &hints, &result);
    if (ret != 0) {
        ERROR("Cannot resolve %s, getaddrinfo error %d", addr, ret);
        return -1;
    }

    for (struct addrinfo *res=result; res; res=res->ai_next) {
        struct sockaddr *sockaddr = res->ai_addr;
        socklen_t sockaddr_len = 0;
        if (res->ai_family == AF_INET) {
            sockaddr_len = sizeof(struct sockaddr_in);
            struct sockaddr_in *sa = (struct sockaddr_in*)sockaddr;
            sa->sin_port = htons(port);
        }
        else if (res->ai_family == AF_INET6) {
            sockaddr_len = sizeof(struct sockaddr_in6);
            struct sockaddr_in6 *sa = (struct sockaddr_in6*)sockaddr;
            sa->sin6_port = htons(port);
        }
        else {
            // Unsupported address family
            ret = -1;
            continue;
        }

        sock = socket_open(res->ai_family, SOCK_STREAM);
        ret = connect(sock, sockaddr, sockaddr_len);
        if (ret == 0)
            break;      // Connected
        sock = socket_close(sock);
    }

    freeaddrinfo(result);
    if (ret == -1) {
        ERROR("Connection to %s %d failed", addr, port);
        return -1;
    }

    return sock;
}


/**
 * Connect unix socket
 *
 */
int socket_connect_unix(const char *path)
{
    int ret = 0;
    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(saddr.sun_path)) {
        RESET("Path %s to long", path);
        return -1;
    }
    strcpy(saddr.sun_path, path);

    int sock = socket_open(AF_UNIX, SOCK_STREAM);
    ret = connect(sock, (struct sockaddr*) &saddr, sizeof(saddr));
    if (ret == 0) {
        return sock;    // Connected
    }

    ERROR("Cannot connect to %s", path);
    return socket_close(sock);
}


/**
 * Bind socket
 *
 */
int socket_bind_inet(int sock, struct sockaddr *addr, unsigned short port)
{
    struct sockaddr_storage bind_addr;
    memset(&bind_addr, 0, sizeof(struct sockaddr_storage));
    size_t bind_addr_len = 0;

    int addr_family = socket_get_addr_family(sock);
    if (addr_family == AF_INET) {
        struct sockaddr_in *tmp_addr = (struct sockaddr_in*)&bind_addr;
        bind_addr_len = sizeof(struct sockaddr_in);
        tmp_addr->sin_family = AF_INET;
        if (port)
            tmp_addr->sin_port = htons(port);
        if (addr)
            memcpy(&tmp_addr->sin_addr, &((struct sockaddr_in*)addr)->sin_addr, sizeof(struct in_addr));
    }
    else if (addr_family == AF_INET6) {
        struct sockaddr_in6 *tmp_addr = (struct sockaddr_in6*)&bind_addr;
        bind_addr_len = sizeof(struct sockaddr_in6);
        tmp_addr->sin6_family = AF_INET6;
        if (port)
            tmp_addr->sin6_port = htons(port);
        if (addr)
            memcpy(&tmp_addr->sin6_addr, &((struct sockaddr_in6*)addr)->sin6_addr, sizeof(struct in6_addr));
    }

    int ret = bind(sock, (struct sockaddr*)&bind_addr, bind_addr_len);
    if (ret < 0) {
        ERROR("Cannot bind to port %d", port);
    }

    return ret;
}



/**
 *  Listen common socket
 *
 */
int socket_listen(const char *addr, unsigned int port)
{
    if (port == 0)
        return socket_listen_unix(SOCKET_INVALID, addr);

    return socket_listen_inet(SOCKET_INVALID, addr, port);
}


/**
 * Listen inet socket
 *
 */
int socket_listen_inet(int sock, const char *addr, unsigned int port)
{
    bool create_socket = !socket_is_valid(sock);

    struct addrinfo hints, *result;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = socket_is_valid(sock) ? AF_UNSPEC : socket_get_addr_family(sock);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_PASSIVE;

    int ret = getaddrinfo(addr, NULL, &hints, &result);
    if (ret != 0) {
        ERROR("Cannot resolve %s, getaddrinfo error %d", addr, ret);
        return ret;
    }

    for (struct addrinfo *res=result; res; res=res->ai_next) {
        if (res->ai_family != AF_INET && res->ai_family != AF_INET6) {
            ret = -1;
            continue;   // Unsupported address family
        }

        if (create_socket && !socket_is_valid(sock))
            sock = socket_open(res->ai_family, SOCK_STREAM);
        struct sockaddr *sockaddr = res->ai_addr;
        ret = socket_bind_inet(sock, sockaddr, port);
        if (ret == 0)
            break;      // Binded
        if (create_socket)
            sock = socket_close(sock);
    }

    freeaddrinfo(result);
    if (ret == -1) {
        ERROR("Cannot bind to %s : %d", addr, port);
        return ret;
    }

    ret = listen(sock, 5);
    if (ret == -1) {
        ERROR("Cannot listen on %s : %d", addr, port);
        return ret;
    }

    return sock;    // Listening
}


/**
 * Listen unix socket
 *
 */
int socket_listen_unix(int sock, const char *path)
{
    bool create_socket = !socket_is_valid(sock);

    int ret = 0;
    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(saddr.sun_path)) {
        RESET("Path %s to long", path);
        return -1;
    }
    strcpy(saddr.sun_path, path);

    if (create_socket)
        sock = socket_open(AF_UNIX, SOCK_STREAM);

    do {
        ret = bind(sock, (struct sockaddr*) &saddr, sizeof(saddr));
        if (ret == -1) {
            ERROR("Cannot bind to %s", path);
            break;
        }
        ret = listen(sock, 5);
        if (ret == -1) {
            ERROR("Cannot listen on %s", path);
            break;
        }

        return sock;    // Listening
    } while (0);

    if (create_socket)
        socket_close(sock);

    return -1;
}



/**
 * Send data through socket
 *
 * Usable for blocking mode
 *
 */
int socket_send(int sock, const void *buffer, size_t len)
{
    int ret = 0;
    const char *ptr = buffer;
    unsigned int remain = len;

    while (remain > 0) {
        ret = write(sock, ptr, remain);
        if (ret <= 0)
            return ret;

        remain -= ret;
        ptr += ret;
    }

    return len;
}


/**
 * Receive data from socket
 *
 * Usable for blocking mode
 *
 */
int socket_receive(int sock, void *buffer, size_t len)
{
    int ret = 0;
    char *ptr = buffer;
    unsigned int remain = len;

    while (remain > 0) {
        ret = read(sock, ptr, remain);
        if (ret <= 0)
            return ret;

        remain -= ret;
        ptr += ret;
    }

    return len;
}


/**
 * Send data through multicast socket
 *
 */
int socket_sendto(int sock, struct sockaddr *addr, unsigned short port, void *data, size_t len)
{
    struct sockaddr_storage dest_addr;
    memset(&dest_addr, 0, sizeof(struct sockaddr_storage));
    size_t dest_addr_len = 0;

    int addr_family = socket_get_addr_family(sock);
    if (addr_family == AF_INET) {
        struct sockaddr_in *tmp_addr = (struct sockaddr_in*)&dest_addr;
        dest_addr_len = sizeof(struct sockaddr_in);
        tmp_addr->sin_family = AF_INET;
        tmp_addr->sin_port = htons(port);
        memcpy(&tmp_addr->sin_addr, &((struct sockaddr_in*)addr)->sin_addr, sizeof(struct in_addr));
    }
    else if (addr_family == AF_INET6) {
        struct sockaddr_in6 *tmp_addr = (struct sockaddr_in6*)&dest_addr;
        dest_addr_len = sizeof(struct sockaddr_in6);
        tmp_addr->sin6_family = AF_INET6;
        tmp_addr->sin6_port = htons(port);
        memcpy(&tmp_addr->sin6_addr, &((struct sockaddr_in6*)addr)->sin6_addr, sizeof(struct in6_addr));
    }

    int ret = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, dest_addr_len);
    if (ret < 0) {
        ERROR("Sending to fd %d failed with %d", sock, errno);
    }

    return ret;
}



/**
 * Send message
 *
 *
 */
int socket_send_msg(int sock, const void *buffer, size_t len)
{
    int ret = 0;
    uint32_t netLen = htonl(len);
    ret = socket_send(sock, &netLen, sizeof(netLen));

    if (ret > 0)
        ret = socket_send(sock, buffer, len);

    return ret;
}


/**
 * Receive message length
 *
 */
int socket_receive_msg_len(int sock)
{
    uint32_t netLen = 0;
    int ret = 0;

    ret = socket_receive(sock, (char*)&netLen, sizeof(netLen));
    if (ret > 0)
        ret = ntohl(netLen);

    return ret;
}


/**
 * Receive message data
 *
 */
int socket_receive_msg(int sock, void *buffer, size_t len)
{
    return socket_receive(sock, buffer, len);
}
