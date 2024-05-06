
#ifndef __MX_NET_H_
#define __MX_NET_H_


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <netinet/in.h>


int net_addr_copy(struct sockaddr *to, struct sockaddr *from);
int net_addr_get_port(struct sockaddr *sa);
bool net_addr_is_loopback(struct sockaddr *sa);
bool net_addr_is_local_link(struct sockaddr *sa);
bool net_addr_to_str(struct sockaddr *sa, char *buffer, size_t buffer_len);
bool net_addr_from_str(struct sockaddr *sa, const char *buffer);

bool net_addr_match(struct sockaddr *sa1, struct sockaddr *sa2, struct sockaddr *mask);

int net_addr_find_matching_iface(struct sockaddr *sa_iface, struct sockaddr *sa_dest);





bool net_if_get_index(const char *ifname, unsigned int *index);
bool net_if_get_addr(const char *ifname, struct sockaddr *addr);





struct net_iface;

bool net_iface_is_up(struct net_iface *self);
bool net_iface_is_running(struct net_iface *self);
bool net_iface_is_loopback(struct net_iface *self);
bool net_iface_is_point_to_point(struct net_iface *self);
bool net_iface_supports_multicast(struct net_iface *self);

struct sockaddr* net_iface_get_address(struct net_iface *self);
struct sockaddr* net_iface_get_netmask(struct net_iface *self);

const char* net_iface_get_name(struct net_iface *self);
const char* net_iface_get_ip(struct net_iface *self);
unsigned int net_iface_get_index(struct net_iface *self);

struct rtnl_link_stats* net_iface_get_stats(struct net_iface *self);



struct net_iface_iterator;

struct net_iface_iterator* net_iface_iterator_new(unsigned short int addr_family);
struct net_iface_iterator* net_iface_iterator_delete(struct net_iface_iterator *self);

bool net_iface_iterator_has_next(struct net_iface_iterator *self);
struct net_iface*  net_iface_iterator_get_next(struct net_iface_iterator *self);





struct net_ifname;

const char* net_ifname_get_name(struct net_ifname *self);
unsigned int net_ifname_get_index(struct net_ifname *self);



struct net_ifname_iterator;

struct net_ifname_iterator* net_ifname_iterator_new(void);
struct net_ifname_iterator* net_ifname_iterator_delete(struct net_ifname_iterator *self);

bool net_ifname_iterator_has_next(struct net_ifname_iterator *self);
struct net_ifname*  net_ifname_iterator_get_next(struct net_ifname_iterator *self);





#endif /* __MX_NET_H_ */
