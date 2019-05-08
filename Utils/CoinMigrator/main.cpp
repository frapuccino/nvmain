#include "STC.h"
#include <iostream>
using namespace std;
int main()
{
    STC* stc = new STC(4,2);
    for(uint64_t i = 0; i < 10; ++i)
    {
        Node node;
        node.address = i;
        stc->InsertNode(node);
    
    }
    Node node;
    node.address = 6;
    stc->InsertNode(node);
    stc->Show();
    //cout<<lru->IsInLru(9)<<endl;
}
