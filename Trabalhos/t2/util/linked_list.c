#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct node_t node_t;

struct node_t {
  int key;
  void *packet;
  node_t *next;
  node_t *previous;
};

node_t *llist_create_node(void *packet, int key){
    node_t *node = malloc(sizeof(node_t));
    if(node) {
      node->packet = packet;
      node->key = key;
      node->next = NULL;
      node->previous = NULL;
    }
    return node;
}

node_t *llist_create_node_round(void *packet, int key){
    node_t *node = malloc(sizeof(node_t));
    if(node) {
      node->packet = packet;
      node->key = key;
      node->next = node;
      node->previous = node;
    }
    return node;
}

node_t *llist_get_next(node_t *node){
    if(node)
      return node->next;
    return NULL;
}

node_t *llist_get_previous(node_t*node){
    if(node)
      return node->previous;
    return NULL;
}

void *llist_get_packet(node_t *node){
    if (node) {
      return node->packet;
    }
    return NULL;
}

int llist_get_key(node_t *node){
    if(node)
      return node->key;
    return  -1;
}

void llist_set_key(node_t *node, int key){
    if(node)
      node->key  == key;

}

int llist_add_node_next(node_t *self, node_t *next){

    node_t  *self_old_next = self->next;
    self->next = next;
    next->previous = self;
    next->next = self_old_next;
    if(self_old_next)
      self_old_next->previous = next;

    return 0;
}

int llist_add_node_previous(node_t* self, node_t *previous){

    node_t  *self_old_previous = self->previous;
    self->previous = previous;
    previous->next = self;
    previous->previous = self_old_previous;
    if(self_old_previous)
      self_old_previous->next =  previous;

    return 0;
}

int llist_node_unlink(node_t *node){
    if(node->next)
      node->next->previous = node->previous;
    if(node->previous)
      node->previous->next = node->next;

    return 0;
}

node_t *llist_add_node(node_t **node_holder, node_t *node){
    if(!(*node_holder))
      *node_holder = node;

    llist_add_node_next((*node_holder), node);

    return node;
}



node_t *llist_iterate_nodes(node_t *start, func callback, void *arg){
    if(!start) {
      return NULL;
    }

    node_t *i = start->next;

    void *ret = callback(start, arg);

    if(ret) {
      return ret;
    }

    for (; i != NULL && i!=start;) {
      node_t  *i_ = i->next;
      ret = callback(i, arg);
      if(ret)
        return ret;
      i = i_;
    }

    return NULL;
}

void *llist_callback_search_key(node_t*node, void *arg){
    if(node->key == *((int *) arg))
      return node;
    else
      return NULL;
}

void *llist_node_search(node_t *first, int key){
    return llist_iterate_nodes(first,
                               llist_callback_search_key,
                               &key);
}

node_t *llist_remove_node(node_t **node_holder, int key){
    node_t *s = llist_iterate_nodes(*node_holder, llist_callback_search_key, &key);

    if(!s)
      return NULL;
    if(*node_holder == s) { // Case the holder pointer to node that will be removed
      if (s->next == s) { // Case has one node in round list
        *node_holder = NULL;
      } else {
        *node_holder = s->next;
      }
    }
    llist_node_unlink(s);

    return s; // Note that this function no deference the node, just remove it
    // from string
}

// Deference all the list, at end node_holder will pointer to unallocated memory
void llist_destruct(node_t **node_holder){
    node_t *next = (*node_holder)->next;

    free(*node_holder);

    while (next != *node_holder && next != NULL){
      node_t *aux = next->next;
      free(next);
      next = aux;
    }


}


void llist_delete_node(node_t *node){
  if (node)
    free(node);
}


