#include "cache.h"

#define TABLE_COUNT 8
#define IP_COUNT 128
#define PREFETCH_DEGREE 1

//FIXME: Change how the table is stored per IP
typedef struct distance_table
{
    int distance;
} distance_table_t;

typedef struct ip_table
{
    uint64_t ip;
    distance_table_t tables[TABLE_COUNT];
    int distance;
    int previous_d;
    int previous_i;
    uint64_t previous_a;
    int lru;
    int fr;
} ip_table_t;

ip_table_t iptable[IP_COUNT];
uint64_t iptrack;
//distance_table_t tables[TABLE_COUNT];

int fr;
int previous_distance;
int previous_index;
int previous_addr;
unsigned long long int previous_page;

void CACHE::l2c_prefetcher_initialize()
{
    fr = 1;
    cout << "Distance Prefetcher" << endl;
    int i, j;
    for (i = 0; i < IP_COUNT; i++)
    {
        iptable[i].ip = 0;
        iptable[i].lru = i;
        iptable[i].previous_a = 0;
        iptable[i].previous_i = 0;
        iptable[i].previous_d = 0;
        iptable[i].fr = 1;
        //iptable[i].tables[TABLE_COUNT];
        for (j = 0; j < TABLE_COUNT; j++)
        {
            iptable[i].tables[j].distance = 0;
        }
    }
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    if (fr)
    {
        iptrack = ip;
        fr = 0;
    }
    unsigned long long int cl_address = addr >> LOG2_BLOCK_SIZE;

    int distance;
    int index = -1;
    for (index = 0; index < IP_COUNT; index++)
    {
        if (iptable[index].ip == ip)
        {
            break;
        }
    }

    if (index == IP_COUNT)
    {
        for (index = 0; index < IP_COUNT; index++)
        {
            if (iptable[index].lru == (IP_COUNT - 1))
            {
                break;
            }
        }

        iptable[index].ip = ip;
        iptable[index].previous_a = cl_address;

        for (int i = 0; i < IP_COUNT; i++)
        {
            if (iptable[i].lru < iptable[index].lru)
            {
                iptable[i].lru++;
            }
        }
        iptable[index].lru = 0;
        return;
    }

    if (index == -1)
    {
        assert(0);
    }

    distance = cl_address - iptable[index].previous_a;
    if (distance == 0)
    {
        return;
    }
    iptable[index].previous_a = cl_address;

    int d_index = -1;
    for (d_index = 0; d_index < TABLE_COUNT; d_index++)
    {
        if (iptable[index].tables[d_index].distance == distance)
        {
            break;
        }
    }

    if (d_index == TABLE_COUNT)
    {
        for (d_index = 0; d_index < TABLE_COUNT; d_index++)
        {
            //find an empty slot in the table
            if (iptable[index].tables[d_index].distance == 0)
            {
                break;
            }
        }
        //If the table is full
        if (d_index == TABLE_COUNT)
        {
            return;
        }
        if (d_index == -1)
        {
            assert(0);
        }

        iptable[index].tables[d_index].distance = distance;
    }

    if (ip == iptrack)
    {
        for (int i = 0; i < TABLE_COUNT; i++)
        {
            cout << iptable[index].tables[i].distance << endl;
        }
    }

    int d2;
    //TODO: Add for loop for prefetch address
    d2 = iptable[index].tables[d_index + 1].distance;
    if (d2 == 0)
    {
        return;
    }

    uint64_t pf_address = (cl_address + d2) << LOG2_BLOCK_SIZE;
    /*if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
        {
            //FIXME: Son't return because we haven't set previous values
            return;
        }*/
    if (MSHR.occupancy < (MSHR.SIZE >> 1))
    {
        prefetch_line(ip, addr, pf_address, FILL_L2);
    }
    else
    {
        prefetch_line(ip, addr, pf_address, FILL_LLC);
    }
}

void CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
}

void CACHE::l2c_prefetcher_final_stats()
{
}