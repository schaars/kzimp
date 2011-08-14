#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sglib.h>
#include <bstrlib.h>
#include <types.h>
#include <hashTable.h>
#include <likwid.h>

typedef struct linkedList {
    pthread_t tid;
    LikwidThreadResults* hashTable[HASH_TABLE_SIZE];
    uint32_t currentMaxSize;
    uint32_t numberOfRegions;
    struct linkedList* next;
} linkedList;


static pthread_key_t buffer_key;
static pthread_once_t buffer_key_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
static linkedList* threadList;

/* ======================================================================== */

/* By Paul Hsieh (C) 2004, 2005.  Covered under the Paul Hsieh derivative 
   license. See: 
   http://www.azillionmonkeys.com/qed/weblicense.html for license details.

   http://www.azillionmonkeys.com/qed/hash.html */


#define get16bits(d) (*((const uint16_t *) (d)))

static uint32_t
hashFunction (LikwidThreadResults* item) {
    const char* data = bdata(item->label);
    int len = HASH_TABLE_SIZE;
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= data[sizeof (uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}
/* ======================================================================== */
#define SLIST_COMPARATOR(e1, e2)    bstrncmp((e1)->label,(e2)->label,100)

SGLIB_DEFINE_LIST_PROTOTYPES(linkedList,SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(linkedList,SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_PROTOTYPES(LikwidThreadResults,SLIST_COMPARATOR , next)
SGLIB_DEFINE_LIST_FUNCTIONS(LikwidThreadResults,SLIST_COMPARATOR , next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(LikwidThreadResults, HASH_TABLE_SIZE, hashFunction)
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(LikwidThreadResults, HASH_TABLE_SIZE, hashFunction)

static void
buffer_destroy(void * buf)
{
    free(buf);
}

static void
buffer_init_alloc()
{
    pthread_key_create(&buffer_key, buffer_destroy);
}


LikwidThreadResults*
hashTable_get(bstring label)
{
    LikwidThreadResults li;
    LikwidThreadResults* resEntry;

    li.label = label;
    pthread_once(&buffer_key_once, buffer_init_alloc);
    linkedList** init = (linkedList**) pthread_getspecific(buffer_key);

    if (init == NULL)
    {
        linkedList** init = (linkedList**) malloc(sizeof(linkedList*));
        /* add thread in linked list
         * and initialize structure */
        pthread_mutex_lock( &mutex1 );
        {
            linkedList* tpt = (linkedList*) malloc(sizeof(linkedList));
            tpt->tid =  pthread_self();
            tpt->numberOfRegions = 0;
            sglib_hashed_LikwidThreadResults_init(tpt->hashTable);
            sglib_linkedList_add(&threadList,tpt); 
            (*init) = tpt;
        }
        pthread_mutex_unlock( &mutex1 );

        /* remember pointer to thread data in thread local storage */
        pthread_setspecific(buffer_key, init);

        /* create new region and add to hashtable */
        if ((resEntry = sglib_hashed_LikwidThreadResults_find_member((*init)->hashTable, &li)) == NULL) 
        {
            resEntry = (LikwidThreadResults*) malloc(sizeof(LikwidThreadResults));
            resEntry->label = bstrcpy (label);
            resEntry->coreId  = likwid_getProcessorId();
            resEntry->time = 0.0;
            resEntry->count = 0;
            sglib_hashed_LikwidThreadResults_add((*init)->hashTable, resEntry);
        }
    }
    else
    {
        /* if region is not known create new region and add to hashtable */
        if ((resEntry = sglib_hashed_LikwidThreadResults_find_member((*init)->hashTable, &li)) == NULL) 
        {
            resEntry = (LikwidThreadResults*) malloc(sizeof(LikwidThreadResults));
            resEntry->label = bstrcpy (label);
            resEntry->coreId  = likwid_getProcessorId();
            resEntry->time = 0.0;
            resEntry->count = 0;
            sglib_hashed_LikwidThreadResults_add((*init)->hashTable, resEntry);
        }
    }

    return resEntry;
}

void
hashTable_finalize(int* numberOfThreads, int* numberOfRegions, LikwidResults** results)
{
    int init = 0;
    int threadId = 0;
    int regionId = 0;
    (*numberOfThreads) = sglib_linkedList_len(threadList);
    linkedList*  ll;
    struct sglib_linkedList_iterator it;
    struct sglib_hashed_LikwidThreadResults_iterator hash_it;
    (*numberOfRegions) = 0;

    /* iterate over all threads */
    for(ll=sglib_linkedList_it_init(&it, threadList);
            ll!=NULL; ll=sglib_linkedList_it_next(&it))
    {
        LikwidThreadResults* hash  = NULL;

        if (!init)
        {
            init =1;
            for(hash=sglib_hashed_LikwidThreadResults_it_init(&hash_it,ll->hashTable);
                    hash!=NULL; hash=sglib_hashed_LikwidThreadResults_it_next(&hash_it))
            {
                ll->numberOfRegions++;
            }
            (*numberOfRegions) = ll->numberOfRegions;
            ll->numberOfRegions = 0;

            /* allocate data structure */
            (*results) = (LikwidResults*) malloc((*numberOfRegions) * sizeof(LikwidResults));

            for (int i=0; i<(*numberOfRegions); i++)
            {
                (*results)[i].time = (double*) malloc((*numberOfThreads) * sizeof(double));
                (*results)[i].count = (uint32_t*) malloc((*numberOfThreads) * sizeof(uint32_t));
                (*results)[i].counters = (double**) malloc((*numberOfThreads) * sizeof(double*));

                for (int j=0;j<(*numberOfThreads); j++)
                {
                    (*results)[i].counters[j] = (double*) malloc(NUM_PMC * sizeof(double));
                }
            }
        }

        regionId = 0;
        /* iterate over all regions in thread */
        for(hash=sglib_hashed_LikwidThreadResults_it_init(&hash_it,ll->hashTable);
                hash!=NULL; hash=sglib_hashed_LikwidThreadResults_it_next(&hash_it))
        {
            (*results)[regionId].tag = bstrcpy (hash->label);
            (*results)[regionId].count[threadId] = hash->count;
            (*results)[regionId].time[threadId] = hash->time;

            for (int i=0; i<NUM_PMC; i++)
            {
                (*results)[regionId].counters[threadId][i] = hash->PMcounters[i];
            }

            ll->numberOfRegions++;
            regionId++;
        }

        if ((*numberOfRegions) != ll->numberOfRegions)
        {
            fprintf(stderr,"Inconsistent number of regions!\n");
        }

        threadId++;
    }

    pthread_key_delete(buffer_key);
}


