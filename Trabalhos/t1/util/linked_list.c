#include <stdlib.h>
#include <stdio.h>

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

node_t *llist_get_previous(node_t*){
    if(node)
      return node->previous;
    return NULL;
}

void *llist_get_packet(node_t *node){
    if (node)
      return node->packet;
}

void *llist_node_search(node_t *first, unsigned int PID);

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

node_t *llist_add_node(node_t **node_holder, node_t *node);

node_t *llist_delete_node(node_t **node_holder, unsigned int key);


void delete_node(node_t *node){
  if (node)
    free(node);
}


node_t *addnext_to(node_t *A, void * packet, unsigned int key){
  node_t *B = newNode(packet, key);
  node_t *C  = A->next;
  A->next = B;
  B->previous = A;
  if(C){
    C->previous = B;
    B->next = C;
  }
  return B;
}

node_t *addback_to(node_t *A, void* packet, unsigned int key){
  node_t *B = newNode(packet, key);
  node_t *C  = A->previous;
  A->previous = B;
  B->next = A;
  if(C){
    C->next = B;
    B->previous = C;
  }
  return B;
}

void remove_node(node_t *A){

  if(A->next)
    A->next->previous = A->previous;
  if(A->previous)
    A->previous->next = A->next;

}



int main(){

  node_t *A = newNode((void *)65, 1);
  A->next = A;
  A->previous = A;

  node_t * salva_last = addnext_to(A, (void *)66, 2);
  addback_to(salva_last, (void *)67, 3);

  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);

  remove_node(salva_last);
  printf("%d,%d,%d,%d\n", A->key, A->next->key, A->next->next->key, A->next->next->next->key);

  return 0;
}