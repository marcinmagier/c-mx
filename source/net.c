
#include "mx/net.h"
#include "mx/memory.h"
#include "mx/string.h"
#include "mx/log.h"
#include "mx/misc.h"

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>





#define INET_LINK_LOCAL_ADDR    0xA9FE0000L
#define INET_LINK_LOCAL_MASK    0xFFFF0000L
#define INET6_LINK_LOCAL_ADDR   0xFE80
#define INET6_LINK_LOCAL_MASK   0xFFC0




int net_addr_copy(struct sockaddr *to, struct sockaddr *from)
{
    size_t length = 0;

    if (from->sa_family == AF_INET) {
        length = sizeof(struct sockaddr_in);
    }
    else if (from->sa_family == AF_INET6) {
        length = sizeof(struct sockaddr_in6);
    }

    if (length > 0) {
        memcpy(to, from, length);
        return 0;
    }

    return -1;
}


int net_addr_get_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in *)sa)->sin_port);
    }
    else if (sa->sa_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
    }

    return -1;
}


/**
 * Check if given address is loopback
 *
 */
bool net_addr_is_loopback(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in*)sa;
        return sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ? true : false;
    }
    else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin = (struct sockaddr_in6*)sa;
        return memcmp(&sin->sin6_addr, &in6addr_loopback, sizeof(struct in6_addr)) == 0 ? true : false;
    }

    return false;
}


/**
 * Check if given address is link local
 *
 */
bool net_addr_is_local_link(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in*)sa;
        uint32_t netaddr = ntohl(sin->sin_addr.s_addr) & INET_LINK_LOCAL_MASK;
        return (netaddr == INET_LINK_LOCAL_ADDR) ? true : false;
    }
    else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin = (struct sockaddr_in6*)sa;
        uint16_t netaddr = (((uint16_t)sin->sin6_addr.s6_addr[0] << 8) | sin->sin6_addr.s6_addr[1]) & INET6_LINK_LOCAL_MASK;
        return (netaddr == INET6_LINK_LOCAL_ADDR) ? true : false;
    }

    return false;
}


/**
 * Convert given address object to string
 *
 */
bool net_addr_to_str(struct sockaddr *sa, char *buffer, size_t buffer_len)
{
    void *addr_ptr = NULL;

    if (sa->sa_family == AF_INET) {
        addr_ptr = &((struct sockaddr_in *)sa)->sin_addr;
    }
    else if (sa->sa_family == AF_INET6) {
        addr_ptr = &((struct sockaddr_in6 *)sa)->sin6_addr;
    }

    if (addr_ptr) {
        if (inet_ntop(sa->sa_family, addr_ptr, buffer, buffer_len))
            return true;
    }

    return false;
}


/**
 *  Configure address object from given string
 *
 * The 'buffer' should be long enought to store IPv6 string i.e. at least INET6_ADDRSTRLEN
 *
 */
bool net_addr_from_str(struct sockaddr *sa, const char *buffer)
{
    void *addr_ptr = NULL;

    if (strchr(buffer, ':')) {
        sa->sa_family = AF_INET6;
        addr_ptr = &((struct sockaddr_in6 *)sa)->sin6_addr;
    }
    else {
        sa->sa_family = AF_INET;
        addr_ptr = &((struct sockaddr_in *)sa)->sin_addr;
    }

    return inet_pton(sa->sa_family, buffer, addr_ptr) == 1 ? true : false;
}


/**
 * Check if addresses match
 *
 */
bool net_addr_match(struct sockaddr *sa1, struct sockaddr *sa2, struct sockaddr *netmask)
{
    if ((sa1->sa_family == sa2->sa_family) && (sa2->sa_family == netmask->sa_family)) {
        if (sa1->sa_family == AF_INET) {
            struct in_addr *addr1 = &((struct sockaddr_in *)sa1)->sin_addr;
            struct in_addr *addr2 = &((struct sockaddr_in *)sa2)->sin_addr;
            struct in_addr *mask = &((struct sockaddr_in *)netmask)->sin_addr;
            if ((addr1->s_addr & mask->s_addr) == (addr2->s_addr & mask->s_addr))
                return true;
        }
        else if (sa1->sa_family == AF_INET6) {
            struct in6_addr *addr1 = &((struct sockaddr_in6*)sa1)->sin6_addr;
            struct in6_addr *addr2 = &((struct sockaddr_in6*)sa2)->sin6_addr;
            struct in6_addr *mask = &((struct sockaddr_in6*)netmask)->sin6_addr;

            for (int i=0; i<16; i++) {
                if ((addr1->s6_addr[i] & mask->s6_addr[i]) != (addr2->s6_addr[i] & mask->s6_addr[i]))
                    return false;
            }
            return true;
        }
    }

    return false;
}


/**
 * Find interface matching destination address
 *
 */
int net_addr_find_matching_iface(struct sockaddr *sa_iface, struct sockaddr *sa_dest)
{
    int ret = -1;

    struct net_iface_iterator *nit = net_iface_iterator_new(sa_dest->sa_family);
    if (nit) {
        ret = 0;
        while(net_iface_iterator_has_next(nit)) {
            struct net_iface *iface = net_iface_iterator_get_next(nit);

            if (!net_iface_is_up(iface) || !net_iface_is_running(iface))
                continue;

            struct sockaddr *sa = net_iface_get_address(iface);
            struct sockaddr *sa_mask = net_iface_get_netmask(iface);
            if (net_addr_match(sa, sa_dest, sa_mask)) {
                net_addr_copy(sa_iface, sa);
                ret = 1;
                break;
            }
        }

        net_iface_iterator_delete(nit);
    }

    return ret;
}






/**
 * Get index of the interface
 *
 */
bool net_if_get_index(const char *ifname, unsigned int *index)
{
    *index = if_nametoindex(ifname);

    return (*index > 0) ? true : false;
}


/**
 * Get address of the interface
 *
 */
bool net_if_get_addr(const char *ifname, struct sockaddr *addr)
{
    bool ret = false;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock > 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, ifname);
        if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
            ERROR("Call ioctl SIOCGIFADDR failed with %d", errno);
        }
        else {
            ret = true;
            memcpy(addr, &ifr.ifr_addr, sizeof(ifr.ifr_addr));
        }
        close(sock);
    }

    return ret;
}







struct net_iface
{
    struct ifaddrs* ifa;
    char buffer[INET6_ADDRSTRLEN+1];
};



struct net_iface_iterator
{
    unsigned short addr_family;
    struct ifaddrs* ifaddrs;

    struct net_iface iface;
};





void net_iface_init(struct net_iface *self)
{
    self->ifa = NULL;
}


void net_iface_clean(struct net_iface *self)
{
    UNUSED(self);
}



bool net_iface_is_up(struct net_iface *self)
{
    return self->ifa->ifa_flags & IFF_UP ? true : false;
}


bool net_iface_is_running(struct net_iface *self)
{
    return self->ifa->ifa_flags & IFF_RUNNING ? true : false;
}


bool net_iface_is_loopback(struct net_iface *self)
{
    return self->ifa->ifa_flags & IFF_LOOPBACK ? true : false;
}


bool net_iface_is_point_to_point(struct net_iface *self)
{
    return self->ifa->ifa_flags & IFF_POINTOPOINT ? true : false;
}


bool net_iface_supports_multicast(struct net_iface *self)
{
    return self->ifa->ifa_flags & IFF_MULTICAST ? true : false;
}




struct sockaddr* net_iface_get_address(struct net_iface *self)
{
    return self->ifa->ifa_addr;
}


struct sockaddr* net_iface_get_netmask(struct net_iface *self)
{
    return self->ifa->ifa_netmask;
}


const char* net_iface_get_name(struct net_iface *self)
{
    return self->ifa->ifa_name;
}


const char* net_iface_get_ip(struct net_iface *self)
{
    if (net_addr_to_str(self->ifa->ifa_addr, self->buffer, sizeof(self->buffer)))
        return self->buffer;

    return NULL;
}


unsigned int net_iface_get_index(struct net_iface *self)
{
    return if_nametoindex(self->ifa->ifa_name);
}


struct rtnl_link_stats* net_iface_get_stats(struct net_iface *self)
{
    if (self->ifa->ifa_addr->sa_family == AF_PACKET)
        return (struct rtnl_link_stats*)self->ifa->ifa_data;

    return NULL;
}





bool net_iface_iterator_init(struct net_iface_iterator *self, unsigned short addr_family)
{
    if (getifaddrs(&self->ifaddrs) < 0) {
        ERROR("Cannot get interface addresses");
        self->ifaddrs = NULL;
        return false;
    }

    net_iface_init(&self->iface);
    self->addr_family = addr_family;

    return true;
}


void net_iface_iterator_clean(struct net_iface_iterator *self)
{
    if (self->ifaddrs)
        freeifaddrs(self->ifaddrs);
}


struct net_iface_iterator* net_iface_iterator_new(unsigned short addr_family)
{
    struct net_iface_iterator *self = xmalloc(sizeof(struct net_iface_iterator));

    if (net_iface_iterator_init(self, addr_family))
        return self;

    return net_iface_iterator_delete(self);
}


struct net_iface_iterator* net_iface_iterator_delete(struct net_iface_iterator *self)
{
    net_iface_iterator_clean(self);
    free(self);
    return NULL;
}


static bool net_iface_iterator_prepare_next(struct net_iface_iterator *self)
{
    struct ifaddrs *ifa = self->iface.ifa;

    if (ifa) {
        self->iface.ifa = ifa->ifa_next;
    }
    else {
        self->iface.ifa = self->ifaddrs;
    }

    ifa = self->iface.ifa;
    if (ifa == NULL)
        return false;

    return true;
}


bool net_iface_iterator_has_next(struct net_iface_iterator *self)
{
    while (net_iface_iterator_prepare_next(self)) {
        if (self->addr_family == AF_UNSPEC)
            return true;    // Requested all interfaces
        struct sockaddr *sa = self->iface.ifa->ifa_addr;
        if (sa && (sa->sa_family == self->addr_family))
            return true;    // Found requested address family
    }

    return false;
}


struct net_iface*  net_iface_iterator_get_next(struct net_iface_iterator *self)
{
    return &self->iface;
}









struct net_ifname
{
    struct if_nameindex *ifni;
};


void net_ifname_init(struct net_ifname *self)
{
    self->ifni = NULL;
}


void net_ifname_clean(struct net_ifname *self)
{
    UNUSED(self);
}



const char* net_ifname_get_name(struct net_ifname *self)
{
    return self->ifni->if_name;
}


unsigned int net_ifname_get_index(struct net_ifname *self)
{
    return self->ifni->if_index;
}





struct net_ifname_iterator
{
    struct if_nameindex *ifnameindex;

    struct net_ifname ifname;
};


struct net_ifname_iterator* net_ifname_iterator_new(void)
{
    struct net_ifname_iterator *self = xmalloc(sizeof(struct net_ifname_iterator));

    self->ifnameindex = if_nameindex();
    if (self->ifnameindex == NULL) {
        ERROR("Cannot get interface names");
        return net_ifname_iterator_delete(self);
    }

    net_ifname_init(&self->ifname);
    return self;
}


struct net_ifname_iterator* net_ifname_iterator_delete(struct net_ifname_iterator *self)
{
    if (self->ifnameindex)
        if_freenameindex(self->ifnameindex);

    return xfree(self);
}


bool net_ifname_iterator_has_next(struct net_ifname_iterator *self)
{
    if (self->ifname.ifni == NULL) {
        self->ifname.ifni = self->ifnameindex;
    }
    else {
        self->ifname.ifni++;
    }

    if (self->ifname.ifni == NULL || self->ifname.ifni->if_index == 0 || self->ifname.ifni->if_name == NULL)
        return false;

    return true;
}


struct net_ifname*  net_ifname_iterator_get_next(struct net_ifname_iterator *self)
{
    return &self->ifname;
}
