#include "Lru.h"
#include<iostream>
using namespace std;

LruMem::LruMem(uint64_t maxlen)
{
    //ctor
    this->maxlen = maxlen;
    len = 0;
    lru_list.clear();
    lru_map.clear();
}

uint64_t LruMem::Length()
{
    return len;
}

bool LruMem::IsInLru(uint64_t address)
{
    return lru_map.find(address) != lru_map.end();
}


uint64_t LruMem::InsertNode(uint64_t address)
{
    list<uint64_t>::iterator iter = lru_list.end();
    uint64_t tmp = -1;
    if(lru_map.find(address) != lru_map.end())
    {
        iter = lru_map[address];
        lru_list.erase(iter);
        //iter->AC += 1;
        lru_list.push_front(*iter);
        lru_map[address] = lru_list.begin();
    }
    else
    {
        if(lru_list.size() == maxlen)
        {

            tmp = lru_list.back();
            lru_map.erase(tmp);
            lru_list.pop_back();
            --len;
        }
        lru_list.push_front(address);
        lru_map[address] = lru_list.begin();
        ++len;
    }
    return tmp;
}

uint64_t LruMem::TakeOut()
{
    uint64_t tmp;
    assert(lru_list.size() > 0);
    tmp = lru_list.back();
    lru_map.erase(tmp);
    lru_list.pop_back();
    --len;
    return tmp;
}

void LruMem::show()
{
    for(auto iter = lru_list.begin(); iter != lru_list.end(); ++iter)
    {
        cout<<*iter<< " ";
    }
    cout<<endl;
}

LruMem::~LruMem()
{
    //dtor
    lru_list.clear();
    lru_map.clear();
    len = 0;
}
