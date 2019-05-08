#include "VictimPageList.h"
#include <assert.h>

VictimPageList::VictimPageList(uint64_t memory_size)
{
    uint64_t addr;
    //ctor
    for(uint64_t i = memory_size; i > 0; --i)
    {
	addr = (i - 1)<<12;
        page_list.push_front(addr);
        table_map[addr] = page_list.begin();
    }
}

void VictimPageList::RemovePage(uint64_t page_number)
{
    if(table_map.find(page_number) == table_map.end())
        return;
//    std::cout<<"pass............"<<std::endl;
    list<uint64_t>::iterator iter = table_map[page_number];

 //   std::cout<<"iter............"<<std::endl;
    page_list.erase(iter);

//    std::cout<<"erase_iter............"<<std::endl;
    table_map.erase(page_number);

//    std::cout<<"erase_page_number............"<<std::endl;
}

bool VictimPageList::IsFree(uint64_t page_number)
{
    return table_map.find(page_number) != table_map.end();
}

uint64_t VictimPageList::UsePage()
{
    assert(page_list.size() != 0);
    unsigned int victim_page = page_list.front();
    table_map.erase(victim_page);
    page_list.pop_front();
    page_list.push_back(victim_page);
    list<uint64_t>::iterator last = page_list.end();
    table_map[victim_page] = --last;
    return victim_page;
}

void VictimPageList::GarbageCollection(uint64_t page_number)
{
    assert(table_map.find(page_number) == table_map.end());
    page_list.push_back(page_number);
    list<uint64_t>::iterator last = page_list.end();
    table_map[page_number] = --last;
}

int VictimPageList::Size()
{
    return page_list.size();
}

void VictimPageList::Show()
{
    for(auto iter = page_list.begin(); iter != page_list.end(); ++iter)
    {
        std::cout<<*iter<<"  ";
    }
    cout<<std::endl;
}

VictimPageList::~VictimPageList()
{
    page_list.clear();
    table_map.clear();
}
