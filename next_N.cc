#include "cache.h"

#define PREFETCH_DEGREE 1

typedef struct cache_line
{
    uint64_t addr;
    uint8_t valid;
    uint8_t pf;
} cache_line_t;

cache_line_t cache[4096];
int cache_size;
int misses;
int acc_count;
int pf_count;
int pf_use;

//On cache fill, insert the line into our cache tracker
void cache_insert(uint64_t addr, uint8_t pf)
{
    int i;
    for (i = 0; i < cache_size; i++)
        //Find an empty slot in the cache
        if (cache[i].valid == 0)
        {
            cache[i].valid = 1;
            cache[i].addr = addr;
            cache[i].pf = pf;
            return;
        }
    cache[cache_size].valid = 1;
    cache[cache_size].addr = addr;
    cache[cache_size].pf = pf;
    cache_size++;
}

//Remove line from our tracker
void cache_remove(uint64_t addr)
{
    int i;
    for (i = 0; i < cache_size; i++)
        if (cache[i].addr == addr)
        {
            cache[i].valid = 0;
        }
}

//FInd an address in our tracker
int cache_search(uint64_t addr)
{
    int i;
    for (i = 0; i < cache_size; i++)
    {
        if (cache[i].addr == addr)
        {
            return i;
        }
    }
    return -1;
}

void CACHE::l2c_prefetcher_initialize()
{
    cout << "\"Name\": "
         << "\"Next N\"," << endl;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
    for (int i = 1; i < PREFETCH_DEGREE + 1; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
        prefetch_line(ip, addr, pf_addr, FILL_L2);
    }

    acc_count++;
    if (!cache_hit)
    {
        misses++;
    }
    int index = cache_search(addr);
    if (index != -1)
    {
        if (cache[index].pf == 1)
        {
            pf_use++;
            cache[index].pf = 0;
        }
    }
}

void CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
    uint64_t cl_address = addr >> LOG2_BLOCK_SIZE;
    uint64_t evict_addr = addr >> LOG2_BLOCK_SIZE;
    cache_insert(addr, prefetch);
    if (prefetch == 1)
    {
        pf_count++;
    }
    //int index = cache_search(evict_addr);
    cache_remove(evicted_addr);
}

void CACHE::l2c_prefetcher_final_stats()
{
    float pf_accuracy = (pf_use * 1.0) / (pf_count * 1.0);
    cout << "\"Accuracy\": " << pf_accuracy << endl;
}
