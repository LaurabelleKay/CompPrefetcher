#ifndef PREFETCHER_H
#define PREFETCHER_H

#include "cache.h"

//Stride Prefetcher -----------------------------------------------------------

#define IP_TRACKER_COUNT 1024
#define PREFETCH_DEGREE 3
#define CONFIDENCE_THRESHOLD 4

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

    uint32_t confidence;

    // use LRU to evict old IP trackers
    uint32_t lru;
  } Ip_tracker_t;

  //TODO: Implement cull, after trial period, remove ip's that are below a cetrain confidence
  //solidify ip list
  static void initialize();
  static void operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
  static void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
  static void stats();
};

//Next Line -------------------------------------------------------------------

#define N_DEGREE 1

class NEXTLINE
{
public:
  static void initialize();
  static void operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
  static void fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr);
  static void stats();
};

//Distance --------------------------------------------------------------------

#define TABLE_COUNT 8
#define IP_COUNT 64
#define DISTANCE_COUNT 4

class DISTANCE
{
public:
  typedef struct Distance_table
  {
    int tag;
    int64_t distances[DISTANCE_COUNT];
    int lru;
  } Distance_table_t;

  class IPENTRY
  {
  public:
    int fr;
    int lru;
    int previous_index;
    uint64_t ip;
    uint64_t previous_addr;
    Distance_table_t dtables[TABLE_COUNT];

    IPENTRY()
    {
      for (int i = 0; i < TABLE_COUNT; i++)
      {
        dtables[i].lru = i;
        previous_addr = 0;
        previous_index = 0;
        ip = 0;
        fr = 1;
      }
    }

    void operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
  };

  //TODO: Implement cull, after trial period, remove ip's that are below a cetrain confidence
  //solidify ip list
  static void initialize();
  static void operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);
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

  static void insert(uint64_t addr, uint8_t pf);
  static void remove(uint64_t addr);
  static int search(uint64_t addr);
};

//Composite--------------------------------------------------------------------
#define BUFFER_SIZE 64
#define TRIAL_PERIOD 500

class PFBUFFER
{
public:
  typedef struct pf_entry
  {
    uint64_t pf_addr;
    uint32_t lru;
  } pf_entry_t;

  pf_entry_t entry[BUFFER_SIZE];
  uint32_t pf_use;
  uint32_t pf_count;
  float accuracy;

  PFBUFFER()
  {
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
      entry[i].pf_addr = 0;
      entry[i].lru = i;
    }
  }

  void insert(uint64_t addr);
  void remove(uint64_t addr);
  void empty();
  int search(uint64_t addr);
};

#endif