/*
 * Copyright (C) 2021 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifndef GNRC_NETIF_NUM_MAX
#define GNRC_NETIF_NUM_MAX  (2)
#endif

#include "net/gnrc/netif.h"
#ifdef MODULE_SOCK_DNS
#include "net/sock/dns.h"
#endif

/* get the next netif, returns true if there are more */
static bool _netif_get(gnrc_netif_t **current_netif)
{
    gnrc_netif_t *netif = *current_netif;
    netif = gnrc_netif_iter(netif);

    *current_netif = netif;
    return !gnrc_netif_highlander() && gnrc_netif_iter(netif);
}

int gnrc_netif_parse_hostname(const char *hostname, ipv6_addr_t *addr,
                              gnrc_netif_t **netif)
{
    *netif = NULL;

#ifdef MODULE_SOCK_DNS
    /* hostname is not an IPv6 address */
    if (strchr(hostname, ':') == NULL) {
        int res = sock_dns_query(hostname, addr, AF_INET6);
        if (res < 0) {
            return res;
        }
        return 0;
    }
#endif

    /* search for interface ID */
    size_t len = strlen(hostname);
    char *iface = strchr(hostname, '%');
    if (iface) {
        *netif = gnrc_netif_get_by_pid(atoi(iface + 1));
        len -= strlen(iface);
    }
    /* preliminary select the first interface */
    else if (_netif_get(netif)) {
        /* don't take it if there is more than one interface */
        *netif = NULL;
    }

    if (ipv6_addr_from_buf(addr, hostname, len) == NULL) {
         return -EINVAL;
    }

    return 0;
}

#if IS_USED(MODULE_GNRC_NETIF_BUS)
static void _subscribe(gnrc_netif_t *netif, msg_bus_entry_t *entry)
{
    msg_bus_t *bus = gnrc_netif_get_bus(netif, GNRC_NETIF_BUS_IPV6);

    msg_bus_attach(bus, entry);
    msg_bus_subscribe(entry, GNRC_IPV6_EVENT_ADDR_VALID);
}

static void _unsubscribe(gnrc_netif_t *netif, msg_bus_entry_t *entry)
{
    msg_bus_t *bus = gnrc_netif_get_bus(netif, GNRC_NETIF_BUS_IPV6);
    msg_bus_detach(bus, entry);
}

int gnrc_netif_wait_for_prefix(gnrc_netif_t *netif, unsigned timeout_us)
{
    msg_bus_entry_t subs[GNRC_NETIF_NUM_MAX];
    int res = 0;
    msg_t m;

    /* subscribe to all interfaces */
    if (netif) {
        _subscribe(netif, &subs[0]);
    } else {
        unsigned count = 0;
        while ((netif = gnrc_netif_iter(netif)) && count < ARRAY_SIZE(subs)) {
            _subscribe(netif, &subs[count++]);
        }
        netif = NULL;
    }

    /* wait for prefix */
    do {
        if (xtimer_msg_receive_timeout(&m, timeout_us) < 0) {
            res = -ETIMEDOUT;
            goto out;
        }
    } while (ipv6_addr_is_link_local(m.content.ptr));

out:
    /* unsubscribe from interface bus */
    if (netif) {
        _unsubscribe(netif, &subs[0]);
    } else {
        unsigned count = 0;
        while ((netif = gnrc_netif_iter(netif)) && count < ARRAY_SIZE(subs)) {
            _unsubscribe(netif, &subs[count++]);
        }
    }

    return res;
}
#endif /* IS_USED(MODULE_GNRC_NETIF_BUS) */
/** @} */
