#include "cache.h"

#define PREFETCH_DEGREE 1

void CACHE::l1d_prefetcher_initialize()
{
    cout << "CPU " << cpu << "Next N-line prefetcher" << endl;
}

void CACHE::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    for(int i = 1; i < PREFETCH_DEGREE + 1; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
        prefetch_line(ip, addr, pf_addr, FILL_L1);
    }    
}

void CACHE::l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{

}

void CACHE::l1d_prefetcher_final_stats()
{

}
