#include <stdlib.h>
#include <stdio.h>

#ifdef ENABLE_NAME
#include <string.h>
#endif

#include "common/netaddr.h"

#include "olsr.h"
#include "util.h"
#include "debug.h"
#include "routing.h"
#include "constants.h"
#include "list.h"

static struct olsr_node* _new_olsr_node(struct netaddr* addr,
	uint8_t distance, metric_t metric, uint8_t vtime, char* name) {

	struct olsr_node* n = calloc(1, sizeof(struct olsr_node));

	if (n == NULL)
		return NULL;

	n->addr = netaddr_dup(addr);

	if (n->addr == NULL) {
		free(n);
		return NULL;
	}

	n->node.key = n->addr;
	n->type = NODE_TYPE_OLSR;
	n->distance = distance;
	n->link_metric = metric;
	n->expires = time_now() + vtime;
#ifdef ENABLE_NAME
	if (name)
		n->name = strdup(name);
#endif

	avl_insert(get_olsr_head(), &n->node);
	return n;
}

/*
 * find a new route for nodes that use last_addr as their default route
 * if lost_node_addr is not null, all reference to it will be removed (aka lost node)
 */
static void _update_children(struct netaddr* last_addr, struct netaddr* lost_node_addr) {
	TRACE_FUN("%s, %s", netaddr_to_str_s(&nbuf[0], last_addr),
		netaddr_to_str_s(&nbuf[1], lost_node_addr));

	struct olsr_node *node;
	avl_for_each_element(get_olsr_head(), node, node) {

		if (lost_node_addr != NULL)
			remove_other_route(node, lost_node_addr);

		if (node->last_addr != NULL && netaddr_cmp(node->last_addr, last_addr) == 0) {

			if (lost_node_addr != NULL)
				remove_default_node(node);
			else
				push_default_route(node);

			add_free_node(node);

			_update_children(node->addr, lost_node_addr);
		}
	}
}

static void _olsr_node_expired(struct olsr_node* node) {
	TRACE_FUN();

	remove_default_node(node);
	_update_children(node->addr, NULL);

	add_free_node(node);

	// 1-hop neighbors will become normal olsr_nodes here, should we care? (possible waste of memory)
}

static void _remove_olsr_node(struct olsr_node* node) {
	TRACE_FUN();

	avl_remove(get_olsr_head(), &node->node);
	remove_free_node(node);

	/* remove other routes from node that is about to be deleted */
	char skipped;
	struct alt_route *route, *prev;
	simple_list_for_each_safe(node->other_routes, route, prev, skipped) {
		netaddr_free(route->last_addr);
		simple_list_for_each_remove(&node->other_routes, route, prev);
	}

	remove_default_node(node);
	_update_children(node->addr, node->addr);

#ifdef ENABLE_NAME
	if (node->name)
		free(node->name);
#endif
	netaddr_free(node->addr);
	free(node);
}

static bool _route_expired(struct olsr_node* node, struct netaddr* last_addr) {
	if (node->last_addr != NULL && netaddr_cmp(node->last_addr, last_addr) == 0)
		return time_now() > node->expires;

	if (node->other_routes == NULL)
		return true;

	struct alt_route* route = simple_list_find_memcmp(node->other_routes, last_addr);

	if (route == NULL)
		return true;

	return time_now() > route->expires;
}

#ifdef ENABLE_HYSTERESIS
static void _update_link_quality(struct nhdp_node* node) {
	TRACE_FUN("%s", netaddr_to_str_s(&nbuf[0], h1_super(node)->addr));
	if (_route_expired(h1_super(node), get_local_addr()))
		node->link_quality = node->link_quality * (1 - HYST_SCALING);
	else
		node->link_quality = node->link_quality * (1 - HYST_SCALING) + HYST_SCALING;

	if (!h1_super(node)->pending && node->link_quality < HYST_LOW) {
		h1_super(node)->pending = 1;
		h1_super(node)->lost = LOST_ITER_MAX;
		node->mpr_neigh_flood = 0;
		node->mpr_neigh_route = 0;

		add_free_node(h1_super(node));
		push_default_route(h1_super(node));
		_update_children(h1_super(node)->addr, NULL);
	}

	if (h1_super(node)->pending && node->link_quality > HYST_HIGH) {
		h1_super(node)->pending = 0;
		h1_super(node)->lost = 0;

		/* node may just have become a 1-hop node */
		if (h1_super(node)->last_addr != NULL)
			push_default_route(h1_super(node));

		add_free_node(h1_super(node));
	}
}
#endif

bool remove_expired(struct olsr_node* node) {
	time_t _now = time_now();

#ifdef ENABLE_HYSTERESIS
	if (node->type == NODE_TYPE_NHDP)
		_update_link_quality(h1_deriv(node));
#endif

	char skipped;
	struct alt_route *route, *prev;
	simple_list_for_each_safe(node->other_routes, route, prev, skipped) {
		if (_now - route->expires < HOLD_TIME)
			continue;

		DEBUG("alternative route to %s (%s) via %s expired, removing it",
			node->name, netaddr_to_str_s(&nbuf[0], node->addr),
			netaddr_to_str_s(&nbuf[1], route->last_addr));
		simple_list_for_each_remove(&node->other_routes, route, prev);
	}

	if (_now - node->expires > HOLD_TIME) {

		DEBUG("%s (%s) expired",
			node->name, netaddr_to_str_s(&nbuf[0], node->addr));

		if (node->other_routes == NULL) {
			_remove_olsr_node(node);
			return true;
		} else
			_olsr_node_expired(node);
	}

	return false;
}

void route_expired(struct olsr_node* node, struct netaddr* last_addr) {
	DEBUG("%s (%s) over %s expired",
			node->name, netaddr_to_str_s(&nbuf[0], node->addr),
			netaddr_to_str_s(&nbuf[1], last_addr));

	if (node->last_addr == NULL || netaddr_cmp(node->last_addr, last_addr) != 0) {
		remove_other_route(node, last_addr);
		if (node->other_routes == NULL)
			_remove_olsr_node(node);
		return;
	}

	if (node->other_routes == NULL)
		_remove_olsr_node(node);
	else
		_olsr_node_expired(node);
}

void add_olsr_node(struct netaddr* addr, struct netaddr* last_addr, uint8_t vtime, uint8_t distance, metric_t metric, char* name) {
	struct olsr_node* n = get_node(addr);

	if (n == NULL)
		n = _new_olsr_node(addr, distance, metric, vtime, name);

	if (n == NULL) {
		puts("ERROR: add_olsr_node failed - out of memory");
		return;
	}

	/* we have added a new node */
	if (n->last_addr == NULL) {
#ifdef ENABLE_NAME
		if (n->name == NULL && name != NULL)
			n->name = strdup(name);
#endif
		add_other_route(n, last_addr, distance, metric, vtime);
		add_free_node(n);

		return;
	}

	struct olsr_node* new_lh = get_node(last_addr);

	// TODO: netaddr_cmp(last_addr, n->last_addr)

	/* minimize MPR count */
	if (new_lh->type == NODE_TYPE_NHDP && new_lh->path_metric + metric == n->path_metric &&
		netaddr_cmp(last_addr, n->last_addr) != 0) {
		struct nhdp_node* cur_mpr = h1_deriv(get_node(n->next_addr));

		/* see if the new route is better, that means uses a neighbor that is alreay
		   used for reaching (more) 2-hop neighbors. */
		if (cur_mpr != NULL && (new_lh != NULL &&
			h1_deriv(new_lh)->mpr_neigh_route + 1 > cur_mpr->mpr_neigh_route)) {
			DEBUG("switching routing MPR (%s -> %s)", h1_super(cur_mpr)->name, new_lh->name);
			_update_children(n->addr, NULL);
			push_default_route(n);
			add_free_node(n);
		}
	}

	if (new_lh->path_metric + metric >= n->path_metric) {
		add_other_route(n, last_addr, distance, metric, vtime);
		return;
	}

	DEBUG("better route found (old: %d (%d) hops over %s new: %d (%d) hops over %s)",
		n->distance, n->path_metric, netaddr_to_str_s(&nbuf[0], n->last_addr),
		distance, new_lh->path_metric + metric, netaddr_to_str_s(&nbuf[1], last_addr));

	n->distance = distance; // only to keep free_nodes sorted
	_update_children(n->addr, NULL);
	push_default_route(n);
	add_other_route(n, last_addr, distance, metric, vtime);
	add_free_node(n);
}

bool is_known_msg(struct netaddr* addr, uint16_t seq_no, uint8_t vtime) {
	struct olsr_node* node = get_node(addr);
	if (!node) {
		node = _new_olsr_node(addr, 255, METRIC_MAX, vtime, NULL);
		node->seq_no = seq_no;
		return false;
	}

	uint16_t tmp = node->seq_no;
	node->seq_no = seq_no;
	/*	S1 > S2 AND S1 - S2 < MAXVALUE/2 OR
		S2 > S1 AND S2 - S1 > MAXVALUE/2	*/
	if ((seq_no > tmp && seq_no - tmp < (1 << 15)) ||
		(seq_no < tmp && tmp - seq_no > (1 << 15)) )
		return false;

	return true;
}

#ifdef ENABLE_DEBUG_OLSR
void print_topology_set(void) {
	DEBUG();
	DEBUG("---[ Topology Set ]--");
	DEBUG(" [ %s | %s ]\n", netaddr_to_str_s(&nbuf[0], get_local_addr()), local_name);

	struct olsr_node* node;
	struct alt_route* route;
	avl_for_each_element(get_olsr_head(), node, node) {
		DEBUG("%s (%s)\t=> %s; %d hops, metric: %d, next: %s (%d), %ld s [%d] %s %.2f [%d|%d] %s%s",
			netaddr_to_str_s(&nbuf[0], node->addr),
			node->name,
			netaddr_to_str_s(&nbuf[1], node->last_addr),
			node->distance,
			node->path_metric,
			netaddr_to_str_s(&nbuf[2], node->next_addr),
			node->link_metric,
			node->expires - time_now(),
			node->seq_no,
			node->type != NODE_TYPE_NHDP ? "" : node->pending ? "pending" : "",
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->link_quality,
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->mpr_neigh_flood,
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->mpr_neigh_route,
			node->type != NODE_TYPE_NHDP ? "" : h1_deriv(node)->mpr_slctr_flood ? "[F" : "[ ",
			node->type != NODE_TYPE_NHDP ? "" : h1_deriv(node)->mpr_slctr_route ? "R]" : " ]"
			);
		simple_list_for_each (node->other_routes, route) {
			DEBUG("\t\t\t=> %s (%d); %ld s",
				netaddr_to_str_s(&nbuf[0], route->last_addr),
				route->link_metric,
				route->expires - time_now());
		}
	}
	DEBUG("---------------------");
	DEBUG();
}

void print_routing_graph(void) {
	puts("\n----BEGIN ROUTING GRAPH----\n");
	puts("subgraph routing {");
	puts("\tedge [ color = red ]");
	struct olsr_node* node, *tmp;
	avl_for_each_element(get_olsr_head(), node, node) {
		if (node->addr != NULL && node->last_addr != NULL) {
			tmp = get_node(node->last_addr);
			printf("\t%s -> %s\n", tmp ? tmp->name : local_name, node->name);
		}
	}
	puts("}");

	puts("subgraph mpr_f {");
	puts("\tedge [ color = green ]");
	puts("// BEGIN FLOODING MPR");
	avl_for_each_element(get_olsr_head(), node, node) {
		if (node->distance == 1 && h1_deriv(node)->mpr_slctr_flood) {
			printf("\t%s -> %s\n", node->name, local_name);
		}
	}
	puts("// END FLOODING MPR");
	puts("}");

	puts("subgraph mpr_r {");
	puts("\tedge [ color = blue ]");
	puts("// BEGIN ROUTING MPR");
	avl_for_each_element(get_olsr_head(), node, node) {
		if (node->distance == 1 && h1_deriv(node)->mpr_slctr_route) {
			printf("\t%s -> %s\n", node->name, local_name);
		}
	}
	puts("// END ROUTING MPR");
	puts("}");

	puts("\n----END ROUTING GRAPH----\n");

}
#else
void print_topology_set(void) {
	struct netaddr_str nbuf[3];

	struct alt_route* route;
	struct olsr_node* node;

	puts("");
	puts("---[ Topology Set ]--");
#ifdef ENABLE_NAME
	printf(" [ %s | %s ]\n", netaddr_to_str_s(&nbuf[0], get_local_addr()), local_name);
#else
	printf(" [%s]\n", netaddr_to_str_s(&nbuf[0], get_local_addr()));
#endif

	avl_for_each_element(get_olsr_head(), node, node) {
#ifdef ENABLE_NAME
		printf("%s (%s)\t=> %s; %d hops, next: %s, %ld s [%d] %s %.2f [%d|%d] %s%s\n",
#else
		printf("%s\t=> %s; %d hops, next: %s, %ld s [%d] %s %.2f [%d|%d] %s%s\n",
#endif
			netaddr_to_str_s(&nbuf[0], node->addr),
#ifdef ENABLE_NAME
			node->name,
#endif
			netaddr_to_str_s(&nbuf[1], node->last_addr),
			node->distance,
			netaddr_to_str_s(&nbuf[2], node->next_addr),
			node->expires - time_now(),
			node->seq_no,
			node->type != NODE_TYPE_NHDP ? "" : node->pending ? "pending" : "",
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->link_quality,
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->mpr_neigh_flood,
			node->type != NODE_TYPE_NHDP ? 0 : h1_deriv(node)->mpr_neigh_route,
			node->type != NODE_TYPE_NHDP ? "" : h1_deriv(node)->mpr_slctr_flood ? "[F" : "[ ",
			node->type != NODE_TYPE_NHDP ? "" : h1_deriv(node)->mpr_slctr_route ? "R]" : " ]"
			);
		simple_list_for_each (node->other_routes, route) {
			printf("\t\t\t=> %s; %ld s\n",
				netaddr_to_str_s(&nbuf[0], route->last_addr),
				route->expires - time_now());
		}
	}
	puts("---------------------");

}
void print_routing_graph(void) {}
#endif
