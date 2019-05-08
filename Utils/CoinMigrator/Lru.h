#include <unordered_map>
#include <list>
#include "assert.h"

using namespace std;

template<class T>
class Lru
{
    public:
        Lru(int maxlen);
        int Length();
        bool IsInLru(uint64_t address);
        T* GetReferBy(uint64_t address);
        unsigned int GetAC(uint64_t address);
        void IncreaseAC(uint64_t address);
        bool InsertNode(T node, T& evicted_node);
        void Show();
        virtual ~Lru();

    protected:

    private:
        int maxlen;
        unordered_map<uint64_t, typename list<T>::iterator> lru_map;
        list<T> lru_list;
};

template<class T>
Lru<T>::Lru(int maxlen)
{
    //ctor
    this->maxlen = maxlen;
    lru_list.clear();
    lru_map.clear();
}

template<class T>
int Lru<T>::Length()
{
    return lru_list.size();
}

template<class T>
bool Lru<T>::IsInLru(uint64_t address)
{
    return lru_map.find(address) != lru_map.end();
}

template<class T>
T* Lru<T>::GetReferBy(uint64_t address)
{
    if(lru_map.find(address) == lru_map.end())
    {
        return NULL;
    }
    return &(*lru_map[address]);
}

template<class T>
unsigned int Lru<T>::GetAC(uint64_t address)
{
    assert(lru_map.find(address) != lru_map.end());
    return lru_map[address]->AC;
}

template<class T>
void Lru<T>::IncreaseAC(uint64_t address)
{
    assert(lru_map.find(address) != lru_map.end());

    lru_map[address]->AC += 1;
}

template<class T>
bool Lru<T>::InsertNode(T node, T& evicted_node)
{
    typename list<T>::iterator iter = lru_list.end();
    bool has_eviction = false;
    if(lru_map.find(node.address) != lru_map.end())
    {
        iter = lru_map[node.address];
        T tmp_iter = *iter;

        lru_list.erase(iter);
        lru_list.push_front(tmp_iter);
	    lru_map[node.address] = lru_list.begin();
    }
    else
    {
        if(lru_list.size() == maxlen)
        {
            has_eviction = true;
            evicted_node = lru_list.back();
            lru_map.erase(evicted_node.address);
            lru_list.pop_back();
        }
        lru_list.push_front(node);
        lru_map[node.address] = lru_list.begin();
    }
    return has_eviction;
}

template<class T>
void Lru<T>::Show()
{
    for(auto iter = lru_list.begin(); iter != lru_list.end(); ++iter)
        std::cout<<iter->address<<" ";
    std::cout<<std::endl;
}

template<class T>
Lru<T>::~Lru()
{
    //dtor
    lru_list.clear();
    lru_map.clear();
}
