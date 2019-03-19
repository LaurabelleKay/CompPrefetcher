#ifndef PREFETCHER_H
#define PREFETCHER_H

#include "cache.h"

//Stride Prefetcher -----------------------------------------------------------

#define IP_TRACKER_COUNT 1024
#define PREFETCH_DEGREE 3

class STRIDE
{
    public:
        typedef struct Ip_tracker
        {
            // the IP we're tracking
            uint64_t ip;

            // the last address accessed by this IP
            uint64_t last_cl_addr;

            // the stride between the last two addresses accessed by this IP
            int64_t last_stride;

            // use LRU to evict old IP trackers
            uint32_t lru;
        }Ip_tracker_t;

    static void initialize();
    static uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
    static void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
    static void stats();
};

//Next Line -------------------------------------------------------------------

#define N_DEGREE 1

class NEXTLINE
{
    static void initialize();
    static uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
    static void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
    static void stats();
};


//Distance --------------------------------------------------------------------

#define TABLE_COUNT 1024
#define DISTANCE_COUNT 3

class DISTANCE  
{
    public:
      typedef struct Distance_table
      {
          int tag;
          int distances[DISTANCE_COUNT];
          int lru;
      } Distance_table_t;

      static void initialize();
      static uint64_t operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
      static void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
      static void stats();
};

//Cache tracker----------------------------------------------------------------

class CACHELINE
{
    public:
      uint64_t addr;
      uint8_t valid;
      uint8_t pf;

      CACHELINE()
      {
          addr = 0;
          valid = 0;
          pf = 0;
      }

      void insert(uint64_t addr, uint8_t pf);
      void remove(uint64_t addr);
      int search(uint64_t addr);
};

//Composite--------------------------------------------------------------------

class PFENTRY
{
    public:
        uint64_t pf_addr;
        uint32_t pf_tag;
        uint32_t lru;

        PFENTRY()
        {
            pf_addr = 0;
            pf_tag = 0;
            lru = 0;
        }

        void insert(uint64_t addr);
        void remove(uint64_t addr);
        int search(uint64_t addr);
};

#endif