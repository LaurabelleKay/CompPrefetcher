#include "prefetcher.h"

//Stride-----------------------------------------------------------------------

STRIDE::Ip_tracker_t trackers[IP_TRACKER_COUNT];

//Distance---------------------------------------------------------------------

DISTANCE::Distance_table_t dtables[DISTANCE_COUNT];
int fr;
int previous_distance;
int previous_index;
int previous_addr;
unsigned long long int previous_page;

//Cache tracker----------------------------------------------------------------

CACHELINE cache[4096];
int cache_size;
int misses;
int acc_count;
int pf_count;
int pf_use;

PFENTRY pf_buffer[BUFFER_SIZE];
int pf_issued = 0;

void CACHE::l2c_prefetcher_initialize()
{
    STRIDE::initialize();
    DISTANCE::initialize();
}

void STRIDE::initialize()
{
    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        trackers[i].lru = i;
        trackers[i].ip = 0;
        trackers[i].last_cl_addr = 0;
        trackers[i].last_stride = 0;
    }
}

void DISTANCE::initialize()
{
    int i, j;
    for (i = 0; i < TABLE_COUNT; i++)
    {
        dtables[i].tag = -1E06;
        dtables[i].lru = i;
        for (j = 0; j < DISTANCE_COUNT; j++)
        {
            dtables[i].distances[j] = 0;
        }
    }

    fr = 1;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    NEXTLINE::operate(addr, ip, cache_hit, type);
    STRIDE::operate(addr, ip, cache_hit, type);
    DISTANCE::operate(addr, ip, cache_hit, type);

    for(int i = 0; i < pf_issued; i++)
    {

    }
}

uint64_t NEXTLINE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
    uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
    return pf_addr;
}

uint64_t STRIDE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    // check for a tracker hit
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int index = -1;
    for (index = 0; index < IP_TRACKER_COUNT; index++)
    {
        if (trackers[index].ip == ip)
            break;
    }

    // this is a new IP that doesn't have a tracker yet, so allocate one
    if (index == IP_TRACKER_COUNT)
    {
        for (index = 0; index < IP_TRACKER_COUNT; index++)
        {
            if (trackers[index].lru == (IP_TRACKER_COUNT - 1))
                break;
        }

        trackers[index].ip = ip;
        trackers[index].last_cl_addr = cl_addr;
        trackers[index].last_stride = 0;

        for (int i = 0; i < IP_TRACKER_COUNT; i++)
        {
            if (trackers[i].lru < trackers[index].lru)
                trackers[i].lru++;
        }
        trackers[index].lru = 0;

        //FIXME: return a value to indicate that there was no prefetch
        return;
    }

    // sanity check
    // at this point we should know a matching tracker index
    if (index == -1)
    {
        assert(0);
    }

    // calculate the stride between the current address and the last address
    // this bit appears overly complicated because we're calculating
    // differences between unsigned address variables
    int64_t stride = 0;
    if (cl_addr > trackers[index].last_cl_addr)
    {
        stride = cl_addr - trackers[index].last_cl_addr;
    }
    else
    {
        stride = trackers[index].last_cl_addr - cl_addr;
        stride *= -1;
    }

    //cout << "[IP_STRIDE] HIT  index: " << index << " lru: " << trackers[index].lru << " ip: " << hex << ip << " cl_addr: " << cl_addr << dec << " stride: " << stride << endl;

    // don't do anything if we somehow saw the same address twice in a row
    if (stride == 0)
    {
        return;
    }

    // only do any prefetching if there's a pattern of seeing the same
    // stride more than once
    if (stride == trackers[index].last_stride)
    {

        // do some prefetching
        for (int i = 0; i < PREFETCH_DEGREE; i++)
        {
            uint64_t pf_address = (cl_addr + (stride * (i + 1))) << LOG2_BLOCK_SIZE;

            // only issue a prefetch if the prefetch address is in the same 4 KB page
            // as the current demand access address
            if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                break;

            // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
            //FIXME: put in CACHE operate / put in queue  and operate can select later
            /*if (MSHR.occupancy < (MSHR.SIZE >> 1))
                prefetch_line(ip, addr, pf_address, FILL_L2);
            else
                prefetch_line(ip, addr, pf_address, FILL_LLC);*/
        }
    }

    trackers[index].last_cl_addr = cl_addr;
    trackers[index].last_stride = stride;

    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        if (trackers[i].lru < trackers[index].lru)
            trackers[i].lru++;
    }
    trackers[index].lru = 0;
}

uint64_t DISTANCE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
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
            if (dtables[previous_index].distances[dindex] == 0)
            {
                dtables[previous_index].distances[dindex] = distance;
                break;
            }
        }

        for (index = 0; index < TABLE_COUNT; index++)
        {
            //Search for this distance tag in the table
            if (dtables[index].tag == distance)
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
                if (dtables[index].lru == (TABLE_COUNT - 1))
                {
                    break;
                }
            }

            //Replace with this distance
            dtables[index].tag = distance;

            //Increment LRU counter for rows least recently used than evicted row
            for (int i = 0; i < TABLE_COUNT; i++)
            {
                if (dtables[i].lru < dtables[index].lru)
                {
                    dtables[i].lru++;
                }
            }
            //cout << "Assign " << distance << " new row in " << index << endl;
            dtables[index].lru = 0;
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
            if (dtables[index].distances[i] != 0)
            {
                pf_address = (cl_address + dtables[index].distances[i]) << LOG2_BLOCK_SIZE;
                //prefetch_line(ip, addr, pf_address, FILL_L2);
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

void PFENTRY::initialize()
{
    for(int i = 0; i < BUFFER_SIZE; i++)
    {
        pf_buffer[i].lru = i;
    }
}

void PFENTRY::insert(uint64_t addr)
{
    for(int i = 0; i < BUFFER_SIZE; i++)
    {
        if(pf_buffer[i].pf_addr == 0)
        {
            pf_buffer[i];pf_addr = addr;
        }
    }
}

void PFENTRY::remove(uint64_t addr)
{
    for(int i = 0; i < BUFFER_SIZE; i++)
    {
        if()
    }
}

void CACHELINE::insert(uint64_t addr, uint8_t pf)
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

void CACHELINE::remove(uint64_t addr)
{
    int i;
    for (i = 0; i < cache_size; i++)
        if (cache[i].addr == addr)
        {
            cache[i].valid = 0;
        }
}

int CACHELINE::search(uint64_t addr)
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
