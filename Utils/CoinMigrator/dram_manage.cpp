#include "dram_manage.h"

dram_manager::dram_manager(int cpu_cnt, uint64_t memory_size)
{
    //ctor
    leisure_page_list = new VictimPageList(memory_size);
    //assert(leisure_page_list != NULL);
    stage = FREE_COMPETITION;
    this->cpu_cnt = cpu_cnt;
    for(int i = 0; i < cpu_cnt; ++i)
    {
        cpus_lru[i] = new LruMem(memory_size);
        cpus_page_lim[i] = 0;
    }
}

uint64_t dram_manager::use_page(int who_use_page)
{
    if(leisure_page_list->GetSize() != 0)
    {
        return leisure_page_list->UsePage();
    }
    if(stage == FREE_COMPETITION)
    {
        if(cpus_lru[who_use_page]->Length() == 0)
        {
            return NOT_FOUND;
        }
        else
        {
            return cpus_lru[who_use_page]->TakeOut();
        }
    }
    else if(stage == REALLOC)
    {
        if(cpus_lru[who_use_page]->Length() >= cpus_page_lim[who_use_page])
        {
            cpus_lru[who_use_page]->TakeOut();
        }
        else
        {
            int wolves = 0;
            uint64_t deference = 0;
            for(int i = 0; i < cpu_cnt; ++i)
            {
                if((cpus_lru[i]->Length() > cpus_page_lim[i]) && (deference < cpus_lru[i]->Length() - cpus_page_lim[i]))
                {
                    wolves = i;
                    deference = cpus_lru[i]->Length() - cpus_page_lim[i];
                }
            }

            return cpus_lru[wolves]->TakeOut();
        }
    }
    return NOT_FOUND;
}

bool dram_manager::insert_to(int to_whom, uint64_t what)
{
    assert(0 <= to_whom && to_whom < cpu_cnt);
    cpus_lru[to_whom]->InsertNode(what);
}

void dram_manager::setting_cpus_page_lim(uint64_t cpus_page_lim[], int cpu_cnt)
{
    assert(cpu_cnt == this->cpu_cnt);
    for(int i = 0; i < cpu_cnt; ++i)
    {
        this->cpus_page_lim[i] = cpus_page_lim[i];
    }
}

void dram_manager::change_stage()
{
    stage = REALLOC;
}

void dram_manager::show()
{
    for(int i = 0; i < cpu_cnt; ++i)
    {
        cpus_lru[i]->show();
    }
}

dram_manager::~dram_manager()
{
    //dtor
    delete [] leisure_page_list;
    for(int i = 0; i < cpu_cnt; ++i)
    {
        delete [] cpus_lru[i];
    }
}
