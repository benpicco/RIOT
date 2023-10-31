/*
 * Copyright (C) 2020 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <m.lenders@fu-berlin.de>
 */

#include "net/ipv6/hdr.h"
#include "net/ipv6/ext.h"
#include "net/gnrc/ipv6/ext/opt.h"
#include "net/gnrc/pktbuf.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define OPTION_ADDER_AVAILABLE \
    IS_USED(MODULE_GNRC_IPV6_EXT_OPT_RPL) || \
    0U

gnrc_pktsnip_t *gnrc_rpl_opt_add(const gnrc_pktsnip_t *ipv6,
                                gnrc_pktsnip_t *opt,
                                uint8_t protnum)
{
    (void)ipv6;
    (void)protnum;

    ipv6_ext_t *ext = opt->data;
    ext->nh = PROTNUM_RESERVED;
    ext->len = 0;

    uint8_t *type = (uint8_t *)opt->data + sizeof(ipv6_ext_t);
    uint8_t *len  = type + 1;
    ipv6_ext_opt_rpl_t *rpl_opt = (void *)(len + 1);

    *type = IPV6_EXT_OPT_RPL;
    *len = sizeof(*rpl_opt);
    memset(rpl_opt, 0, sizeof(*rpl_opt));

    return opt;
}

static gnrc_pktsnip_t *_opt_add(const gnrc_pktsnip_t *ipv6,
                                gnrc_pktsnip_t *opt,
                                uint8_t protnum)
{
    (void)protnum;
    (void)ipv6;
#if IS_USED(MODULE_GNRC_IPV6_EXT_OPT_RPL)
    opt = gnrc_rpl_opt_add(ipv6, opt, protnum);
#endif
    return opt;
}

gnrc_pktsnip_t *gnrc_ipv6_ext_opt_add_hopopt(gnrc_pktsnip_t *pkt)
{
    if (OPTION_ADDER_AVAILABLE) {
        gnrc_pktsnip_t *ipv6 = pkt, *hopopt;
        ipv6_hdr_t *ipv6_hdr = ipv6->data;

        assert(ipv6->users == 1);
        if (ipv6_hdr->nh == PROTNUM_IPV6_EXT_HOPOPT) {
            hopopt = ipv6->next;
            /* TODO: mark hop-by-hop header? */
            assert(hopopt->users == 1);
            LL_DELETE(ipv6, hopopt);
        }
        else {
            hopopt = gnrc_pktbuf_add(ipv6->next, NULL, IPV6_EXT_LEN_UNIT,
                                     GNRC_NETTYPE_IPV6_EXT);
            if (hopopt == NULL) {
                DEBUG("gnrc_ipv6_ext_opt: unable to allocate hop-by-hop header\n");
                gnrc_pktbuf_release(pkt);
                return NULL;
            }
            ipv6_ext_t *hopopt_hdr = hopopt->data;
            hopopt_hdr->nh = ipv6_hdr->nh;
            ipv6_hdr->nh = PROTNUM_IPV6_EXT_HOPOPT;
        }
        hopopt = _opt_add(ipv6, hopopt, PROTNUM_IPV6_EXT_HOPOPT);
        hopopt->next = ipv6->next;
        ipv6->next = hopopt;
    }
    return pkt;

}

/** @} */
