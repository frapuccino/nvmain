#ifndef STC_H
#define STC_H
#include "Lru.h"
#include <vector>

class STC
{
    public:
        STC(uint64_t capacity, int way);
        bool IsInSTC(uint64_t address);
        unsigned int GetAC(uint64_t address);
	void IncreaseAC(uint64_t address);
        Node InsertNode(Node node);
        virtual ~STC();

    protected:

    private:
    int way;
    vector<Lru*> stc_cache;
    uint64_t capacity;
    int sets;
    int setBits;
};

#endif // STC_H
