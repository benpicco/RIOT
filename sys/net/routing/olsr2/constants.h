#ifndef CONSTANTS_H_
#define CONSTANTS_H_

/* The well-known UDP port for MANET as defined in RFC 5498 */
#define MANET_PORT	269

/* in seconds */
#define OLSR2_HELLO_REFRESH_INTERVAL_SECONDS	2
#define OLSR2_TC_REFRESH_INTERVAL_SECONDS	5
#define OLSR2_HOLD_TIME_SECONDS		(3 * OLSR2_TC_REFRESH_INTERVAL_SECONDS)

#define OLSR2_TC_HOP_LIMIT	16

#define OLSR2_HYST_SCALING	0.4
#define OLSR2_HYST_LOW	0.3
#define OLSR2_HYST_HIGH	0.8

/* in µs */
#define OLSR2_MAX_JITTER_US	1000000

#define OLSR2_FLOODING_MPR_SELECTOR 1
#define OLSR2_ROUTING_MPR_SELECTOR  2

#define RFC5444_TLV_NODE_NAME 42

/* NHDP message TLV array index */
enum {
    IDX_TLV_ITIME,			/* Interval time */
    IDX_TLV_VTIME,			/* validity time */
    IDX_TLV_NODE_NAME,		/* name of the node */
};

/* NHDP address TLV array index */
enum {
    IDX_ADDRTLV_LOCAL_IF,		/* is local if */
    IDX_ADDRTLV_LINK_STATUS,	/* link status TODO */
    IDX_ADDRTLV_MPR,		/* neighbor selected as mpr */
    IDX_ADDRTLV_METRIC,		/* incomming link metric */
    IDX_ADDRTLV_NODE_NAME,		/* 'name' of a node from graph.gv */
};

#endif /* CONSTANTS_H_ */
