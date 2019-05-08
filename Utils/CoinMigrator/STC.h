#include <cmath>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Lru.h"

template<class T>
class STC
{
    public:
        STC(uint64_t capacity, int way);
        bool IsInSTC(uint64_t address);
        T* GetReferBy(uint64_t address);
        unsigned int GetAC(uint64_t address);
        void IncreaseAC(uint64_t address);
        bool InsertNode(T node, T& evicted_node);
        void Show();
        virtual ~STC();

        protected:

    private:
        int way;
        vector<Lru<T>*> stc_cache;
        uint64_t capacity;
        int sets;
        uint64_t setBits;
};


template<class T>
STC<T>::STC(uint64_t capacity, int way) //parameter: {capacity: pageCounts}
{
    //ctor
    this->way = way;
    this->capacity = capacity;
    assert((capacity % way) == 0);
    sets = capacity / way;
    setBits = ceil(log((double)sets)/log(2.0));
//    std::cout<<setBits<<std::endl;
    // assert(sizeof(unsigned int) == 4);
    assert(setBits < (64 - 12));
    for(int i = 0; i < sets; ++i)
    {
        Lru<T>* p_num = new Lru<T>(way);
        stc_cache.push_back(p_num);
    }
}

template<class T>
bool STC<T>::IsInSTC(uint64_t address)
{
   // int cur_set = ((1 << setBits) - 1) & address;
   int cur_set = ((1 << setBits) - 1) & (address >> 12);
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->IsInLru(address);
}

template<class T>
T* STC<T>::GetReferBy(uint64_t address)
{
    int cur_set = ((1 << setBits) - 1) & (address >> 12);
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->GetReferBy(address);
}

template<class T>
unsigned int STC<T>::GetAC(uint64_t address)
{

    //int cur_set = ((1 << setBits) - 1) & address;
    int cur_set = ((1 << setBits) - 1) & (address >> 12);
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->GetAC(address);
}

template<class T>
void STC<T>::IncreaseAC(uint64_t address)
{
    int cur_set = ((1 << setBits) - 1) & (address >> 12);
    //int cur_set = ((1 << setBits) - 1) & address;
    assert(cur_set >= 0 && cur_set < sets);
    stc_cache[cur_set]->IncreaseAC(address);
}

template<class T>
bool STC<T>::InsertNode(T node, T& evicted_node)   //parameter: {node: {node.address: pageNumber}}
{

    // assert(sizeof(unsigned int) == 4);
    //int cur_set = ((1 << setBits) - 1) & node.address;
    int cur_set = ((1 << setBits) - 1) & (node.address >> 12);
    assert(cur_set >= 0 && cur_set < sets);
    return stc_cache[cur_set]->InsertNode(node, evicted_node);
}

template<class T>
void STC<T>::Show()
{
    //int cur_set = ((1 << setBits) - 1) & (node.address >> 12);
    for(int i = 0; i < sets; ++i)
    {
        stc_cache[i]->Show();
        cout<<endl;
    }
}

template<class T>
STC<T>::~STC()
{
    for(int i = 0; i < sets; ++i)
    {
        delete stc_cache[i];
    }
    stc_cache.clear();
}
