//
// Created by jvbrates on 9/16/23.
//

#ifndef T1_LINKED_LIST_C_H
#define T1_LINKED_LIST_C_H

typedef struct node_t node_t;

node_t *llist_create_node(void *packet, int key);
node_t *llist_create_node_round(void *packet, int key);

node_t *llist_get_next(node_t*);

node_t *llist_get_previous(node_t*);

void *llist_get_packet(node_t *);

int llist_get_key(node_t *node);

void *llist_node_search(node_t *first, int key);

int llist_add_node_next(node_t *self, node_t *next);

int llist_add_node_previous(node_t *self, node_t *next);

int llist_node_unlink(node_t *node);

node_t *llist_add_node(node_t **node_holder, node_t *node);

node_t *llist_remove_node(node_t **node_holder, int key);

void llist_delete_node(node_t *node);

void llist_destruct(node_t **node_holder);

typedef void * (*func)(node_t *, void *arg);

node_t *llist_iterate_nodes(node_t *start, func callback, void *arg);



#endif // T1_LINKED_LIST_C_H
