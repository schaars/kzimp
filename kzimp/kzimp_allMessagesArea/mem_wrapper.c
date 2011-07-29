/* 
 * In-kernel memory wrapper for my module
 */

#include <linux/types.h>        /* atomic ops */
#include <linux/spinlock.h>     /* spinlock */
#include <linux/list.h>         /* linked list */

#include "mem_wrapper.h"

#ifdef MEMORY_WRAPPING


#define MEM_WRAP_MALLOC    0
#define MEM_WRAP_FREE      1


// to enable more debug information about memory (de)allocation
//#define MEMORY_WRAPPING_DEBUG

struct mem_node
{
   void *ptr;
   size_t size;
   struct list_head next;
};

static LIST_HEAD(mem_ptr_list);
DEFINE_SPINLOCK(mem_ptr_lock);
static size_t mem_count = 0;


static void mem_printf(const char *op, void *ptr, size_t size)
{
#ifdef MEMORY_WRAPPING_DEBUG
   printk(KERN_DEBUG "%s: %lu\tmem_count=%lu\n", op, (unsigned long)size, (unsigned long)mem_count);
#endif
}


static void mem_count_update(void *ptr, int op, size_t size)
{
  if (op == MEM_WRAP_MALLOC)
  {
    mem_count += size;
    mem_printf("malloc", ptr, size);
  }
  else if (op == MEM_WRAP_FREE)
  {
    mem_count -= size;
    mem_printf("free", ptr, size);
  }
  else
  {
    mem_printf("unknown", 0, -1);
  }
}


static void ptr_add(void* ptr, size_t size)
{
   struct mem_node* node = kmalloc(sizeof(*node), GFP_KERNEL);
   node->ptr = ptr;
   node->size = size;
   list_add(&node->next, &mem_ptr_list);
}

static struct mem_node* ptr_search(void* ptr)
{
   struct mem_node *node;

   list_for_each_entry(node, &mem_ptr_list, next)
   {
      if (node->ptr == ptr)
      {
         return node;
      }
   }

   return NULL;
}


static void ptr_remove(struct mem_node* node)
{
   list_del(&node->next);
   kfree(node);
}


void* mem_wrapper_kmalloc(size_t size, gfp_t flags, const char *file, int line)
{
   void *ptr;

   spin_lock(&mem_ptr_lock);

   ptr = kmalloc(size, flags);
   if (ptr)
   {
      ptr_add(ptr, size);
      mem_count_update(ptr, MEM_WRAP_MALLOC, size);
   }

   spin_unlock(&mem_ptr_lock);

   return ptr;
}

void mem_wrapper_kfree(const void *ptr, const char *file, int line)
{
   struct mem_node *node;

   if (ptr)
   {
      spin_lock(&mem_ptr_lock);

      node = ptr_search((void*)ptr);

      if (node)
      {
         mem_count_update((void*)ptr, MEM_WRAP_FREE, node->size);
         ptr_remove(node);
         kfree(ptr);
      }

      spin_unlock(&mem_ptr_lock);
   }
}

void mem_wrapper_stats(void)
{
   struct list_head *p, *n;
   struct mem_node *node;

   spin_lock(&mem_ptr_lock);

   printk(KERN_DEBUG "Memory wrapper stats\tmem_count=%lu\n", (unsigned long)mem_count);

   // free the list to avoid a memory leak
   if (!list_empty(&mem_ptr_list))
   {
   list_for_each_safe(p, n, &mem_ptr_list)
   {
      node = list_entry(p, struct mem_node, next);
      list_del(p);
      kfree(node);
      }
   }

   spin_unlock(&mem_ptr_lock);
}

#endif
