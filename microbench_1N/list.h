#ifndef _LIST_
#define _LIST_

struct my_list_node
{
  double v;
  struct my_list_node *next;
};

/* destroy the list */
void list_del(struct my_list_node *L);

/* Add a new element in the list */
struct my_list_node* list_add(struct my_list_node *L, double v);

/* compute the average of the list values */
double list_compute_avg(struct my_list_node *L);

/* compute the standard deviation of the list values */
double list_compute_stddev(struct my_list_node *L, double avg);

#endif
