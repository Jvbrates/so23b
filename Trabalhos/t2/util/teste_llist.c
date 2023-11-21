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




  teste t = {node1};

  llist_add_node_next(node2, node3);
  llist_add_node_next(node3, node1);

  print_node(node1);
  print_node(node2);
  print_node(node3);


  llist_node_unlink(node3);

  print_node(node1);
  print_node(node2);
  print_node(node3);

  llist_add_node_previous(node2, node3);

  print_node(node1);
  print_node(node2);
  print_node(node3);


  llist_add_node_next(node3, node3);

  print_node(node1);
  print_node(node2);
  print_node(node3);

  return 0;
}