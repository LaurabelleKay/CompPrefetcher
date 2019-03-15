#include "cache.h"

#define MK_ENTRY_COUNT 1024
#define PREDICTION_COUNT 3

typedef struct pred_addr
{
    uint64_t addr;
    uint32_t probability;
}pred_addr_t;

typedef struct markov_entry
{
    uint64_t miss_addr;
    pred_addr_t predictions[PREDICTION_COUNT];
    uint32_t lru;

}markov_entry_t;

markov_entry_t mk_table[MK_ENTRY_COUNT];

uint8_t fr;
uint64_t prev_addr;
int prev_index;

void CACHE::l2c_prefetcher_initialize()
{
    int i,j;
    for(i = 0; i < MK_ENTRY_COUNT; i++)
    {
        mk_table[i].miss_addr = 0;
        mk_table[i].lru = 0;
        for(j = 0; j < PREDICTION_COUNT; j++)
        {
            mk_table[i].predictions[j].addr = 0;
            mk_table[i].predictions[j].probability = 0;
        }
    }

    fr = 1;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int index = -1;
    int pindex = -1;
    //TODO: On first run, don't search for previous address

    //check entry for last address to see if this address is in it
    //if so, add to probability
    //if not, remove one with lowest probability and add it
    int min = 1E04;
    for (int i = 0; i < PREDICTION_COUNT; i++)
    {
        if (mk_table[prev_index].predictions[i].probability < min)
        {
            min = mk_table[prev_index].predictions[pindex].probability;
            pindex = i;
            break;
        }
    }

    mk_table[prev_index].predictions[pindex].addr = cl_addr;
    mk_table[prev_index].predictions[pindex].probability = 1;


    //Look for this address in our table
    for(index = 0; index < MK_ENTRY_COUNT; index++)
    {
        if(mk_table[index].miss_addr == cl_addr)
        {
            break;
        }
    }

    //If this address is not in the table, allocate it an entry
    if(index == MK_ENTRY_COUNT)
    {
        for(index = 0; index < MK_ENTRY_COUNT; index ++)
        {
            if(mk_table[index].lru == (MK_ENTRY_COUNT - 1))
            {
                break;
            }
        }

        mk_table[index].miss_addr = cl_addr;

        for(int i = 0; i < MK_ENTRY_COUNT; i++)
        {
            if(mk_table[i].lru < mk_table[index].lru)
            {
                mk_table[i].lru++;
            }
        }
        mk_table[index].lru = 0;
        prev_addr = addr;

        return;
    }

    if(index == -1) {assert(0);}

    for(int i = 0; i < PREDICTION_COUNT; i++)
    {

    }
}

void CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
}

void CACHE::l2c_prefetcher_final_stats()
{
}
