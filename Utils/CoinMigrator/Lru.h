#ifndef LRU_H
#define LRU_H
#include <unordered_map>
#include <list>
#include "Node.h"

using namespace std;

class Lru
{
    public:
        Lru(int maxlen);
        int Length();
        bool IsInLru(uint64_t address);
        unsigned int GetAC(uint64_t address);
	void IncreaseAC(uint64_t address);
        Node InsertNode(Node node);
        virtual ~Lru();

    protected:

    private:
    int len;
    int maxlen;
    unordered_map<uint64_t, list<Node>::iterator> lru_map;
    list<Node> lru_list;
};

#endif // LRU_H
