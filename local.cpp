#include "local.h"

//Stride-----------------------------------------------------------------------

STRIDE::Ip_tracker_t trackers[IP_TRACKER_COUNT];

//Distance---------------------------------------------------------------------

DELTA::IPENTRY ip_del[IP_COUNT];
uint32_t num_entries;

//Cache tracker----------------------------------------------------------------

CACHELINE cache[4096];
int cache_size;
int misses;
int acc_count;
int pf_count;
int pf_use;

PFBUFFER pf_buf;

uint32_t timer;
uint8_t trial_p;
uint32_t prefetcher;

int pf_issued;

void CACHE::l2c_prefetcher_initialize()
{
    cout << "\"Name\": "
         << "\"Local Pref\"," << endl;
    STRIDE::initialize();
    DELTA::initialize();
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

void DELTA::initialize()
{
    for (int i = 0; i < IP_COUNT; i++)
    {
        ip_del[i].score = i * -1;
    }
}

void cull()
{
    for (int i = 0; i < IP_COUNT; i++)
    {
        if (ip_del[i].score < 2)
        {
            ip_del[i].reset();
        }
    }

    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        if (trackers[i].confidence < 3)
        {
            trackers[i].ip = 0;
            trackers[i].last_cl_addr = 0;
            trackers[i].last_stride = 0;
            trackers[i].confidence = 0;
        }
    }
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

    if (timer < INTERVAL)
    {
        timer++;

        pf_issued = 0;

        STRIDE::operate(addr, ip, cache_hit, type);
        DELTA::operate(addr, ip, cache_hit, type);

        if (!pf_issued)
        {
            NEXTLINE::operate(addr, ip, cache_hit, type);
        }
    }
    else
    {
        timer = 0;
        cull();
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        uint64_t pf_addr = pf_buf.entry[i].pf_addr;
        if (MSHR.occupancy < (MSHR.SIZE >> 1))
        {
            prefetch_line(ip, addr, pf_addr, FILL_L2);
        }
        else
        {
            prefetch_line(ip, addr, pf_addr, FILL_LLC);
        }

        pf_buf.remove(pf_addr);
    }
}

void NEXTLINE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    for (int i = 0; i < N_DEGREE + 1; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;

        int index = pf_buf.search(pf_addr);
        if (index == -1)
        {
            pf_buf.insert(pf_addr);
        }
    }
}

void STRIDE::operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
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
                {
                    break;
                }

                //Check if this address is already in the buffer
                int ind = pf_buf.search(pf_address);
                if (ind == -1)
                {
                    pf_issued = 1;
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

    //If this ip isn't in the table, replace the one with the lowest score
    if (index == IP_COUNT)
    {
        int32_t min = 1E06;
        int min_index = 0;

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
        ip_del[index].tail = 0;

        //Empty the delta table
        for (int i = 0; i < DELTA_COUNT; i++)
        {
            ip_del[index].deltas[i] = 0;
        }

        return;
    }

    if (index == -1)
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
        int64_t delta;

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
        uint8_t match = 0;

        while ((index % DELTA_COUNT) != tail)
        {
            //Search for the delta pair in the buffer
            if (v1 == deltas[index % DELTA_COUNT] && v2 == deltas[(index - 1) % DELTA_COUNT])
            {
                prefetch(index + 1);
                pf_issued = 1;
                match++;
                break;
            }
            index--;
        }

        score = (match > 0) ? score + 1 : 0;
        tail = (tail < DELTA_COUNT - 1) ? tail + 1 : 0;
    }
}

void DELTA::IPENTRY::prefetch(int index)
{
    //Get the first prefetch address
    uint64_t pf_addr = (deltas[index % DELTA_COUNT] + previous_addr);
    int ind = pf_buf.search(pf_addr << LOG2_BLOCK_SIZE);

    if (ind == -1)
    {
        pf_buf.insert(pf_addr << LOG2_BLOCK_SIZE);
    }
    //Prefetch the following addresses by adding the deltas
    for (int i = index + 1; i < tail - 1; i++)
    {
        pf_addr += deltas[i % DELTA_COUNT];

        ind = pf_buf.search(pf_addr << LOG2_BLOCK_SIZE);
        if (ind == -1)
        {
            pf_buf.insert(pf_addr << LOG2_BLOCK_SIZE);
        }
    }
}

void DELTA::IPENTRY::reset()
{
    ip = 0;
    previous_addr = 0;
    tail = 0;
    score = 0;
    for (int i = 0; i < DELTA_COUNT; i++)
    {
        deltas[i] = 0;
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