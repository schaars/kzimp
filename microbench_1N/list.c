#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

/* destroy the list */
void list_del(struct my_list_node *L)
{
  if (L)
  {
    list_del(L->next);
    free(L);
  }
}

/* Add a new element in the list */
struct my_list_node* list_add(struct my_list_node *L, double v)
{
  struct my_list_node *nnew = (struct my_list_node*) malloc(sizeof(struct my_list_node));

  if (!nnew)
  {
    printf("Error while allocating memory for new node\n");
    return L;
  }
  else
  {
    nnew->v = v;
    nnew->next = L;
    return nnew;
  }
}

/* compute the average of the list values */
double list_compute_avg(struct my_list_node *L)
{
  double avg;
  int nb_elem;
  struct my_list_node *cur;

  avg = 0;
  nb_elem = 0;
  cur = L;
  while (cur != NULL)
  {
    avg += cur->v;
    nb_elem++;
    cur = cur->next;
  }

  avg /= nb_elem;

  return avg;
}

/* compute the standard deviation of the list values */
double list_compute_stddev(struct my_list_node *L, double avg)
{
  double sum, tmp;
  int nb_elem;
  struct my_list_node *cur;

  sum = 0;
  nb_elem = 0;
  cur = L;
  while (cur != NULL)
  {
    tmp = cur->v - avg;
    sum += tmp * tmp;
    nb_elem++;
    cur = cur->next;
  }

  return sqrtf(sum / nb_elem);
}
