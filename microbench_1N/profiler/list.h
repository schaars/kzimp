#ifndef SYMB_LIST_H
#define SYMB_LIST_H 1

struct list {
   void *data;
   struct list* next;
};


#define list_add(p, ddata)                 \
   ({                                     \
   struct list *l = malloc(sizeof(*l));   \
   l->data = (void*)ddata;                \
   l->next = p;                           \
   l;                                     \
   })

#define list_foreach(l, n) \
      struct list* ___ll = l; \
      for(;___ll && (n = ___ll->data); ___ll = ___ll->next)

#endif

