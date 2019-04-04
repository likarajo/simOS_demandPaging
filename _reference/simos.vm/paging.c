#include <stdio.h>
#include <stdlib.h>
#include "simos.h"

// Memory definitions, including the memory itself and a page structure
// that maintains the informtion about each memory page
// config.sys input: pageSize, numFrames, OSpages
// ------------------------------------------------
// process page table definitions
// config.sys input: loadPpages, maxPpages

mType *Memory;   // The physical memory, size = pageSize*numFrames

typedef unsigned ageType;
typedef struct
{ int pid, page;   // the frame is allocated to process pid for page page
  ageType age;
  char free, dirty, pinned;   // in real systems, these are bits
  int next, prev;
} FrameStruct;

FrameStruct *memFrame;   // memFrame[numFrames]
int freeFhead, freeFtail;   // the head and tail of free frame list

// define special values for page/frame number
#define nullIndex -1   // free frame list null pointer
#define nullPage -1   // page does not exist yet
#define diskPage -2   // page is on disk swap space
#define pendingPage -3  // page is pending till it is actually swapped
   // have to ensure: #memory-frames < address-space/2, (pageSize >= 2)
   //    becuase we use negative values with the frame number
   // nullPage & diskPage are used in process page table 

// define values for fields in FrameStruct
#define zeroAge 0x00000000
#define highestAge 0x80000000
#define dirtyFrame 1
#define cleanFrame 0
#define freeFrame 1
#define usedFrame 0
#define pinnedFrame 1
#define nopinFrame 0

// define shifts and masks for instruction and memory address 
#define opcodeShift 24
#define operandMask 0x00ffffff

// shift address by pagenumShift bits to get the page number
unsigned pageoffsetMask;
int pagenumShift; // 2^pagenumShift = pageSize

//============================
// Our memory implementation is a mix of memory manager and physical memory.
// get_instr, put_instr, get_data, put_data are the physical memory operations
//   for instr, instr is fetched into registers: IRopcode and IRoperand
//   for data, data is fetched into registers: MBR (need to retain AC value)
// page table management is software implementation
//============================


//==========================================
// run time memory access operations, called by cpu.c
//==========================================

// define rwflag to indicate whehter the addr computation is for read or write
#define flagRead 1
#define flagWrite 2

// address calcuation are performed for the program in execution
// so, we can get address related infor from CPU registers
int calculate_memory_address (unsigned offset, int rwflag)
{ 
  // rwflag is used to differentiate the caller
  // different access violation decisions are made for reader/writer
  // if there is a page fault, need to set the page fault interrupt
  // also need to set the age and dirty fields accordingly
  // returns memory address or mPFault or mError
}

int get_data (int offset)
{ 
  // call calculate_memory_address to get memory address
  // copy the memory content to MBR
  // return mNormal, mPFault or mError
}

int put_data (int offset)
{ 
  // call calculate_memory_address to get memory address
  // copy MBR to memory 
  // return mNormal, mPFault or mError
}

int get_instruction (int offset)
{ 
  // call calculate_memory_address to get memory address
  // convert memory content to opcode and operand
  // return mNormal, mPFault or mError
}

// these two direct_put functions are only called for loading idle process
// no specific protection check is done
void direct_put_instruction (int findex, int offset, int instr)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mInstr = instr;
}

void direct_put_data (int findex, int offset, mdType data)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mData = data;
}

//==========================================
// Memory and memory frame management
//==========================================

void dump_one_frame (int findex)
{ 
  // dump the content of one memory frame
}

void dump_memory ()
{ int i;

  printf ("************ Dump the entire memory\n");
  for (i=0; i<numFrames; i++) dump_one_frame (i);
}

// above: dump memory content, below: only dump frame infor

void dump_free_list ()
{ 
  // dump the list of free memory frames
}

void print_one_frameinfo (int indx)
{ printf ("pid/page/age=%d,%d,%x, ",
          memFrame[indx].pid, memFrame[indx].page, memFrame[indx].age);
  printf ("dir/free/pin=%d/%d/%d, ",
          memFrame[indx].dirty, memFrame[indx].free, memFrame[indx].pinned);
  printf ("next/prev=%d,%d\n",
          memFrame[indx].next, memFrame[indx].prev);
}

void dump_memoryframe_info ()
{ int i;

  printf ("******************** Memory Frame Metadata\n");
  printf ("Memory frame head/tail: %d/%d\n", freeFhead, freeFtail);
  for (i=OSpages; i<numFrames; i++)
  { printf ("Frame %d: ", i); print_one_frameinfo (i); }
  dump_free_list ();
}

void  update_frame_info (findex, pid, page)
int findex, pid, page;
{
  // update the metadata of a frame, need to consider different update scenarios
  // need this function also becuase loader also needs to update memFrame fields
  // while it is better to not to expose memFrame fields externally
}

// should write dirty frames to disk and remove them from process page table
// but we delay updates till the actual swap (page_fault_handler)
// unless frames are from the terminated process (status = nullPage)
// so, the process can continue using the page, till actual swap
void addto_free_frame (int findex, int status)
{
}

int select_agest_frame ()
{ 
  // select a frame with the lowest age 
  // if there are multiple frames with the same lowest age, then choose the one
  // that is not dirty
}

int get_free_frame ()
{ 
// get a free frame from the head of the free list 
// if there is no free frame, then get one frame with the lowest age
// this func always returns a frame, either from free list or get one with lowest age
} 

void initialize_memory ()
{ int i;

  // create memory + create page frame array memFrame 
  Memory = (mType *) malloc (numFrames*pageSize*sizeof(mType));
  memFrame = (FrameStruct *) malloc (numFrames*sizeof(FrameStruct));

  // compute #bits for page offset, set pagenumShift and pageoffsetMask
  // *** ADD CODE

  // initialize OS pages
  for (i=0; i<OSpages; i++)
  { memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = usedFrame;
    memFrame[i].pinned = pinnedFrame;
    memFrame[i].pid = osPid;
  }
  // initilize the remaining pages, also put them in free list
  // *** ADD CODE

}

//==========================================
// process page table manamgement
//==========================================

void init_process_pagetable (int pid)
{ int i;

  PCB[pid]->PTptr = (int *) malloc (addrSize*maxPpages);
  for (i=0; i<maxPpages; i++) PCB[pid]->PTptr[i] = nullPage;
}

// frame can be normal frame number or nullPage, diskPage
void update_process_pagetable (pid, page, frame)
int pid, page, frame;
{ 
  // update the page table entry for process pid to point to the frame
  // or point to disk or null
}

int free_process_memory (int pid)
{ 
  // free the memory frames for a terminated process
  // some frames may have already been freed, but still in process pagetable
}

void dump_process_pagetable (int pid)
{ 
  // print page table entries of process pid
}

void dump_process_memory (int pid)
{ 
  // print out the memory content for process pid
}

//==========================================
// the major functions for paging, invoked externally
//==========================================

#define sendtoReady 1  // has to be the same as those in swap.c
#define notReady 0   
#define actRead 0   
#define actWrite 1

void page_fault_handler ()
{ 
  // handle page fault
  // obtain a free frame or get a frame with the lowest age
  // if the frame is dirty, insert a write request to swapQ 
  // insert a read request to swapQ to bring the new page to this frame
  // update the frame metadata and the page tables of the involved processes
}

// scan the memory and update the age field of each frame
void memory_agescan ()
{ 
}

void initialize_memory_manager ()
{ 
  // initialize memory and add page scan event request
}

