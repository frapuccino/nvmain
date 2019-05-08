#include "VictimPageList.h"
#include <assert.h>

VictimPageList::VictimPageList(uint64_t memory_size)
{
    //ctor
    for(int i = memory_size - 1; i >= 0; --i)
    {
        page_list.push_front(i);
    }
    len = memory_size;
}


uint64_t VictimPageList::UsePage()
{
    assert(page_list.size() != 0);
    unsigned int victim_page = page_list.front();
    page_list.pop_front();
    len -= 1;
    return victim_page;

}

void VictimPageList::GarbageCollection(uint64_t page_number)
{
    page_list.push_back(page_number);
    len++;
}

uint64_t VictimPageList::GetSize()
{

    return len;
}

VictimPageList::~VictimPageList()
{
    page_list.clear();
    len = 0;
}
