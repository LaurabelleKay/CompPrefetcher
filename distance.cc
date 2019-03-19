#include "cache.h"

#define TABLE_COUNT 1024
#define DISTANCE_COUNT 3

typedef struct distance_table
{
    int tag;
    int distances[DISTANCE_COUNT];
    int lru;
} distance_table_t;

distance_table_t tables[TABLE_COUNT];

int fr;
int previous_distance;
int previous_index;
int previous_addr;
unsigned long long int previous_page;

void CACHE::l2c_prefetcher_initialize()
{
    int i, j;
    for (i = 0; i < TABLE_COUNT; i++)
    {
        tables[i].tag = -1E06;
        tables[i].lru = i;
        for (j = 0; j < DISTANCE_COUNT; j++)
        {
            tables[i].distances[j] = 0;
        }
    }

    fr = 1;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    unsigned long long int cl_address = addr >> LOG2_BLOCK_SIZE;
    unsigned long long int page = cl_address >> LOG2_BLOCK_SIZE;

    int distance;
    int index;
    int dindex;
    //if(cache_hit == 0) {return;}
    if (!fr)
    {
        index = -1;
        dindex = -1;

        //Calculate this distance
        distance = cl_address - previous_addr;
        if (distance = 0)
        {
            return;
        }

        for (dindex = 0; dindex < DISTANCE_COUNT; dindex++)
        {
            if (tables[previous_index].distances[dindex] == 0)
            {
                tables[previous_index].distances[dindex] = distance;
                break;
            }
        }

        for (index = 0; index < TABLE_COUNT; index++)
        {
            //Search for this distance tag in the table
            if (tables[index].tag == distance)
            {
                break;
            }
        }
        //If this distance is not in our table
        if (index == TABLE_COUNT)
        {

            for (index = 0; index < TABLE_COUNT; index++)
            {
                //Find the least recently used row in the table
                if (tables[index].lru == (TABLE_COUNT - 1))
                {
                    break;
                }
            }

            //Replace with this distance
            tables[index].tag = distance;

            //Increment LRU counter for rows least recently used than evicted row
            for (int i = 0; i < TABLE_COUNT; i++)
            {
                if (tables[i].lru < tables[index].lru)
                {
                    tables[i].lru++;
                }
            }
            //cout << "Assign " << distance << " new row in " << index << endl;
            tables[index].lru = 0;
            previous_distance = distance;
            previous_addr = cl_address;
            return;
        }

        //Check that we now have an index
        if (index == -1)
        {
            assert(0);
        }

        uint64_t pf_address;

        for (int i = 0; i < DISTANCE_COUNT; i++)
        {
            if (tables[index].distances[i] != 0)
            {
                pf_address = (cl_address + tables[index].distances[i]) << LOG2_BLOCK_SIZE;
                prefetch_line(ip, addr, pf_address, FILL_L2);
            }
        }
        /*if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
        {
            //FIXME: Son't return because we haven't set previous values
            return;
        }*/
        /* MSHR.occupancy;
        if (MSHR.occupancy < (MSHR.SIZE >> 1))
        {
            prefetch_line(ip, addr, pf_address, FILL_L2);
        }
        else
        {
            prefetch_line(ip, addr, pf_address, FILL_LLC);
        }*/

        //cout << "Previous set" << endl;
        //tables[previous_index].d1 = distance;
        previous_addr = cl_address;
        previous_distance = distance;
        previous_index = index;
        //TODO: update lru!!
    }
    else
    {
        fr = 0;
        cout << "First run" << fr << endl;
        previous_addr = cl_address;
        //previous_distance = distance;
    }
}

void CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
}

void CACHE::l2c_prefetcher_final_stats()
{
}