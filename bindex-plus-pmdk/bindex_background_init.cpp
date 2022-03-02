#include "bindex.h"

extern pobj::pool<BinDexLayout> pop;

void FreeBlockList::initFreeBlockList(int initCount)
{
    assert(initCount <= maxBlockNum_);
    for (int i = 0; i < initCount; i++) {
        pobj::make_persistent_atomic<pos_block>(pop,freeblocks[i]);
        freeblocks[i]->init_free_pos_block();
        if (i % 1000 == 0)
            cout << i << " ";
    }
    count_ = initCount;
}

pobj::persistent_ptr<pos_block> FreeBlockList::getFreeBlock()
{
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    if(!count_) {
        // temp
        cout << "free block used out!" << endl;
        exit(0);
    }
    while(!count_) // Handle spurious wake-ups.
        condition_.wait(lock);
    --count_;
    unsigned long result = count_;
    pobj::persistent_ptr<pos_block> newBlock = freeblocks[result];
    // lock.~unique_lock();

    // TODO: generate new block here
    return newBlock;                                               
}



FreeBlockList::~FreeBlockList()
{
}



void FreeBlockList::release() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    ++count_;
    condition_.notify_one();
}

void FreeBlockList::acquire() {
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    while(!count_) // Handle spurious wake-ups.
        condition_.wait(lock);
    --count_;
}

bool FreeBlockList::try_acquire() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if(count_) {
        --count_;
        return true;
    }
    return false;
}
