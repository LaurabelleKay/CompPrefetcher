#ifndef PREFETCHER_H
#define PREFETCHER_H

#include "cache.h"

//Stride Prefetcher -----------------------------------------------------------
#define IP_TRACKER_COUNT 1024
#define PREFETCH_DEGREE 3

class STRIDE
{
    public:
        typedef struct IP_TRACKER
        {
            // the IP we're tracking
            uint64_t ip;

            // the last address accessed by this IP
            uint64_t last_cl_addr;

            // the stride between the last two addresses accessed by this IP
            int64_t last_stride;

            // use LRU to evict old IP trackers
            uint32_t lru;
        }IP_TRACKER_T;

    void initialize();
    uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
    void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
    void stats();
};

//Next Line -------------------------------------------------------------------
#define N_DEGREE 1

class NEXTLINE
{
    void initialize();
    uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
    void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
    void stats();
};


//Distance --------------------------------------------------------------------
#define DISTANCE_COUNT 1024

class DISTANCE  
{

    void initialize();
    uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
    void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
    void stats();
};

#endif