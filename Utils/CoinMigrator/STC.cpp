#include "STC.h"
#include <cmath>
#include <iostream>
#include <assert.h>

STC::STC(uint64_t capacity, int way) //parameter: {capacity: pageCounts}
{
    //ctor
    this->way = way;
    this->capacity = capacity;
    assert((capacity % way) == 0);
    sets = capacity / way;
    setBits = ceil(log((double)sets)/log(2.0));
    // assert(sizeof(unsigned int) == 4);
    assert(setBits < (64 - 12));
    for(int i = 0; i < sets; ++i)
    {
        Lru* p_num = new Lru(way);
        stc_cache.push_back(p_num);
    }
}

bool STC::IsInSTC(uint64_t address)
{
    int cur_set = ((1 << setBits) - 1) & address;
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->IsInLru(address);
}

unsigned int STC::GetAC(uint64_t address)
{
    int cur_set = ((1 << setBits) - 1) & address;
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->GetAC(address);
}

void STC::IncreaseAC(uint64_t address)
{
    int cur_set = ((1 << setBits) - 1) & address;
    assert(cur_set >= 0 && cur_set < sets);
    stc_cache[cur_set]->IncreaseAC(address);
}

Node STC::InsertNode(Node node)   //parameter: {node: {node.address: pageNumber}}
{

    // assert(sizeof(unsigned int) == 4);
    int cur_set = ((1 << setBits) - 1) & node.address;
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->InsertNode(node);
}

STC::~STC()
{
    for(int i = 0; i < sets; ++i)
    {
        delete stc_cache[i];
    }
    stc_cache.clear();
}
