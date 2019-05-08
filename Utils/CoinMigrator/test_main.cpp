#include <iostream>
#include <fstream>
#include <assert.h>
#include "VictimPageList.h"
using namespace std;

uint64_t trans2dec(string addr){
    uint64_t n=0;
    while(addr.size()>0){
        char ch=addr[0];
        uint64_t ch_n;
        addr=addr.substr(1,addr.size()-1);
        /*
        if(ch>57){ch_n=ch-97+10;}
        else{ch_n=ch-48;}
        n=n*16+ch_n;
        */
        n = n*10 + (ch - '0');
    }
    return n;
}

int main()
{
    //STC* stc_cache = new STC(64, 8);
   // Lru* lru = new Lru(4);
    VictimPageList* vpl = new VictimPageList(100);
    ifstream ifs("TraceMig-2.txt");
    uint64_t id;
    int op;
    string str2;
    int odds = 0, evens = 0;
    int cnt = 0;
    while(ifs >> id >> str2 >> op)
    {
        //cout<<str1<< str2 << op<<endl;
        //if(cnt == 1000) break;
        //++cnt;
        uint64_t addr = trans2dec(str2);
        //uint64_t id = trans2dec(str1) % 1;
        //uint64_t page_num = addr;
        uint64_t page_num = addr >> 12;
        //assert(page_num >= 0 && id >= 0 && id < 4);
        //assert(id != 1 && id != 2);
        //mr->access(page_num, id);
//        Node node;
  //      node.address = page_num;
        //lru->InsertNode(node);
        vpl->RemovePage(page_num);

    }
/*
    lru->Show();
    cout<<lru->Length()<<endl;
    cout<<lru->IsInLru(3)<<endl;
    */
    vpl->Show();
   // cout<<vpl->UsePage()<<endl;
    vpl->Show();
    cout<<vpl->Size()<<endl;
    vpl->GarbageCollection(4096);
    vpl->Show();
    cout<<vpl->IsFree(8193)<<endl;
    return 0;
}
