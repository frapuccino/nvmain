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

    minbenefit = 8;
    cur_epoch = 0;
    interval = 2000;

    queriedMemory = false;
    promotionChannelParams = NULL;
    currentPromotionPage = 0;
    a1 = a2 = 0;
    
    access_count = hit_count = 0;

    memset(tmp_accum_cnt, 0, sizeof(tmp_accum_cnt));
    memset(tmp_num_q, 0, sizeof(tmp_num_q));
    memset(tmp_num_q_sumE, 0, sizeof(tmp_num_q_sumE));
    memset(tmp_num_q_sumI, 0, sizeof(tmp_num_q_sumI));
    memset(accum_cnt, 0, sizeof(accum_cnt));
    memset(num_q, 0, sizeof(num_q));
    memset(num_q_sumE, 0, sizeof(num_q_sumE));
    memset(num_q_sumI, 0, sizeof(num_q_sumI));
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
    ranks = 1;banks = 8; rows = 65536;
    uint64_t total = ranks*banks*rows;
    int stc_size = 8192;
    stc_cache = new STC(stc_size, 8);
    victim_page_list = new VictimPageList(total);

}


bool CoinMigrator::IssueAtomic( NVMainRequest *request )
{
    /* For atomic mode, we just swap the pages instantly. */
    return TryMigration( request, true );
}


bool CoinMigrator::IssueCommand( NVMainRequest *request )
{
    /* 
     *  In cycle-accurate mode, we must read each page, buffer it, enqueue a
     *  write request, and wait for write completion.
     */
    return TryMigration( request, false );
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
            /* A migration read completed, update state. */
            migratorTranslator->SetMigrationState( request->address, MIGRATION_BUFFERED ); 

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
                    migratorTranslator->SetMigrationState( demoRequest->address, MIGRATION_WRITING );
                }
                if( promoIssued )
                {
                    migratorTranslator->SetMigrationState( promoRequest->address, MIGRATION_WRITING );
                }

                promoBuffered = !promoIssued;
                demoBuffered = !demoIssued;
            }
        }
        /* A write completed. */
        else if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_WRITE_TAG )
        {
            // Note: request should be deleted by parent
            migratorTranslator->SetMigrationState( request->address, MIGRATION_DONE );

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

void CoinMigrator::InsertToSTC(Node node)
{
    Node evicted_node = stc_cache->InsertNode(node);
            if(evicted_node.QAC != 10)
            {
                int qacv = GetQACV(evicted_node.AC);
                tmp_accum_cnt[qacv] += evicted_node.AC;

                tmp_num_q_sumI[qacv] += 1;
                tmp_num_q[evicted_node.QAC][qacv] += 1;
                tmp_num_q_sumE[evicted_node.QAC] += 1;
                //uint64_t row, col, bank, rank, channel, subarray;
                STentry tmp = migratorTranslator->GetEntryFromRemapTable(evicted_node.address);
                //migratorTranslator->TranslateWithoutMig(evicted_node.address, &row, &col, &bank, &rank, &channel, &subarray);
                if(tmp.channel == 0)
                {
                    victim_page_list->GarbageCollection(tmp.real_address);
                }
                STentry new_entry;
                new_entry.real_address = tmp.real_address;
                new_entry.QAC = qacv;
                new_entry.channel = tmp.channel;
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
bool CoinMigrator::TryMigration( NVMainRequest *request, bool atomic )
{
    bool rv = true;

    if( NVMTypeMatches(NVMain) )
    {
        /* Ensure the Migrator translator is used. */
        migratorTranslator = dynamic_cast<Migrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        /* Migrations in progress must be served from the buffers during migration. */
        if( GetCurrentHookType( ) == NVMHOOK_PREISSUE && migratorTranslator->IsBuffered( request->address ) )
        {
            /* Short circuit this request so it is not queued. */
            rv = false;

            /* Complete the request, adding some buffer read latency. */
            GetEventQueue( )->InsertEvent( EventResponse, parent->GetTrampoline( ), request,
                              GetEventQueue()->GetCurrentCycle()+bufferReadLatency );

            bufferedReads++;

            return rv;
        }

        /* Don't inject results before the original is issued to prevent deadlock */
        if( GetCurrentHookType( ) != NVMHOOK_POSTISSUE )
        {
            return rv;
        }



	/*gyx:for debug  output the physical address;*/
	    std::ofstream output_mig;
	    output_mig.open("/home/hx/gem5/TraceMig.txt",std::ios::app);
	    output_mig<<request->threadId<<" "<<request->address.GetPhysicalAddress()<<std::endl;
	 




        /* See if any migration is possible (i.e., no migration is in progress) */
        bool migrationPossible = false;

        if( !migratorTranslator->Migrating( ) )
            //&& !migratorTranslator->IsMigrated( request->address ) 
            //&& request->address.GetChannel( ) != promotionChannel )
        {
                migrationPossible = true;
        }

	bool need_migration = false;
            uint64_t row, bank, rank, channel, subarray;
            request->address.GetTranslatedAddress( &row, NULL, &bank, &rank, &channel, &subarray );
            uint64_t promoteeAddress = migratorTranslator->ReverseTranslate( row, 0, bank, rank, channel, subarray );
        if( migrationPossible )
        {
            assert( !demoBuffered && !promoBuffered );

            
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
            //uint64_t real_promotee_addr = promoteeAddress;
            Node node;
	    if(!migratorTranslator->IsMapped(promoteeAddress))
            //if(remap_table.find(promoteeAddress) == remap_table.end())
            {
                STentry stentry;
                stentry.real_address = promoteeAddress;
                stentry.QAC = 0;
		stentry.channel = channel;
		migratorTranslator->Add2RemapTable(promoteeAddress, stentry);
                //remap_table[promoteeAddress] = stentry;
                node.address = promoteeAddress;
                node.QAC = 0;
                node.AC = 1;
		uint64_t real_channel = request->address.GetChannel();
		stentry.channel = real_channel;
                if(request->address.GetChannel() == 0)
                {
		    migratorTranslator->Add2InvertedTable(promoteeAddress, promoteeAddress);
                    victim_page_list->RemovePage(promoteeAddress);
		    
                }
		this->InsertToSTC(node);
            }
            else
            {
                if(!(stc_cache->IsInSTC(promoteeAddress)))
                {
		    STentry real_target = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                    node.address = promoteeAddress;
		    node.QAC = real_target.QAC;
                    node.AC = 1;
		    uint64_t row, col, bank, rank, channel, subarray;
                    migratorTranslator->TranslateWithoutMig(real_target.real_address, &row, &col, &bank, &rank, &channel, &subarray);;
		    //if(channel == 0)
		    if(real_target.channel == 0)
                    //if(request->address.GetChannel() == 0)
                    {
                        victim_page_list->RemovePage(real_target.real_address);
                    }
		    this->InsertToSTC(node);
                }
                else
                {
		    hit_count += 1;
                    node.address = promoteeAddress;
		   stc_cache->IncreaseAC(promoteeAddress); 
		    STentry real_target = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
		    this->InsertToSTC(node);
		    uint64_t real_addr = real_target.real_address;
                    NVMAddress real_NVMAddr;
                    uint64_t row, col, bank, rank, channel, subarray;
                    migratorTranslator->TranslateWithoutMig(real_addr, &row, &col, &bank, &rank, &channel, &subarray);
		    if(channel == 1)
                    {
                        double exp_cnt = 0;
			unsigned char q_i = real_target.QAC;
                        for(int i = 1; i < 4; ++i)
                        {
			    if(num_q_sumI[i] == 0) continue;
                            double avg_cnt = (double)accum_cnt[i] / (double)num_q_sumI[i];
                            double p_qe_qi = ((double)num_q[q_i][i] + 1) / (num_q_sumE[q_i] + 3);
                            exp_cnt += avg_cnt * p_qe_qi;
                        }
			
                        unsigned int ac = stc_cache->GetAC(promoteeAddress);
                        if(exp_cnt - ac > minbenefit)
                        {
                            need_migration = true;
                        }
                    }
                }
            }
	    //cout<<"hit rate--------------------->"<<(double)hit_count / access_count<<endl;
	    // cout<<promoteeAddress<<endl;
            if(need_migration)
            {

                NVMObject *savedParent = parent->GetTrampoline( );
                /* Discard the unused column address. */

                uint64_t row, bank, rank, channel, subarray;
		
                request->address.GetTranslatedAddress( &row, NULL, &bank, &rank, &channel, &subarray );
                uint64_t promoteeAddress = migratorTranslator->ReverseTranslate( row, 0, bank, rank, channel, subarray ); 
		
		 //STentry real_target = migratorTranslator->GetEntryFromRemapTable(promoteeAddress);
                 //uint64_t real_addr = real_target.real_address;
                 //migratorTranslator->TranslateWithoutMig(real_addr, &row, &col, &bank, &rank, &channel, &subarray);
                 promotee.SetPhysicalAddress( promoteeAddress );
                 promotee.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );
                
		/*
                promotee.SetPhysicalAddress( real_promotee_addr );
		migratorTranslator->TranslateWithoutMig(real_promotee_addr, &row, &col, &bank, &rank, &channel, &subarray);
                promotee.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );
		*/
                /* Pick a victim to replace. */
                ChooseVictim( migratorTranslator, promotee, demotee );

               // assert( migratorTranslator->IsMigrated( demotee ) == false );
               // assert( migratorTranslator->IsMigrated( promotee ) == false );

                if( atomic )
                {
                    // migratorTranslator->StartMigration( request->address, demotee );
                    migratorTranslator->StartMigration(promotee, demotee);
                    migratorTranslator->SetMigrationState( promotee, MIGRATION_DONE );
                    migratorTranslator->SetMigrationState( demotee, MIGRATION_DONE );
                }
                /* Lastly, make sure we can queue the migration requests. */
                else if( CheckIssuable( promotee, READ ) &&
                         CheckIssuable( demotee, READ ) )
                {
                    // migratorTranslator->StartMigration( request->address, demotee );
                    migratorTranslator->StartMigration(promotee, demotee);

                    promoRequest = new NVMainRequest( ); 
                    demoRequest = new NVMainRequest( );

                    promoRequest->address = promotee;
                    promoRequest->type = READ;
                    promoRequest->tag = MIG_READ_TAG;
                    promoRequest->burstCount = numCols;

                    demoRequest->address = demotee;
                    demoRequest->type = READ;
                    demoRequest->tag = MIG_READ_TAG;
                    demoRequest->burstCount = numCols;

                    promoRequest->owner = savedParent;
                    demoRequest->owner = savedParent;
                    savedParent->IssueCommand( promoRequest );
                    savedParent->IssueCommand( demoRequest );
                }
                else
                {
                    queueWaits++;
                }
            //}
            }
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

    uint64_t victimRank, victimBank, victimRow, victimSubarray, subarrayCount;
    ncounter_t promoPage = victim_page_list->UsePage();

    STentry stentry;
    uint64_t srcPage = migratorTranslator->GetFromInvertedTable(promoPage);
    if(srcPage == promoPage){
    stentry.real_address = promoPage;
    stentry.QAC = 0;
    stentry.channel = 0;
    migratorTranslator->Add2RemapTable(promoPage, stentry);
    }
    victimRank = promoPage % promotionChannelParams->RANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->RANKS );

    victimBank = promoPage % promotionChannelParams->BANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->BANKS );

    subarrayCount = promotionChannelParams->ROWS / promotionChannelParams->MATHeight;
    victimSubarray = promoPage % subarrayCount;
    promoPage >>= NVM::mlog2( subarrayCount );

    victimRow = promoPage;

    victim.SetTranslatedAddress( victimRow, 0, victimBank, victimRank, /*promotionChannel*/ 0, victimSubarray );
    uint64_t victimAddress = at->ReverseTranslate( victimRow, 0, victimBank, victimRank, /*promotionChannel*/ 0, victimSubarray );
    victim.SetPhysicalAddress( victimAddress );

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

