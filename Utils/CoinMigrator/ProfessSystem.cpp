#include "ProfessSystem.h"
#include "STentry.h"
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <assert.h>

ProfessSystem::ProfessSystem(int stc_size, int stc_ways, int fast_memory_size, int slow_memory_size, int interval, int minbenefit)   //pageCounts
{
    //ctor
    this->fast_memory_size = fast_memory_size;
    this->slow_memory_size = slow_memory_size;
    this->interval = interval;
    isStart = false;
    promoStart = 0;
    this->minbenefit = minbenefit;
    remap_table.clear();
    stc_cache = new STC(stc_size, stc_ways);
    victim_page_list = new VictimPageList(fast_memory_size);
}

inline unsigned int ProfessSystem::GetPageNumber(unsigned int phy_addr)
{
    return phy_addr >> 12;
}

int ProfessSystem::MemoryRegion(unsigned int address)
{
    if(address < 1024)
        return 0;
    else
        return 1;
}

unsigned char ProfessSystem::GetQACV(int access_counts)
{
    assert(access_counts >= 0);
    if(access_counts >= 8 && access_counts < 32)
    {
        return 2;
    }
    else if(access_counts >= 1 && access_counts < 8)
    {
        return 1;
    }
    else if(access_counts >= 32)
    {
        return 3;
    }
    else
    {
        return 0;
    }
}

STentry *ProfessSystem::Access(unsigned int phy_addr)
{
    unsigned int address = GetPageNumber(phy_addr);
    Node node;
    if(remap_table.find(address) == remap_table.end())
    {
        STentry stentry;
        stentry.real_address = address;
        stentry.QAC = 0;
        remap_table[address] = stentry;
        node.address = address;
        node.QAC = 0;
        node.AC = 1;
        if(MemoryRegion(address) == 0)
        {
            victim_page_list->RemovePage(address);
        }
    }
    else
    {
        if(!(stc_cache->IsInSTC(address)))
        {
            node.address = address;
            node.QAC = remap_table[address].QAC;
            node.AC = 1;
            if(MemoryRegion(address) == 0)
            {
                victim_page_list->RemovePage(address);
            }
        }
        else
        {
            node.address = address;
            unsigned int real_addr = remap_table[address].real_address;
            if(MemoryRegion(real_addr) == 1)
            {
                 double exp_cnt = 0;
                 int q_i = remap_table[address].QAC;
                 for(int i = 1; i < 4; ++i)
                 {
                     double avg_cnt = (double)accum_cnt[i] / (double)num_q_sumI[i];
                     double p_qe_qi = ((double)num_q[q_i][i] + 1) / (num_q_sumE[q_i] + 3);
                     exp_cnt += avg_cnt * p_qe_qi;
                 }
                 unsigned int ac = stc_cache->GetAC(address);
                 if(exp_cnt - ac > minbenefit)
                 {
                     unsigned int victim_page = victim_page_list->UsePage();
                     unsigned int tmp;
                     tmp = remap_table[address].real_address;
                     remap_table[address].real_address = victim_page;
                     remap_table[victim_page].real_address = tmp;
                 }
            }
        }
    }
    Node* evicted_node = stc_cache->InsertNode(node);
    if(evicted_node)
    {
        int qacv = GetQACV(evicted_node->AC);
        accum_cnt[qacv] += evicted_node -> AC;
        num_q_sumI[qacv] += 1;
        num_q[evicted_node->QAC][qacv] += 1;
        num_q_sumE[evicted_node->QAC] += 1;
        if(MemoryRegion(evicted_node -> address) == 0)
        {
            victim_page_list->GarbageCollection(evicted_node -> address);
        }

        remap_table[evicted_node -> address].QAC = qacv;
        delete evicted_node;
    }
 
    std::cout<<"------------->"<<victim_page_list->Size()<<std::endl;
}

ProfessSystem::~ProfessSystem()
{
    //dtor
    remap_table.clear();
    delete stc_cache;
    delete victim_page_list;
}
