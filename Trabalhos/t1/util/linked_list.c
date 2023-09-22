#include <stdlib.h>
#include <stdio.h>

typedef struct node node_t;

struct node {
  unsigned int key;
  void *packet;
  node_t *next;
  node_t *previous;
};


void delete_node(node_t *node){
  if (node)
    free(node);
}

node_t * newNode(void * packet, unsigned int key){
  node_t *ret = malloc(sizeof (node_t));
  ret->packet = packet;
  ret->key = key;
  ret->next = NULL;
  ret->previous = NULL;
  return ret;
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