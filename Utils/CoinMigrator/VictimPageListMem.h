#ifndef VICTIMPAGELIST_H
#define VICTIMPAGELIST_H

#include <list>
#include <unordered_map>
using namespace std;


class VictimPageList
{
    public:
        VictimPageList(uint64_t memory_size); //pageCounts;
        uint64_t UsePage();
        uint64_t GetSize();
        void GarbageCollection(uint64_t page_number);
        virtual ~VictimPageList();
    protected:
    private:
        list<uint64_t> page_list;
        uint64_t len;
        // unordered_map<uint64_t, list<uint64_t>::iterator> table_map;
};

#endif // VICTIMPAGELIST_H
