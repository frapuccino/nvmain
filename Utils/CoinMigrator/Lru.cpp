#include "Lru.h"
#include<iostream>
using namespace std;

Lru::Lru(int maxlen)
{
    //ctor
    this->maxlen = maxlen;
    len = 0;
    lru_list.clear();
    lru_map.clear();
}

int Lru::Length()
{
    return len;
}

bool Lru::IsInLru(uint64_t address)
{
    return lru_map.find(address) != lru_map.end();
}

unsigned int Lru::GetAC(uint64_t address)
{
    return lru_map[address]->AC;
}

void Lru::IncreaseAC(uint64_t address)
{
    lru_map[address]->AC += 1;
}

Node Lru::InsertNode(Node node)
{
    list<Node>::iterator iter = lru_list.end();
    //Node* ret = NULL;
    Node tmp;
    tmp.QAC = 10;
    if(lru_map.find(node.address) != lru_map.end())
    {
        iter = lru_map[node.address];
        lru_list.erase(iter);
        //iter->AC += 1;
        lru_list.push_front(*iter);
	lru_map[node.address] = lru_list.begin();
    }
    else
    {
        if(lru_list.size() == maxlen)
        {

            tmp = lru_list.back();
	    //ret = new Node();
	    //ret->address = tmp.address;
	    //ret->QAC = tmp.QAC;
	    //ret->AC = tmp.AC;
            //ret = &tmp;
            lru_map.erase(tmp.address);
            lru_list.pop_back();
            --len;
        }
        lru_list.push_front(node);
        lru_map[node.address] = lru_list.begin();
        ++len;
    }
    return tmp;
}


Lru::~Lru()
{
    //dtor
    lru_list.clear();
    lru_map.clear();
    len = 0;
}
