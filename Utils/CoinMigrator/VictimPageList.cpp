#include "VictimPageList.h"
#include <assert.h>

VictimPageList::VictimPageList(uint64_t memory_size)
{
    //ctor
    for(int i = memory_size - 1; i >= 0; --i)
    {
        page_list.push_front(i);
        table_map[i] = page_list.begin();
    }
}

void VictimPageList::RemovePage(uint64_t page_number)
{
    if(table_map.find(page_number) == table_map.end())
        return;
    list<uint64_t>::iterator iter = table_map[page_number];
    page_list.erase(iter);
    table_map.erase(page_number);
}

uint64_t VictimPageList::UsePage()
{
    assert(page_list.size() != 0);
    unsigned int victim_page = page_list.front();
    page_list.pop_front();
    table_map.erase(victim_page);
    return victim_page;
}

void VictimPageList::GarbageCollection(uint64_t page_number)
{
   // assert(table_map.find(page_number) == table_map.end());
    page_list.push_back(page_number);
    list<uint64_t>::iterator last = page_list.end();
    table_map[page_number] = --last;
}

int VictimPageList::Size()
{
    return page_list.size();
}
VictimPageList::~VictimPageList()
{
    page_list.clear();
    table_map.clear();
}
