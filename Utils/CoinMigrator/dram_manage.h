#ifndef DRAM_MANAGE_H
#define DRAM_MANAGE_H

#include "Lru.h"
#include "VictimPageList.h"

#define MAX_CPU_CNT 10

enum Stage
{
    FREE_COMPETITION = 0,
    REALLOC
};


class dram_manager
{
    public:
        dram_manager(int, uint64_t);
        uint64_t use_page(int);
        bool insert_to(int to_whom, uint64_t what);
        void setting_cpus_page_lim(uint64_t cpus_page_lim[], int cpu_cnt);
        void change_stage();
        void show();
        virtual ~dram_manager();
    protected:
    private:
        int blank_page_cnt;
        int cpu_cnt;
        Stage stage;
        VictimPageList* leisure_page_list;
        LruMem* cpus_lru[MAX_CPU_CNT];
        uint64_t cpus_page_lim[MAX_CPU_CNT];
};

#endif // DRAM_MANAGE_H
