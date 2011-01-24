#ifndef _LIST_
#define _LIST_


struct node {
  float v;
  struct node *next;
};


/* destroy the list */
void list_del(struct node *L);

/* Add a new element in the list */
struct node* list_add(struct node *L, float v);

/* compute the average of the list values */
float list_compute_avg(struct node *L);

/* compute the standard deviation of the list values */
float list_compute_stddev(struct node *L, float avg);

#endif
