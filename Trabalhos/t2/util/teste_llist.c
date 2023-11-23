//
// Created by jvbrates on 11/21/23.
//

#include "linked_list.h"
#include "stdio.h"

typedef struct{
  node_t  *n;
}teste;

void print_node(node_t *n){
  printf("%i<-[%i]->%i\n",
         *(int*)(llist_get_packet(llist_get_previous(n))),
         *(int*)(llist_get_packet(n)),
         *(int*)(llist_get_packet(llist_get_next(n))));



}

int main(){
  int a = 1;
  int b = 2;
  int c = 3;
  node_t *node1 = llist_create_node_round(&a, 1);
  node_t *node2 = llist_create_node_round(&b, 2);
  node_t *node3 = llist_create_node_round(&c, 3);


  print_node(node1);

  llist_node_unlink(node1);

  print_node(node1);

  llist_add_node_next(node1, node1);

  print_node(node1);




  return 0;
}