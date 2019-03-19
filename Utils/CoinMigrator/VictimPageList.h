#ifndef VICTIMPAGELIST_H
#define VICTIMPAGELIST_H
#include <list>
#include <unordered_map>
using namespace std;

class VictimPageList
{
    public:
        VictimPageList(uint64_t memory_size); //pageCounts;
        void RemovePage(uint64_t page_number);
        uint64_t UsePage();
        int Size();
        void GarbageCollection(uint64_t page_number);
        virtual ~VictimPageList();
    protected:
    private:
        list<uint64_t> page_list;
        unordered_map<uint64_t, list<uint64_t>::iterator> table_map;
};

#endif // VICTIMPAGELIST_H
