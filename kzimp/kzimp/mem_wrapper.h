/* 
 * In-kernel memory wrapper for my module
 */

#ifndef _KERNEL_MODULE_MEM_WRAPPER_
#define _KERNEL_MODULE_MEM_WRAPPER_

#include <linux/slab.h>         /* kmalloc */
#include <linux/vmalloc.h>      /* vmalloc */


#ifdef MEMORY_WRAPPING

#warning "Memory wrapper enabled"

#define my_kmalloc(size, flags) mem_wrapper_kmalloc(size, flags, __FILE__, __LINE__)
#define my_kfree(ptr) mem_wrapper_kfree(ptr, __FILE__, __LINE__)
#define my_memory_stats() mem_wrapper_stats()

//fixme: we do not take them into account in our wrapper
#warning "There is no wrapper for vmalloc and vfree"
#define my_vmalloc(size) vmalloc(size)
#define my_vfree(ptr) vfree(ptr)

void* mem_wrapper_kmalloc(size_t size, gfp_t flags, const char *file, int line);
void mem_wrapper_kfree(const void *ptr, const char *file, int line);
void mem_wrapper_stats(void);

#else

#warning "No Memory wrapper" 

#define my_kmalloc(size, flags) kmalloc(size, flags)
#define my_kfree(ptr) kfree(ptr)
#define my_vmalloc(size) vmalloc(size)
#define my_vfree(ptr) vfree(ptr)
#define my_memory_stats() do {} while(0);

#endif

#endif
