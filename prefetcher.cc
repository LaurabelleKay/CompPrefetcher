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

PFBUFFER nl_buffer;
PFBUFFER stride_buffer;
PFBUFFER distance_buffer;

uint32_t timer;
uint8_t trial_p;
uint32_t prefetcher;

void CACHE::l2c_prefetcher_initialize()
{
    cout << "\"Name\": "
         << "\"Single Pref\"," << endl;
    STRIDE::initialize();
    DISTANCE::initialize();
    trial_p = 1;
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
        dtables[i].tag = 0;
        dtables[i].lru = i;
        for (j = 0; j < DISTANCE_COUNT; j++)
        {
            dtables[i].distances[j] = 0;
        }
    }
    fr = 1;
}

int select()
{
    float max = 0;
    int pf = -1;

    pf = nl_buffer.accuracy > max ? 1 : pf;
    max = nl_buffer.accuracy > max ? nl_buffer.accuracy : max;

    pf = stride_buffer.accuracy > max ? 2 : pf;
    max = stride_buffer.accuracy > max ? stride_buffer.accuracy : max;

    pf = distance_buffer.accuracy > max ? 3 : pf;
    max = distance_buffer.accuracy > max ? distance_buffer.accuracy : max;

    return pf;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{

    if (trial_p)
    {
        timer++;
        if (timer == TRIAL_PERIOD)
        {
            prefetcher = select();
            trial_p = 0;
        }
        NEXTLINE::operate(addr, ip, cache_hit, type);
        STRIDE::operate(addr, ip, cache_hit, type);
        DISTANCE::operate(addr, ip, cache_hit, type);

        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            if (nl_buffer.entry[i].pf_addr != 0)
            {
                nl_buffer.pf_count++;
            }
            if (stride_buffer.entry[i].pf_addr != 0)
            {
                stride_buffer.pf_count++;
            }
            if (distance_buffer.entry[i].pf_addr != 0)
            {
                distance_buffer.pf_count++;
            }
        }
    }
    else
    {
        switch (prefetcher)
        {
        case 0:
        {
            NEXTLINE::operate(addr, ip, cache_hit, type);
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                if (nl_buffer.entry[i].pf_addr != 0)
                {
                    prefetch_line(ip, addr, nl_buffer.entry[i].pf_addr, FILL_L2);
                    nl_buffer.pf_count++;
                }
            }
        }
        case 1:
        {
            STRIDE::operate(addr, ip, cache_hit, type);
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                if (stride_buffer.entry[i].pf_addr != 0)
                {
                    prefetch_line(ip, addr, stride_buffer.entry[i].pf_addr, FILL_L2);
                    stride_buffer.pf_count++;
                }
            }
        }
        case 2:
        {
            DISTANCE::operate(addr, ip, cache_hit, type);
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                if (distance_buffer.entry[i].pf_addr != 0)
                {
                    prefetch_line(ip, addr, distance_buffer.entry[i].pf_addr, FILL_L2);
                    distance_buffer.pf_count++;
                }
            }
        }
        }
    }

    acc_count++;
    if (!cache_hit)
    {
        misses++;
    }
    int index = CACHELINE::search(addr);
    if (index != -1)
    {
        if (cache[index].pf == 1)
        {
            pf_use++;
            cache[index].pf = 0;
        }
    }
}

void NEXTLINE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
    for (int i = 0; i < N_DEGREE; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
        nl_buffer.insert(pf_addr);
    }

    int index = nl_buffer.search(addr);
    if (index != -1)
    {
        nl_buffer.pf_use++;
        nl_buffer.accuracy = (nl_buffer.pf_use * 1.0) / (nl_buffer.pf_count * 1.0);
        nl_buffer.remove(addr);
    }
}

void STRIDE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    int index = stride_buffer.search(addr);
    if (index != -1)
    {
        stride_buffer.pf_use++;
        stride_buffer.accuracy = (stride_buffer.pf_use * 1.0) / (stride_buffer.pf_count * 1.0);
        stride_buffer.remove(addr);
    }

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

            stride_buffer.insert(pf_address);
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

void DISTANCE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    int index = distance_buffer.search(addr);
    if (index != -1)
    {
        distance_buffer.pf_use++;
        distance_buffer.accuracy = (distance_buffer.pf_use * 1.0) / (distance_buffer.pf_count * 1.0);
        distance_buffer.remove(addr);
    }

    uint64_t cl_address = addr >> LOG2_BLOCK_SIZE;
    uint64_t page = cl_address >> LOG2_BLOCK_SIZE;

    int distance;
    int index;
    int dindex;

    if (!fr)
    {
        index = -1;
        dindex = -1;

        //Calculate this distance
        distance = cl_address - previous_addr;
        if (distance == 0)
        {
            return;
        }

        //Find a slot in the previous delta's entry for this distance
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

        uint64_t pf_addr;

        for (int i = 0; i < DISTANCE_COUNT; i++)
        {
            if (dtables[index].distances[i] != 0)
            {
                pf_addr = (cl_address + dtables[index].distances[i]) << LOG2_BLOCK_SIZE;
                distance_buffer.insert(pf_addr);
            }
        }

        previous_addr = cl_address;
        previous_distance = distance;
        previous_index = index;

        for (int i = 0; i < TABLE_COUNT; i++)
        {
            if (dtables[i].lru < dtables[index].lru)
            {
                dtables[i].lru++;
            }
        }
        dtables[index].lru = 0;
    }
    else
    {
        fr = 0;
        previous_addr = cl_address;
    }
}

void PFBUFFER::insert(uint64_t addr)
{
    int index;
    for (index = 0; index < BUFFER_SIZE; index++)
    {
        if (this->entry[index].lru == (BUFFER_SIZE - 1))
        {
            break;
        }

        this->entry[index].pf_addr = addr;
        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            if (this->entry[i].lru < this->entry[index].lru)
            {
                this->entry[i].lru++;
            }
        }
        this->entry[index].lru = 0;
    }
}

void PFBUFFER::remove(uint64_t addr)
{
    int index = -1;
    for (index = 0; index < BUFFER_SIZE; index++)
    {
        if (this->entry[index].pf_addr == addr)
        {
            break;
        }
    }

    this->entry[index].pf_addr = 0;
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (this->entry[i].lru > this->entry[index].lru)
        {
            this->entry[i].lru--;
        }
    }

    this->entry[index].lru = BUFFER_SIZE - 1;
}

void PFBUFFER::empty()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        this->entry[i].pf_addr = 0;
    }
}

int PFBUFFER::search(uint64_t addr)
{
    int index = -1;
    for (index = 0; index < BUFFER_SIZE; index++)
    {
        if (this->entry[index].pf_addr == addr)
        {
            return index;
        }
    }
    return -1;
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

void CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
    uint64_t cl_address = addr >> LOG2_BLOCK_SIZE;
    uint64_t evict_addr = addr >> LOG2_BLOCK_SIZE;
    CACHELINE::insert(addr, prefetch);
    if (prefetch == 1)
    {
        pf_count++;
    }
    //int index = cache_search(evict_addr);
    CACHELINE::remove(evicted_addr);
}

void CACHE::l2c_prefetcher_final_stats()
{
    float pf_accuracy = (pf_use * 1.0) / (pf_count * 1.0);
    cout << "\"Accuracy\": " << pf_accuracy << endl;
}
