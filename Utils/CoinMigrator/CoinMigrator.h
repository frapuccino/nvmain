/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*******************************************************************************/

#ifndef __NVMAIN_UTILS_COINMIGRATOR_H__
#define __NVMAIN_UTILS_COINMIGRATOR_H__

#include "src/NVMObject.h"
#include "src/Params.h"
#include "include/NVMainRequest.h"

#include "VictimPageList.h"
#include "STC.h"
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <unordered_set>
#include "STC_entry.h"
#include "ST_entry.h"

#define THETA 68
#define MAX_FEATURE_CNT 10
#define PRED_TABLE_SIZE 1024
#define COST 9

namespace NVM {

#define MIG_READ_TAG GetTagGenerator( )->CreateTag("MIGREAD")
#define MIG_WRITE_TAG GetTagGenerator( )->CreateTag("MIGWRITE")

class Migrator;

class CoinMigrator : public NVMObject
{
  public:
    CoinMigrator( );
    ~CoinMigrator( );

    void Init( Config *config );

    bool IssueAtomic( NVMainRequest *request );
    bool IssueCommand( NVMainRequest *request );
    bool RequestComplete( NVMainRequest *request );

    unsigned char GetQACV(int access_counts);
    //void InsertToSTC(Node node);
    
    void UpdateAddrHistory(uint64_t address);
    void UpdateQACHistory(unsigned char my_QAC);
    unsigned char GetQACBy(unsigned int benefit);
    void GetFeatureIndex(uint64_t hash[], unsigned char my_QAC, uint64_t address, int feature_cnt);
    void Train_in_STC(uint64_t hash[], uint64_t address, bool writeOp, int feature_cnt);

    void Cycle( ncycle_t steps );

  private:
    bool promoBuffered, demoBuffered; 
    NVMAddress demotee, promotee; 
    NVMainRequest *promoRequest;
    NVMainRequest *demoRequest;

    int access_count;
    int hit_count;
    VictimPageList *victim_page_list;
    int minbenefit;
    int cur_epoch;
    STC<STC_entry>* stc_cache;
    int stc_size;
    int interval;
    int accum_cnt[4];
        int num_q_sumI[4];
        int num_q[4][4];
        int num_q_sumE[4];
        int tmp_accum_cnt[4];
        int tmp_num_q_sumI[4];
        int tmp_num_q[4][4];
        int tmp_num_q_sumE[4];
int a1;
int a2;
    unsigned int seed;
    double probability;
    ncounter_t numCols;
    bool queriedMemory;
    ncycle_t bufferReadLatency;
    Params *promotionChannelParams;
    ncounter_t totalPromotionPages;
    ncounter_t currentPromotionPage;
    ncounter_t promotionChannel;
    Migrator* migratorTranslator;

    ncounter_t migrationCount;
    ncounter_t queueWaits;
    ncounter_t bufferedReads;
    int stc_hit;
    int stc_access;
    double stc_hit_rate;

    int feature_cnt;
    signed char pred_table[MAX_FEATURE_CNT][PRED_TABLE_SIZE];
    uint64_t addr_history[3];
    unsigned char QAC_history[3];
    int Tbypass;

    ncounter_t dram_hit;
    ncounter_t memory_access;
    ncounter_t nvm_hit;
    ncounter_t nvm_read;
    ncounter_t nvm_write;

    std::unordered_set<uint64_t> uniq_checker;

    bool CheckIssuable( NVMAddress address, OpType type );
    bool TryMigration( NVMainRequest *request, bool atomic );
    void ChooseVictim( Migrator *at, NVMAddress& promo, NVMAddress& victim );
};

};

#endif

