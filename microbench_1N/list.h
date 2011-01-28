#ifndef _LIST_
#define _LIST_

typedef struct my_list_node
{
  double v;
  struct my_list_node *next;
} my_list_node2;

/* destroy the list */
void list_del(my_list_node *L);

/* Add a new element in the list */
my_list_node* list_add(my_list_node *L, double v);

/* compute the average of the list values */
double list_compute_avg(my_list_node *L);

/* compute the standard deviation of the list values */
double list_compute_stddev(my_list_node *L, double avg);

#endif
