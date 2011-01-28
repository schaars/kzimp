#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

/* destroy the list */
void list_del(my_list_node2 *L)
{
  if (L)
  {
    list_del(L->next);
    free(L);
  }
}

/* Add a new element in the list */
my_list_node2* list_add(my_list_node2 *L, double v)
{
  my_list_node2 *nnew = (my_list_node2*) malloc(sizeof(my_list_node2));

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
double list_compute_avg(my_list_node2 *L)
{
  double avg;
  int nb_elem;
  my_list_node2 *cur;

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
double list_compute_stddev(my_list_node2 *L, double avg)
{
  double sum, tmp;
  int nb_elem;
  my_list_node2 *cur;

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
