
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/ipv6/nib.h"
#include "net/gnrc/ndp.h"
#include "net/gnrc/rpl.h"

#define ENABKLE_DEBUG 1
#include "debug.h"

void gnrc_ipv6_nib_rtr_adv_pio_cb(gnrc_netif_t *upstream, const ndp_opt_pi_t *pio)
{
    gnrc_netif_t *downstream = NULL;
    const ipv6_addr_t *prefix = &pio->prefix;
    uint32_t valid_ltime = byteorder_ntohl(pio->valid_ltime);
    uint32_t pref_ltime = byteorder_ntohl(pio->pref_ltime);
    const uint8_t prefix_len = pio->prefix_len;

    while ((downstream = gnrc_netif_iter(downstream))) {

        if (downstream == upstream) {
            continue;
        }

        /* configure subnet on downstream interface */
        int idx = gnrc_netif_ipv6_add_prefix(downstream, prefix, prefix_len,
                                             valid_ltime, pref_ltime);
        if (idx < 0) {
            DEBUG("auto_subnets: adding prefix to %u failed\n", downstream->pid);
            continue;
        }

        /* start advertising subnet */
        gnrc_ipv6_nib_change_rtr_adv_iface(downstream, true);

        /* configure RPL root if applicable */
        gnrc_rpl_configure_root(downstream, &downstream->ipv6.addrs[idx]);
    }
}
