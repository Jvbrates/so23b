#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct node_t node_t;

struct node_t {
  unsigned int key;
  void *packet;
  node_t *next;
  node_t *previous;
};

node_t *llist_create_node(void *packet, unsigned int key){
    node_t *node = malloc(sizeof(node_t));
    if(node) {
      node->packet = packet;
      node->key = key;
      node->next = NULL;
      node->previous = NULL;
    }
    return node;
}

node_t *llist_create_node_round(void *packet, unsigned int key){
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
    if (node)
      return node->packet;
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


typedef void * (*func)(node_t *, void *arg);
node_t *llist_iterate_nodes(node_t *start, func callback, void *arg){
    void *ret = callback(start, arg);

    if(ret)
      return ret;

    for (node_t *i = start->next; i != NULL && i!=start; i = i->next) {
      ret = callback(i, arg);
      if(ret)
        return ret;
    }

    return NULL;
}

void *llist_callback_search_key(node_t*node, void *arg){
    if(node->key == (unsigned  int) arg)
      return node;
    else
      return NULL;
}

void *llist_node_search(node_t *first, unsigned int key){
    return llist_iterate_nodes(first,
                               llist_callback_search_key,
                               (void *)key);
}

node_t *llist_remove_node(node_t **node_holder, unsigned int key){
    node_t *s = llist_iterate_nodes(*node_holder, llist_callback_search_key, (void *)key);

    if(!s)
      return NULL;
    if(*node_holder == s) // Case the holder pointer to node that will be removed
      if(s->next == s) // Case has one node in round list
        *node_holder = NULL;
      else
         *node_holder = s->next;
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


/*

int main(){

  node_t *A = llist_create_node_round((void *)65, 1);
  A->next = A;
  A->previous = A;

  node_t *salva_last = llist_create_node_round((void *)(52), 2);
  llist_add_node_next(A, salva_last);
//node_t * salva_last = addnext_to(A, (void *)66, 2);
  salva_last = llist_create_node_round((void *)(52), 0);
  llist_add_node_previous(A, salva_last);

  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);

  llist_remove_node(&A, 1);
  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);
  llist_remove_node(&A, 2);
  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);
  llist_remove_node(&A, 0);
  printf("pointer %p\n", A);
  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);
  // remove_node(salva_last);
  // llist_delete_node(&A, A);

  //Teste search | funcionando
  // node_t *s = llist_iterate_nodes(A, callback_search_key, (void *)1);



  //printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);

  return 0;
}
 */
