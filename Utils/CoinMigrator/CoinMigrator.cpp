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

#include "Utils/CoinMigrator/CoinMigrator.h"
#include "Decoders/Migrator/Migrator.h"
#include "NVM/nvmain.h"
#include "src/SubArray.h"
#include "src/EventQueue.h"
#include "include/NVMHelpers.h"
#include <cstring>
#include <fstream>

using namespace NVM;

CoinMigrator::CoinMigrator( )
{
    /*
     *  We will eventually be injecting requests to perform migration, so we
     *  would like IssueCommand to be called on the original request first so
     *  that we do not unintentially fill up the transaction queue causing 
     *  the original request triggering migration to fail.
     */
    SetHookType( NVMHOOK_BOTHISSUE );

    promoRequest = NULL;
    demoRequest = NULL;
    promoBuffered = false;
    demoBuffered = false;

    migrationCount = 0;
    queueWaits = 0;
    bufferedReads = 0;
    
    stc_hit = 0;
    stc_access = 0;
    stc_hit_rate = 0;

    dram_hit = 0;
    nvm_hit = 0;
    memory_access = 0;
    nvm_read = 0;
    nvm_write = 0;


    minbenefit = 8;
    cur_epoch = 0;
    interval = 2000;

    queriedMemory = false;
    promotionChannelParams = NULL;
    currentPromotionPage = 0;
    //a1 = a2 = 0;
    
    access_count = hit_count = 0;

    memset(tmp_accum_cnt, 0, sizeof(tmp_accum_cnt));
    memset(tmp_num_q, 0, sizeof(tmp_num_q));
    memset(tmp_num_q_sumE, 0, sizeof(tmp_num_q_sumE));
    memset(tmp_num_q_sumI, 0, sizeof(tmp_num_q_sumI));
    memset(accum_cnt, 0, sizeof(accum_cnt));
    memset(num_q, 0, sizeof(num_q));
    memset(num_q_sumE, 0, sizeof(num_q_sumE));
    memset(num_q_sumI, 0, sizeof(num_q_sumI));

    feature_cnt = 8;
    QAC_history[2] = QAC_history[1] = QAC_history[0] = 0;
    addr_history[2] = addr_history[1] = addr_history[0] = 0;
}


CoinMigrator::~CoinMigrator( )
{

}


void CoinMigrator::Init( Config *config )
{
    /* 
     *  Our seed for migration probability. This should be a known constant if
     *  you wish to reproduce the same results each simulation.
     */
    seed = 1;

    /* Chance to migrate: 0 = 0%, 1.00 = 100%. */
    probability = 0.02; 
    config->GetEnergy( "CoinMigratorProbability", probability ); 

    /* Specifies with channel is the "fast" memory. */
    promotionChannel = 0;
    config->GetValueUL( "CoinMigratorPromotionChannel", promotionChannel );

    /* If we want to simulate additional latency serving buffered requests. */
    bufferReadLatency = 4;
    config->GetValueUL( "MigrationBufferReadLatency", bufferReadLatency );

    /* 
     *  We migrate entire rows between banks, so the column count needs to
     *  match across all channels for valid results.
     */
    numCols = config->GetValue( "COLS" );

    AddStat(migrationCount);
    AddStat(queueWaits);
    AddStat(bufferedReads);

    AddStat(stc_access);
    AddStat(stc_hit);
    AddStat(dram_hit);
    AddStat(memory_access);
    AddStat(nvm_hit);
    AddStat(nvm_read);
    AddStat(nvm_write);
/*
    NVMainRequest queryRequest;
    queryRequest.address.SetTranslatedAddress( 0, 0, 0, 0, promotionChannel, 0 );
    queryRequest.address.SetPhysicalAddress( 0 );
    queryRequest.type = READ;
    queryRequest.owner = this;
    NVMObject *curObject = NULL;
    FindModuleChildType( &queryRequest, SubArray, curObject, parent->GetTrampoline( ) );

    SubArray *promotionChannelSubarray = NULL;
    promotionChannelSubarray = dynamic_cast<SubArray *>( curObject );

    assert( promotionChannelSubarray != NULL );
    Params *p = promotionChannelSubarray->GetParams( );

    ncounter_t total = p->RANKS * p->BANKS * p->ROWS;
*/
    uint64_t ranks, banks, rows;
    ranks = 1;banks = 16; rows = 128;
    uint64_t total = ranks*banks*rows;
    int stc_size = 1024;
    stc_cache = new STC<STC_entry>(stc_size, 8);
    victim_page_list = new VictimPageList(total);
    uniq_checker.clear();
    ifstream ifs("/home/hx/gem5/pred_arg.txt");
    ifs >> Tbypass;
    ifs.close();
}


bool CoinMigrator::IssueAtomic( NVMainRequest *request )
{
    /* For atomic mode, we just swap the pages instantly. */
    //return TryMigration( request, true );

    return TryMigration( request, false );
}


bool CoinMigrator::IssueCommand( NVMainRequest *request )
{
    /* 
     *  In cycle-accurate mode, we must read each page, buffer it, enqueue a
     *  write request, and wait for write completion.
     */
   // return TryMigration( request, false );

    return TryMigration( request, true );
}


bool CoinMigrator::RequestComplete( NVMainRequest *request )
{
    if( NVMTypeMatches(NVMain) && GetCurrentHookType( ) == NVMHOOK_PREISSUE )
    {
        /* Ensure the Migrator translator is used. */
        Migrator *migratorTranslator = dynamic_cast<Migrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_READ_TAG )
        {
	    std::cout<<"set_mig1........."<<std::endl;
            /* A migration read completed, update state. */
            migratorTranslator->SetMigrationState( request->address, MIGRATION_BUFFERED ); 

	    std::cout<<"set_mig2........."<<std::endl;
            /* If both requests are buffered, we can attempt to write. */
            bool bufferComplete = false;

            if( (request == promoRequest 
                 && migratorTranslator->IsBuffered( demotee ))
                || (request == demoRequest
                 && migratorTranslator->IsBuffered( promotee )) )
            {
                bufferComplete = true;
            }

            /* Make a new request to issue for write. Parent will delete current pointer. */
            if( request == promoRequest )
            {
                promoRequest = new NVMainRequest( );
                *promoRequest = *request;
            }
            else if( request == demoRequest )
            {
                demoRequest = new NVMainRequest( );
                *demoRequest = *request;
            }
            else
            {
                assert( false );
            }

            /* Swap the address and set type to write. */
            if( bufferComplete )
            {
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
                NVMObject *savedParent = parent->GetTrampoline( );

                NVMAddress tempAddress = promoRequest->address;
                promoRequest->address = demoRequest->address;
                demoRequest->address = tempAddress;

                demoRequest->type = WRITE;
                promoRequest->type = WRITE;

                demoRequest->tag = MIG_WRITE_TAG;
                promoRequest->tag = MIG_WRITE_TAG;

                /* Try to issue these now, otherwise we can try later. */
                bool demoIssued, promoIssued;

                demoIssued = savedParent->GetChild( demoRequest )->IssueCommand( demoRequest );
                promoIssued = savedParent->GetChild( promoRequest )->IssueCommand( promoRequest );

                if( demoIssued )
                {
		    
	    	    std::cout<<"set_mig3........."<<std::endl;
                    migratorTranslator->SetMigrationState( demoRequest->address, MIGRATION_WRITING );
	   	    std::cout<<"set_mig4........."<<std::endl;
		    
                }
                if( promoIssued )
                {
	      	    std::cout<<"set_mig5........."<<std::endl;
		     
                    migratorTranslator->SetMigrationState( promoRequest->address, MIGRATION_WRITING );
		    std::cout<<"set_mig6........."<<std::endl;
                }

                promoBuffered = !promoIssued;
                demoBuffered = !demoIssued;
            }
        }
        /* A write completed. */
        else if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_WRITE_TAG )
        {
		  std::cout<<"set_mig7........."<<std::endl;
	
            // Note: request should be deleted by parent
            migratorTranslator->SetMigrationState( request->address, MIGRATION_DONE );
		 std::cout<<"set_mig8........."<<std::endl;


            migrationCount++;
        }
        /* Some other request completed, see if we can ninja issue some migration writes that did not queue. */
        else if( promoBuffered || demoBuffered )
        {
            bool demoIssued, promoIssued;

            if( promoBuffered )
            {
                promoIssued = parent->GetTrampoline( )->GetChild( promoRequest )->IssueCommand( promoRequest );
                promoBuffered = !promoIssued;
            }

            if( demoBuffered )
            {
                demoIssued = parent->GetTrampoline( )->GetChild( demoRequest )->IssueCommand( demoRequest );
                demoBuffered = !demoIssued;
            }
        }
    }

    return true;
}


bool CoinMigrator::CheckIssuable( NVMAddress address, OpType type )
{
    NVMainRequest request;

    request.address = address;
    request.type = type;

    return parent->GetTrampoline( )->GetChild( &request )->IsIssuable( &request );
}

unsigned char CoinMigrator::GetQACV(int access_counts)
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

/*
void CoinMigrator::InsertToSTC(Node node)
{
    ++stc_access;
//    std::cout<<"insert1......"<<std::endl;
    if(stc_cache->IsInSTC(node.address))
    {
        assert(uniq_checker.find(node.address) != uniq_checker.end());
        ++stc_hit;
    }
    else
    {
        assert(uniq_checker.find(node.address) == uniq_checker.end());
    }

    assert(migratorTranslator->IsMapped(node.address));
    STentry node_real_addr = migratorTranslator->GetEntryFromRemapTable(node.address);
    if(node_real_addr.channel == 0)
    {
        victim_page_list->RemovePage(node_real_addr.real_address);
    }

    //stc_hit_rate = (double)stc_hit / (double)stc_access;
    Node evicted_node;  
    bool ret = stc_cache->InsertNode(node, evicted_node);
    if(uniq_checker.find(node.address) == uniq_checker.end())
        uniq_checker.insert(node.address);
//    std::cout<<"insert2......"<<std::endl;
            if(ret)
            {
                uniq_checker.erase(evicted_node.address);
                
                assert(evicted_node.QAC <= 3);
  //              std::cout<<"insert3......"<<std::endl;
                int qacv = GetQACV(evicted_node.AC);
                tmp_accum_cnt[qacv] += evicted_node.AC;

                tmp_num_q_sumI[qacv] += 1;
                tmp_num_q[evicted_node.QAC][qacv] += 1;
                tmp_num_q_sumE[evicted_node.QAC] += 1;
                uint64_t row, col, bank, rank, channel, subarray;
                STentry tmp = migratorTranslator->GetEntryFromRemapTable(evicted_node.address);
                migratorTranslator->TranslateWithoutMig(tmp.real_address, &row, &col, &bank, &rank, &channel, &subarray);
                if(channel == 0)
                {
                    victim_page_list->GarbageCollection(tmp.real_address);
    //                std::cout<<"insert4......"<<std::endl;
                }
                STentry new_entry;
                new_entry.real_address = tmp.real_address;
                new_entry.QAC = qacv;
                new_entry.channel = tmp.channel;
                assert(migratorTranslator->IsMapped(evicted_node.address));
                migratorTranslator->Add2RemapTable(evicted_node.address, new_entry);
	    ++cur_epoch;
            if(cur_epoch == interval)
            {
                for(int i = 1; i < 4; ++i)
                {
                    accum_cnt[i] = tmp_accum_cnt[i];
                    num_q_sumI[i] = tmp_num_q_sumI[i];
                }
                for(int i = 0; i < 4; ++i)
                {
                    num_q_sumE[i] = tmp_num_q_sumE[i];
                    for(int j = 1; j < 4; ++j)
                    {
                        num_q[i][j] = tmp_num_q[i][j];
                    }
                }
                memset(tmp_accum_cnt, 0, sizeof(tmp_accum_cnt));
                memset(tmp_num_q, 0, sizeof(tmp_num_q));
                memset(tmp_num_q_sumE, 0, sizeof(tmp_num_q_sumE));
                memset(tmp_num_q_sumI, 0, sizeof(tmp_num_q_sumI));
                cur_epoch = 0;
            }
        }
}
*/

unsigned char CoinMigrator::GetQACBy(unsigned int benefit)
{
    if(benefit == 0)
    {
        return 0;
    }
    else if(benefit >= 1 && benefit < 10)
    {
        return 1;
    }
    else if(benefit  >= 10 && benefit < 16)
    {
        return 2;
    }
    else if(benefit >= 16 && benefit < 40)
    {
        return 3;
    }
    else
    {
        return 4;
    }
}

void CoinMigrator::UpdateAddrHistory(uint64_t address)
{
    addr_history[2] = addr_history[1];
    addr_history[1] = addr_history[0];
    addr_history[0] = address;
}
    
void CoinMigrator::UpdateQACHistory(unsigned char my_QAC)
{
    QAC_history[2] = QAC_history[1];
    QAC_history[1] = QAC_history[0];
    QAC_history[0] = my_QAC;
}

void CoinMigrator::GetFeatureIndex(uint64_t hash[], unsigned char my_QAC, uint64_t address, int feature_cnt)
{
    assert(feature_cnt < 9);
    uint64_t tag = (address >> 12);
    hash[0] = (tag ^ (tag >> 1)) % PRED_TABLE_SIZE;
    hash[1] = (tag ^ my_QAC) % PRED_TABLE_SIZE;
    hash[2] = (tag ^ (addr_history[0] >> 1)) % PRED_TABLE_SIZE;
    hash[3] = (tag ^ (addr_history[1] >> 2)) % PRED_TABLE_SIZE;
    hash[4] = (tag ^ (addr_history[2] >> 3)) % PRED_TABLE_SIZE;
    hash[5] = (tag ^ (QAC_history[0] >> 1)) % PRED_TABLE_SIZE;
    hash[6] = (tag ^ (QAC_history[1] >> 2)) % PRED_TABLE_SIZE;
    hash[7] = (tag ^ (QAC_history[2] >> 3)) % PRED_TABLE_SIZE;
}


void CoinMigrator::Train_in_STC(uint64_t hash[], uint64_t address, bool writeOp, int feature_cnt)
{
    ++stc_access;
    if(stc_cache->IsInSTC(address))
    {
        stc_hit++;
        STC_entry* entry = stc_cache->GetReferBy(address);
       // if(migratorTranslator->IsInDRAM(address))
        {
            if(writeOp)
            {
                entry->benefit += 8;
            }
            else
            {
                entry->benefit += 1;
            }
        }
        
        bool is_benefit = (entry->benefit > COST);
        if(entry->yout > -THETA && is_benefit)
        {
            for(int i = 0; i < feature_cnt; ++i)
            {
                pred_table[i][(entry->hash)[i]] = (pred_table[i][(entry->hash)[i]] > (-32) ? pred_table[i][(entry->hash)[i]] - 1 : -32);
            
            }
        }

        for(int i = 0; i < feature_cnt; ++i)
        {
            (entry->hash)[i] = hash[i];
        }

        int tmp_out = 0;
        for(int i = 0; i < feature_cnt; ++i)
        {
            tmp_out += (int)pred_table[i][hash[i]];
        }
        
        if(tmp_out > 255)
        {
            (entry->yout) = 255;
        }
        else if(tmp_out < (-256))
        {
            (entry->yout) = -256;
        }
        else
        {
            (entry->yout) = tmp_out;
        }
        STC_entry evicted_node;
        bool has_eviction = stc_cache->InsertNode(*entry, evicted_node);
        assert(has_eviction == false);
    }
    else
    {
        STC_entry insert_node, evicted_node;
        insert_node.address = address;
        insert_node.benefit = 0;

        bool has_eviction = stc_cache->InsertNode(insert_node, evicted_node);
        STC_entry* i_entry = stc_cache->GetReferBy(address);
        if(has_eviction)
        {
            //bool mispredict = (evicted_node.yout < Treplace);
            //bool mispredict = (evicted_node.benefit <= COST && migratorTranslator->IsInDRAM(address));
            if(evicted_node.yout < THETA || evicted_node.benefit <= COST)
            {
                for(int i = 0; i < feature_cnt; ++i)
                {
                    pred_table[i][(evicted_node.hash)[i]] = (pred_table[i][(evicted_node.hash)[i]] < 32 ? pred_table[i][(evicted_node.hash)[i]] + 1 : 32);
                }
            }
        }
        
        ST_entry stentry = migratorTranslator->GetEntryFromRemapTable(address);
        UpdateQACHistory(stentry.QAC);
        stentry.QAC = GetQACBy(evicted_node.benefit);
        migratorTranslator->Add2RemapTable(address, stentry);
        for(int i = 0; i < feature_cnt; ++i)
        {
            (i_entry->hash)[i] = hash[i];
        }

        int tmp_out = 0;
        for(int i = 0; i < feature_cnt; ++i)
        {
            tmp_out += (int)pred_table[i][hash[i]];
        }

        if(tmp_out > 255)
        {
            i_entry->yout = 255;
        }
        else if(tmp_out < (-256))
        {
            i_entry->yout = -256;
        }
        else
        {
            i_entry->yout = tmp_out;
        }
    }
}

bool CoinMigrator::TryMigration( NVMainRequest *request, bool atomic )
{
    //std::cout<<"start......"<<std::endl;
    bool rv = true;

    if( NVMTypeMatches(NVMain) )
    {
        /* Ensure the Migrator translator is used. */
        migratorTranslator = dynamic_cast<Migrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        /* Migrations in progress must be served from the buffers during migration. */
/*
        if( GetCurrentHookType( ) == NVMHOOK_PREISSUE && migratorTranslator->IsBuffered( request->address ) )
        {
*/
            /* Short circuit this request so it is not queued. */
 //           rv = false;

            /* Complete the request, adding some buffer read latency. */
/*
            GetEventQueue( )->InsertEvent( EventResponse, parent->GetTrampoline( ), request,
                              GetEventQueue()->GetCurrentCycle()+bufferReadLatency );

            bufferedReads++;

            return rv;
        }
*/
        /* Don't inject results before the original is issued to prevent deadlock */
        if( GetCurrentHookType( ) != NVMHOOK_POSTISSUE )
        {
            return rv;
        }



	/*gyx:for debug  output the physical address;*/
	    /*
	    std::ofstream output_mig;
	    output_mig.open("/home/hx/gem5/TraceMig.txt",std::ios::app);
	    output_mig<<request->threadId<<" "<<request->address.GetPhysicalAddress()<<" "<<request->type<<std::endl;
	 */




        /* See if any migration is possible (i.e., no migration is in progress) */
        bool migrationPossible = false;

        //if( !migratorTranslator->Migrating( ) )
            //&& !migratorTranslator->IsMigrated( request->address ) 
            //&& request->address.GetChannel( ) != promotionChannel )
        {
                migrationPossible = true;
        }

        bool writeOp = (request->type == WRITE);
        uint64_t row, bank, rank, channel, subarray, col;
        uint64_t promoteeAddress = request->address.GetPhysicalAddress();
        migratorTranslator->TranslateWithoutMig(promoteeAddress, &row, &col, &bank, &rank, &channel, &subarray);;
        promoteeAddress = migratorTranslator->ReverseTranslate( row, 0, bank, rank, channel, subarray );
        //std::cout<<"step1................"<<std::endl;
        if( migrationPossible )
        {
            //assert( !demoBuffered && !promoBuffered );

            
            /* Flip a biased coin to determine whether to migrate. */
            /*
            double coinToss = static_cast<double>(::rand_r(&seed)) 
                            / static_cast<double>(RAND_MAX);
            */
            /*
            if( coinToss <= probability )
            {
            */
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
	        access_count+= 1;
            //Node node;
            memory_access += 1;

            if(!migratorTranslator->IsMapped(promoteeAddress))
            {
                ST_entry stentry;
                stentry.real_address = promoteeAddress;
                stentry.QAC = 0;
                migratorTranslator->Add2RemapTable(promoteeAddress, stentry);
                if(channel == 0)
                {
                    migratorTranslator->Add2InvertedTable(promoteeAddress,promoteeAddress);
                }
            }

            if(migratorTranslator->IsInDRAM(promoteeAddress))
            {
                dram_hit++;
                uint64_t hash[8];
                ST_entry stentry = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                GetFeatureIndex(hash, stentry.QAC, promoteeAddress, feature_cnt);
                Train_in_STC(hash, promoteeAddress, writeOp, feature_cnt);
            }
            else
            {
                nvm_hit++;

                uint64_t hash[8];
                ST_entry stentry = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                GetFeatureIndex(hash, stentry.QAC, promoteeAddress, feature_cnt);
                int total_weight = 0;
                for(int i = 0; i < feature_cnt; ++i)
                {
                    total_weight += (int)pred_table[i][hash[i]];
                }
                bool need_migration = !(total_weight >= Tbypass);
                if(!need_migration)
                {
                    if(writeOp) nvm_write++;
                    else nvm_read++;
                }
                else
                {
                    nvm_write++;
                    nvm_read++;
                }

                if(need_migration)
                {
                    ST_entry real_target = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                    promotee.SetPhysicalAddress( real_target.real_address );
                    migratorTranslator->TranslateWithoutMig(real_target.real_address, &row, &col, &bank, &rank, &channel, &subarray);
                    promotee.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );
                    ChooseVictim( migratorTranslator, promotee, demotee );
                    uint64_t demo_addr = demotee.GetPhysicalAddress();
                    uint64_t real_demo_addr = migratorTranslator->GetFromInvertedTable(demo_addr);
                    uint64_t channel_t, row_t, col_t, bank_t, rank_t, subarray_t;
                    migratorTranslator->TranslateWithoutMig(demo_addr, &row_t, &col_t, &bank_t, &rank_t, &channel_t, &subarray_t);
                    assert(channel_t == 0);
                    if(real_demo_addr == MAX_UINT64)
                    {
                        ST_entry stentry;
                        stentry.real_address = demo_addr;
                        stentry.QAC = 0;
                        real_demo_addr = demo_addr;
                        migratorTranslator->Add2RemapTable(demo_addr, stentry);
                        migratorTranslator->Add2InvertedTable(demo_addr, demo_addr);
                        victim_page_list->RemovePage(demo_addr);
                    }

                    migratorTranslator->Add2InvertedTable(promoteeAddress, demo_addr);
                    unsigned char tmp_dram_qac, tmp_nvm_qac;
                    ST_entry tmp_nvm = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                    ST_entry tmp_dram = migratorTranslator->GetEntryFromRemapTable(real_demo_addr);
                    tmp_nvm_qac = tmp_nvm.QAC;
                    tmp_dram_qac = tmp_dram.QAC;
                    tmp_nvm.QAC = tmp_dram_qac;
                    tmp_dram.QAC = tmp_nvm_qac;
                    migratorTranslator->Add2RemapTable(promoteeAddress, tmp_dram);
                    migratorTranslator->Add2RemapTable(real_demo_addr, tmp_nvm);
                    
                    if(atomic)
                    {
                        ++migrationCount;
                    }
                }

                Train_in_STC(hash, promoteeAddress, writeOp, feature_cnt);
            }
            
            UpdateAddrHistory(promoteeAddress);
        }
    }
    
    return rv;
}


void CoinMigrator::ChooseVictim( Migrator *at, NVMAddress& /*promotee*/, NVMAddress& victim )
{
    /*
     *  Since this is no method called after every module in the system is 
     *  initialized, we check here to see if we have queried the memory system
     *  about the information we need.
     */
    if( !queriedMemory )
    {
        /*
         *  Our naive replacement policy will simply circle through all the pages
         *  in the fast memory. In order to count the pages we need to count the
         *  number of rows in the fast memory channel. We do this by creating a
         *  dummy request which would route to the fast memory channel. From this
         *  we can grab it's config pointer and calculate the page count.
         */
        NVMainRequest queryRequest;

        queryRequest.address.SetTranslatedAddress( 0, 0, 0, 0, promotionChannel, 0 );
        queryRequest.address.SetPhysicalAddress( 0 );
        queryRequest.type = READ;
        queryRequest.owner = this;

        NVMObject *curObject = NULL;
        FindModuleChildType( &queryRequest, SubArray, curObject, parent->GetTrampoline( ) );

        SubArray *promotionChannelSubarray = NULL;
        promotionChannelSubarray = dynamic_cast<SubArray *>( curObject );

        assert( promotionChannelSubarray != NULL );
        Params *p = promotionChannelSubarray->GetParams( );
        promotionChannelParams = p;

        totalPromotionPages = p->RANKS * p->BANKS * p->ROWS;

        currentPromotionPage = 0;

        if( p->COLS != numCols )
        {
            std::cout << "Warning: Page size of fast and slow memory differs." << std::endl;
        }

        queriedMemory = true;
    }

    /*
     *  From the current promotion page, simply craft some translated address together
     *  as the victim address.
     */
    uint64_t row, col, bank, rank, subarray, channel;
    //uint64_t victimRank, victimBank, victimRow, victimSubarray, subarrayCount;
    assert(victim_page_list->Size() != 0);
    ncounter_t promoPage = victim_page_list->UsePage();
    victim.SetPhysicalAddress( promoPage );
    migratorTranslator->TranslateWithoutMig( promoPage, &row, &col, &bank, &rank, &channel, &subarray);
    victim.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );
    /*
    STentry stentry;
    uint64_t srcPage = migratorTranslator->GetFromInvertedTable(promoPage);
    if(srcPage == promoPage){
    stentry.real_address = promoPage;
    stentry.QAC = 0;
    stentry.channel = 0;
    migratorTranslator->Add2RemapTable(promoPage, stentry);
    }
    */
    /*
    victimRank = promoPage % promotionChannelParams->RANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->RANKS );

    victimBank = promoPage % promotionChannelParams->BANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->BANKS );

    subarrayCount = promotionChannelParams->ROWS / promotionChannelParams->MATHeight;
    victimSubarray = promoPage % subarrayCount;
    promoPage >>= NVM::mlog2( subarrayCount );

    victimRow = promoPage;
    */
    //victim.SetTranslatedAddress( victimRow, 0, victimBank, victimRank, /*promotionChannel*/ 0, victimSubarray );
   // uint64_t victimAddress = at->ReverseTranslate( victimRow, 0, victimBank, victimRank, /*promotionChannel*/ 0, victimSubarray );
   // victim.SetPhysicalAddress( victimAddress );

    //currentPromotionPage = (currentPromotionPage + 1) % totalPromotionPages;
    /*
    uint64_t victimRank, victimBank, victimRow, victimSubarray, subarrayCount;
    ncounter_t promoPage = currentPromotionPage;

    victimRank = promoPage % promotionChannelParams->RANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->RANKS );

    victimBank = promoPage % promotionChannelParams->BANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->BANKS );

    subarrayCount = promotionChannelParams->ROWS / promotionChannelParams->MATHeight;
    victimSubarray = promoPage % subarrayCount;
    promoPage >>= NVM::mlog2( subarrayCount );

    victimRow = promoPage;

    victim.SetTranslatedAddress( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    uint64_t victimAddress = at->ReverseTranslate( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    victim.SetPhysicalAddress( victimAddress );

    currentPromotionPage = (currentPromotionPage + 1) % totalPromotionPages;
    */
}


void CoinMigrator::Cycle( ncycle_t /*steps*/ )
{

}

