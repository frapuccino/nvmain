#ifndef PROFESSSYSTEM_H
#define PROFESSSYSTEM_H
#include "STC.h"
#include <map>
#include "STentry.h"
#include "VictimPageList.h"

class ProfessSystem
{
    public:
        ProfessSystem(int stc_size, int stc_ways, int fast_memory_size, int slow_memory_size, int interval, int minbenefit);
        unsigned int GetPageNumber(unsigned int phy_addr);
        int MemoryRegion(unsigned int address);
        unsigned char GetQACV(int access_counts);
        STentry *Access(unsigned int phy_addr);

        virtual ~ProfessSystem();
    protected:
    private:
        int fast_memory_size;
        int slow_memory_size;
        int interval;
        STC* stc_cache;
        int accum_cnt[4];
        int num_q_sumI[4];
        int num_q[4][4];
        int num_q_sumE[4];
        int tmp_accum_cnt[4];
        int tmp_num_q_sumI[4];
        int tmp_num_q[4][4];
        int tmp_num_q_sumE[4];
        int promoStart;
        int minbenefit;
        int cur_epoch;
        unordered_map<unsigned int, STentry> remap_table;
        bool isStart;
        VictimPageList* victim_page_list;
};

#endif // PROFESSSYSTEM_H
