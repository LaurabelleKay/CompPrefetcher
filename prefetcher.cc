#include "prefetcher.h"

//Stride-----------------------------------------------------------------------

STRIDE::Ip_tracker_t trackers[IP_TRACKER_COUNT];

//Distance---------------------------------------------------------------------

DISTANCE::IPENTRY itables[IP_COUNT];
int fr;
int previous_distance;
int previous_index;
int previous_addr;
unsigned long long int previous_page;

DELTA::IPENTRY ip_del[IP_COUNT];

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
PFBUFFER pf_buf;

uint32_t timer;
uint8_t trial_p;
uint32_t prefetcher;

void CACHE::l2c_prefetcher_initialize()
{
    cout << "\"Name\": "
         << "\"Local Pref\"," << endl;
    STRIDE::initialize();
    DISTANCE::initialize();
    trial_p = 1;
    timer = 0;
}

void STRIDE::initialize()
{
    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        trackers[i].ip = 0;
        trackers[i].last_cl_addr = 0;
        trackers[i].last_stride = 0;
        trackers[i].confidence = -i;
    }
}

void DISTANCE::initialize()
{
    /*int i, j;
    for (i = 0; i < TABLE_COUNT; i++)
    {
        dtables[i].tag = 0;
        dtables[i].lru = i;
        for (j = 0; j < DISTANCE_COUNT; j++)
        {
            dtables[i].distances[j] = 0;
        }
    }*/
    fr = 1;
}

void CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
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

    //Always check the stride table first
    if (!trial_p)
    {
        index = STRIDE::search(ip);
        if (index != -1)
        {
            STRIDE::operate(addr, ip, cache_hit, type);
        }
        else
        {
            //! Distance has no metric
            //check if it's been trained by the distance pref
            index = DISTANCE::search(ip);
            if (index != -1)
            {
                DISTANCE::operate(addr, ip, cache_hit, type);
            }
            else
            {
                //Resort to next line
                NEXTLINE::operate(addr, ip, cache_hit, type);
            }
        }

        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            prefetch_line(ip, addr, pf_buf.entry[i].pf_addr, FILL_L2);
            pf_buf.remove(pf_buf.entry[i].pf_addr);
        }
    }
    else
    {
        timer++;
        STRIDE::operate(addr, ip, cache_hit, type);
        DISTANCE::operate(addr, ip, cache_hit, type);
        NEXTLINE::operate(addr, ip, cache_hit, type);
        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            if (MSHR.occupancy < (MSHR.SIZE >> 1))
            {
                prefetch_line(ip, addr, pf_buf.entry[i].pf_addr, FILL_L2);
            }
            else
            {
                prefetch_line(ip, addr, pf_buf.entry[i].pf_addr, FILL_LLC);
            }
            pf_buf.remove(pf_buf.entry[i].pf_addr);
        }
        if (timer == TRIAL_PERIOD)
        {
            trial_p = 0;
        }
    }
}

void NEXTLINE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    for (int i = 0; i < N_DEGREE + 1; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
        pf_buf.insert(pf_addr);
    }
}

void STRIDE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    ////int index = stride_buffer.search(addr);
    ////if (index != -1)
    ////{
    ////stride_buffer.pf_use++;
    ////stride_buffer.accuracy = (stride_buffer.pf_use * 1.0) / (stride_buffer.pf_count * 1.0);
    ////stride_buffer.remove(addr);
    ////}

    // check for a tracker hit
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int index = -1;
    for (index = 0; index < IP_TRACKER_COUNT; index++)
    {
        if (trackers[index].ip == ip)
            break;
    }

    // this is a new IP that doesn't have a tracker yet, so allocate one
    int min = 1E06;
    int min_index = 0;

    if (index == IP_TRACKER_COUNT)
    {
        for (index = 0; index < IP_TRACKER_COUNT; index++)
        {
            if (min > trackers[index].confidence)
            {
                min_index = index;
                min = trackers[index].confidence;
            }
        }

        //Replace the entry with the lowest confidence
        index = min_index;

        trackers[index].ip = ip;
        trackers[index].last_cl_addr = cl_addr;
        trackers[index].last_stride = 0;
        trackers[index].confidence = 0;
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
        //Increase the confidence if we've seen this stride before
        trackers[index].confidence++;
        // do some prefetching
        if (trackers[index].confidence > CONFIDENCE_THRESHOLD)
        {
            for (int i = 0; i < PREFETCH_DEGREE; i++)
            {
                uint64_t pf_address = (cl_addr + (stride * (i + 1))) << LOG2_BLOCK_SIZE;

                // only issue a prefetch if the prefetch address is in the same 4 KB page
                // as the current demand access address
                if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                    break;

                if (!trial_p) //TODO: get rid of trial period
                {
                    pf_buf.insert(pf_address);
                }
            }
        }
    }
    else
    {
        //Reset our confidence if this is a new stride
        trackers[index].confidence = 0;
    }

    trackers[index].last_cl_addr = cl_addr;
    trackers[index].last_stride = stride;

    ////for (int i = 0; i < IP_TRACKER_COUNT; i++)
    ////{
    ////if (trackers[i].lru < trackers[index].lru)
    ////trackers[i].lru++;
    ////}
    ////trackers[index].lru = 0;
}

void DELTA::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int index = -1;

    //Search for this ip in our table
    for (index = 0; index < IP_COUNT; index++)
    {
        if (ip_del[index].ip == ip)
        {
            break;
        }
    }

    int min = 1E06;
    int min_index = 0;

    //If this ip isn't in the table, replace the one with the lowest score
    if (index == IP_COUNT)
    {
        for (index = 0; index < IP_COUNT; index++)
        {
            if (min > ip_del[index].score)
            {
                min_index = index;
                min = ip_del[index].score;
            }
        }

        index = min_index;
        ip_del[index].ip = ip;
        ip_del[index].previous_addr = cl_addr;
        ip_del[index].score = 0;

        //Empty the delta table
        for (int i = 0; i < DELTA_COUNT; i++)
        {
            ip_del[index].deltas[i] = 0;
        }

        return;
    }

    if(index == -1)
    {
        assert(0);
    }

    //Perfom prefetching for this ip
    ip_del[index].operate(addr, ip, cache_hit, type);
}

void DELTA::IPENTRY::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    if (!cache_hit)
    {
        uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
        int64_t delta = 0;

        if (cl_addr > previous_addr)
        {
            delta = cl_addr - previous_addr;
        }
        else
        {
            delta = previous_addr - cl_addr;
            delta *= -1;
        }

        if (delta == 0)
        {
            return;
        }

        //Replace the tail (oldest entry)
        deltas[tail] = delta;
        previous_addr = cl_addr;

        int t = tail + DELTA_COUNT; //Allows for circular traversal of delta buffer

        //Get the last delta pair
        int v1 = deltas[t % DELTA_COUNT];
        int v2 = deltas[(t - 1) % DELTA_COUNT];

        int index = t - 2;

        while((index % DELTA_COUNT) != tail)
        {
            //Search for the delta pair in the buffer
            if(v1 == deltas[index % DELTA_COUNT] && v2 == deltas[(index - 1) % DELTA_COUNT]) //TODO: Update score
            {
                prefetch(index + 1);
            }
            index --;
        }
    }
}

void DELTA::IPENTRY::prefetch(int index)
{
    //Get the first prefetch address
    uint64_t pf_addr = (deltas[index % DELTA_COUNT] + previous_addr) >> LOG2_BLOCK_SIZE;
    pf_buf.insert(pf_addr);

    //Prefetch the following addresses by adding the deltas
    for(int i = index + 1; i < tail - 1; i++)
    {
        pf_addr += deltas[i % DELTA_COUNT];
        pf_buf.insert(pf_addr);
    }
}

void DISTANCE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    ////int index = distance_buffer.search(addr);
    ////if (index != -1)
    ////{
    ////distance_buffer.pf_use++;
    ////distance_buffer.accuracy = (distance_buffer.pf_use * 1.0) / (distance_buffer.pf_count * 1.0);
    ////distance_buffer.remove(addr);
    ////}

    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int distance;
    ////int dindex;

    int index = -1;
    for (index = 0; index < IP_COUNT; index++)
    {
        if (itables[index].ip == ip)
        {
            break;
        }
    }

    //If this ip is not in our table
    if (index == IP_COUNT)
    {
        for (index = 0; index < IP_COUNT; index++)
        {
            if (itables[index].lru == (IP_COUNT - 1))
            {
                break;
            }
        }

        itables[index].ip = ip;
        itables[index].base_addr = cl_addr;
        itables[index].previous_addr = cl_addr;
        //itables[index].previous_index = index;

        for (int i = 0; i < IP_COUNT; i++)
        {
            if (itables[i].lru < itables[index].lru)
            {
                itables[i].lru++;
            }
        }
        itables[index].lru = 0;

        return;
    }

    if (index == -1)
    {
        assert(0);
    }

    itables[index].operate(addr, ip, cache_hit, type);

    itables[index].previous_addr = cl_addr;
    for (int i = 0; i < IP_COUNT; i++)
    {
        if (itables[i].lru < itables[index].lru)
        {
            itables[i].lru++;
        }
    }
    itables[index].lru = 0;
}

void DISTANCE::IPENTRY::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    if (!cache_hit)
    {
        int64_t delta;
        uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

        if (cl_addr > this->previous_addr)
        {
            delta = cl_addr - this->previous_addr;
        }
        else
        {
            delta = this->previous_addr - cl_addr;
            delta *= -1;
        }

        int index = -1;
        uint64_t curr_addr = this->base_addr;

        //Check for this address in our table using the deltas;
        for (index = 0; index < DELTA_COUNT; index++)
        {
            curr_addr += this->deltas[index];
            if (curr_addr == cl_addr)
            {
                break;
            }
        }

        //? By checking if this was a cache hit we can determine if the pf was useful?
        //If we were able to find this address, issue the prefetch
        uint64_t pf_addr;
        if (index < DELTA_COUNT)
        {
            for (int i = index; i < DELTA_COUNT; i++)
            {
                pf_addr = (cl_addr + this->deltas[i]) << LOG2_BLOCK_SIZE;
                pf_buf.insert(pf_addr);
            }
        }

        if (delta == 0)
        {
            return;
        }

        int dindex = -1;
        for (dindex = 0; dindex < DELTA_COUNT; dindex++)
        {
            if (this->deltas[dindex] == 0)
            {
                break;
            }
        }

        //If the delta table is full
        if (dindex == DELTA_COUNT)
        {
            //Shuffle our deltas left
            this->base_addr += this->deltas[0];
            for (int i = 0; i < DELTA_COUNT - 1; i++)
            {
                this->deltas[i] = this->deltas[i + 1];
            }

            dindex = DELTA_COUNT - 1;
        }

        if (dindex == -1)
        {
            assert(0);
        }

        this->deltas[dindex] = delta;
    }
}

//? Potentially unneeded if stride and dist will track

void PFBUFFER::insert(uint64_t addr)
{
    int index;
    for (index = 0; index < BUFFER_SIZE; index++)
    {
        if (this->entry[index].lru == (BUFFER_SIZE - 1))
        {
            break;
        }
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

int STRIDE::search(uint64_t ip)
{
    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        if (trackers[i].ip == ip)
        {
            return i;
        }
    }
    return -1;
}

int DISTANCE::search(uint64_t ip)
{
    for (int i = 0; i < IP_COUNT; i++)
    {
        if (itables[i].ip == ip)
        {
            return i;
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
    CACHELINE::insert(addr, prefetch);
    if (prefetch == 1)
    {
        pf_count++;
    }

    CACHELINE::remove(evicted_addr);
}

void CACHE::l2c_prefetcher_final_stats()
{
    float pf_accuracy = (pf_use * 1.0) / (pf_count * 1.0);
    cout << "\"Accuracy\": " << pf_accuracy << endl;
}