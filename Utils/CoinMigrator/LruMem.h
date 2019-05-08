#ifndef LRU_H
#define LRU_H
#include <unordered_map>
#include <list>

#define NOT_FOUND ((uint64_t)(-1))
using namespace std;

class LruMem
{
    public:
        LruMem(uint64_t maxlen);
        uint64_t Length();
        bool IsInLru(uint64_t address);
        uint64_t InsertNode(uint64_t);
        uint64_t TakeOut();
        void show();
        virtual ~LruMem();

    protected:

    private:
    uint64_t len;
    uint64_t maxlen;
    unordered_map<uint64_t, list<uint64_t>::iterator> lru_map;
    list<uint64_t> lru_list;
};

#endif // LRU_H
