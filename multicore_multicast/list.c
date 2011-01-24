#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"


/* destroy the list */
void list_del(struct node *L) {
  if (L) {
    list_del(L->next);
    free(L);
  }
}

/* Add a new element in the list */
struct node* list_add(struct node *L, float v) {
  struct node *nnew = (struct node*)malloc(sizeof(struct node));

  if (!nnew) {
    printf("Error while allocating memory for new node\n");
    return L;
  } else {
    nnew->v = v;
    nnew->next = L;
    return nnew;
  }
}

/* compute the average of the list values */
float list_compute_avg(struct node *L) {
  float avg;
  int nb_elem;
  struct node *cur;

  avg = 0;
  nb_elem = 0;
  cur = L;
  while (cur != NULL) {
    avg += cur->v;
    nb_elem++;
    cur = cur->next;
  }

  avg /= nb_elem;

  return avg;
}

/* compute the standard deviation of the list values */
float list_compute_stddev(struct node *L, float avg) {
  float sum, tmp;
  int nb_elem;
  struct node *cur;

  sum = 0;
  nb_elem = 0;
  cur = L;
  while (cur != NULL) {
    tmp = cur->v - avg;
    sum += tmp*tmp;
    nb_elem++;
    cur = cur->next;
  }

  return sqrtf(sum/nb_elem);
}
