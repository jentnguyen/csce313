#include "BuddyAllocator.h"
#include <iostream>
#include <math.h>
using namespace std;

BlockHeader* BuddyAllocator::split (BlockHeader* b) {
  int bs = b->block_size;
  b->block_size /= 2;
  b->next = nullptr;

  BlockHeader* sh = (BlockHeader*)((char*)b + b->block_size); //typecast so you can calculate in bytes
  // sh->next = nullptr, sh->block_size = b->block_size;
  BlockHeader* temp = new (sh) BlockHeader (b->block_size);   //alternative
  return sh;
}

BuddyAllocator::BuddyAllocator (int _basic_block_size, int _total_memory_length){
  total_memory_size = _total_memory_length, basic_block_size = _basic_block_size;
  start = new char [total_memory_size];
  int l = ceil(log2(ceil((double)total_memory_size/basic_block_size)));
  for (int i = 0; i < l; i++) {
    FreeList.push_back(LinkedList());
  }
  FreeList.push_back(LinkedList((BlockHeader*)start));
  BlockHeader* h = new (start) BlockHeader (total_memory_size);
}

BuddyAllocator::~BuddyAllocator (){
	delete [] start;
}

char* BuddyAllocator::alloc(int _length) {
  int x = _length + sizeof(BlockHeader); //have to account for the wasted space
  // cout << "allocating something of size " << x << endl;
  int index = ceil(log2 (ceil ((double) x / basic_block_size))); //compute index in FreeList
  int blockSizeReturn = (1 << index) * basic_block_size;

  if (FreeList[index].head != nullptr) {  //if not = nullptr, there is something in the list which means you have found the block of correct size
    BlockHeader* b = FreeList[index].remove();
    b->isFree = 0; //mark that the block is being used
    return (char*) (b+1);
  }

  int indexCorrect = index;    //remember the index, make a backup
  for(; index < FreeList.size(); index++){  //no initializer because index is where we want it to be already
    if(FreeList[index].head) {    //if it's not null then you have found something
      break; //that is the index you are looking for
    }
  }
  if(index >= FreeList.size()) {    //no bigger block found
    return nullptr;    //indicates that the allocation could not happen
  }

  //a bigger block found; you have to split the blocks and look at the smaller ones to see if they are desired size
  while(index > indexCorrect) {    //keep splitting until you find the right size
    BlockHeader* b = FreeList[index].remove();
    BlockHeader* shb = split(b);    //second half of b
    --index;
    FreeList[index].insert(b);
    FreeList[index].insert(shb);
  }
  BlockHeader* block = FreeList[index].remove();
  block->isFree = 0; //block is no longer free
  return (char*)(block + 1);    //advance the block by one item
}
BlockHeader* BuddyAllocator::getbuddy(BlockHeader* b){ //calculate Buddy
  int offset = (int) ((char*)b - start);
  int buddy_offset = offset ^ b->block_size;
  char* buddy = start + buddy_offset;
  return (BlockHeader*) buddy;
}

BlockHeader* BuddyAllocator::merge(BlockHeader* sa, BlockHeader* ba) {    //sa = smaller address (on the left), ba = bigger address (on the right)
  if(sa > ba){ //seeing which block is on the left
    swap(sa, ba); //swap if sa is bigger than ba
  }
  sa->block_size *= 2;
  return sa;
}

int BuddyAllocator::free(char* _a) {
  BlockHeader* b = (BlockHeader*)(_a - sizeof(BlockHeader));    //subtract BlockHeader to backtrack the pointer to the beginning of the block
  // cout << "freeing something of size " << b->block_size;

  while(true) {
    int size = b->block_size;
    b->isFree = 1;
    int index = getIndex(size);
    if(index == FreeList.size() - 1) {    //you have already found the biggest possible block
      FreeList[index].insert(b);
      break;
    }
    BlockHeader* buddy = getbuddy(b);
    if(buddy->isFree && buddy->block_size == b->block_size) {    //check if buddy is actually in the FreeList or not
      FreeList[index].remove(buddy);    //remove the buddy from index
      b = merge(b, buddy);  //merge them back together to get a bigger block
    } else {    //if buddy is free, continue working, otherwise break out of the loop
      FreeList[index].insert(b);   //insert into correct spot when buddy is no longer free
      break;
    }
  }
  return 0;
}

void BuddyAllocator::printlist (){
  cout << "Printing the Freelist in the format \"[index] (block size) : # of blocks\"" << endl;
  for (int i=0; i<FreeList.size(); i++){
    cout << "[" << i <<"] (" << ((1<<i) * basic_block_size) << ") : ";  // block size at index should always be 2^i * bbs
    int count = 0;
    BlockHeader* b = FreeList [i].head;
    // go through the list from head to tail and count
    while (b){
      count ++;
      // block size at index should always be 2^i * bbs
      // checking to make sure that the block is not out of place
      if (b->block_size != (1<<i) * basic_block_size){
        cerr << "ERROR:: Block is in a wrong list " << b->block_size << endl;
        exit (-1);
      }
      b = b->next;
    }
    cout << count << endl;  
  }
}

