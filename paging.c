#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "simos.h"

// Memory definitions, including the memory itself and a page structure
// that maintains the informtion about each memory page
// config.sys input: pageSize, numFrames, OSpages
// ------------------------------------------------
// process page table definitions
// config.sys input: loadPpages, maxPpages

//mType *Memory;   // The physical memory, size = pageSize*numFrames

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
int calculate_memory_address (unsigned offset, int rwflag) // DONE
{
  // rwflag is used to differentiate the caller
  // different access violation decisions are made for reader/writer
  // if there is a page fault, need to set the page fault interrupt
  // also need to set the age and dirty fields accordingly
  // returns memory address or mPFault or mError

  int pageIndex;

  if(offset == 0){
    pageIndex = 0;
  } else {
    pageIndex = offset / pageSize + 1; //shouldn't add 1
  }

   if(pageIndex >= maxPpages){
    return mError; // memory access violation
   }

   int frame = CPU.PTptr[pageIndex];
   if(frame == nullPage){
    return mError; //access violation
   } else if (frame == diskPage){
    if(rwflag == flagRead){
        set_interrupt(pFaultException);
        return mPFault;
    } else if(rwflag == flagWrite){
           //swap ??
    } else {
        return mError;
    }
  } else if (frame == pendingPage) {
    return -1;
   	//break;
  } else {
    int memOffset = frame * pageSize;
   	int pageOffset = offset - pageIndex * pageSize;
   	int address = memOffset + pageOffset;

   	// set age and dirty fields
   	memFrame[frame].dirty = dirtyFrame;
    memFrame[frame].age = zeroAge; //should be zeroAge or highest age: used age constants

   	return address;
  }

}

int get_data (int offset) // DONE
{
  // call calculate_memory_address to get memory address
  // copy the memory content to MBR
  // return mNormal, mPFault or mError

  int address = calculate_memory_address(CPU.dataOffset + offset, flagRead);

  if (address == mError){
    return mError;
  } else if (address == mPFault){
    return mPFault;
  } else {
    CPU.MBR = Memory[address].mData;
    return mNormal;
  }

}

int put_data (int offset) // DONE
{
  // call calculate_memory_address to get memory address
  // copy MBR to memory
  // return mNormal, mPFault or mError

  int address = calculate_memory_address(CPU.dataOffset + offset, flagWrite);

  if (address == mError){
    return mError;
  } else if (address == mPFault){
    return mPFault;
  } else {
    Memory[address].mData = CPU.MBR;

    return mNormal;
  }

}

int get_instruction (int offset) // DONE
{
  // call calculate_memory_address to get memory address
  // convert memory content to opcode and operand
  // return mNormal, mPFault or mError

  int address = calculate_memory_address(offset, flagRead);

  if (address == mError){
    return mError;
  } else if (address == mPFault){
    return mPFault;
  } else {
    int instr = Memory[address].mInstr;
    CPU.IRopcode = instr >> opcodeShift;
    CPU.IRoperand = instr & operandMask;
    return mNormal;
  }

}

// these two direct_put functions are only called for loading idle process
// no specific protection check is done
void direct_put_instruction (int findex, int offset, int instr)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  //printf("Address stuff: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", offset, pageoffsetMask, findex, pagenumShift,(offset & pageoffsetMask), (findex << pagenumShift));
  Memory[addr].mInstr = instr;
  printf("Mem: 0x%08x, Data: 0x%08x  0x%08x  0x%08x\n", addr, Memory[addr], Memory[addr].mInstr, opcodeShift);
}

void direct_put_data (int findex, int offset, mdType data)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  //printf("Address stuff: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", offset, pageoffsetMask, findex, pagenumShift,(offset & pageoffsetMask), (findex << pagenumShift));
  Memory[addr].mData = data;
  printf("Mem: 0x%08x, Data: 0x%08x 0x%08x\n", addr, Memory[addr], Memory[addr].mData);
}

//==========================================
// Memory and memory frame management
//==========================================

void dump_one_frame (int findex) // DONE
{
  // dump the content of one memory frame

  int i;

  printf("************Dump the content of memory frame %d\n", findex);
  for(i = findex * pageSize; i < (findex + 1) * pageSize; i++){
    printf("Mem: 0x%032x, Data: 0x%016x,  %f\n", i, Memory[i], Memory[i].mData); //maybe should dump memory and instructions separately?
  }

}

void dump_memory () // DONE
{ int i;

  printf ("************ Dump the entire memory\n");
  for (i=0; i<numFrames; i++) dump_one_frame (i);
}

// above: dump memory content, below: only dump frame infor

void dump_free_list () // DONE
{
  // dump the list of free memory frames

  int i = freeFhead;

  printf ("*************Dump the list of Free memory Frame\n");
  while(i < nullIndex){
    printf ("Free Frame %d: ", i);
    i = memFrame[i].next;
  }
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

void  update_frame_info (findex, pid, page) // To be checked by Dinidu
int findex, pid, page; //findex is frame in memory; page is page in page table
{
  // update the metadata of a frame, need to consider different update scenarios
  // need this function also becuase loader also needs to update memFrame fields
  // while it is better to not to expose memFrame fields externally

  if (PCB[pid]->PTptr[page] == nullPage) {
	  //
  }
  PCB[pid]->PTptr[page] = findex;
  if (page == 0) {
	  memFrame[findex].prev = nullIndex;
	  memFrame[findex].next = PCB[pid]->PTptr[page+1];
  }
  else if (page == maxPpages-1) {
	  memFrame[findex].prev = PCB[pid]->PTptr[page-1];
	  memFrame[findex].next = nullIndex;
  }
  else {
	  memFrame[findex].prev = PCB[pid]->PTptr[page-1];
	  memFrame[findex].next = PCB[pid]->PTptr[page+1];
  }


  memFrame[findex].age = zeroAge;
  memFrame[findex].dirty = cleanFrame; //will change for VM
  memFrame[findex].free = usedFrame;
  memFrame[findex].pinned = nopinFrame;
  memFrame[findex].pid = pid;
  memFrame[findex].page = page;
}

// should write dirty frames to disk and remove them from process page table
// but we delay updates till the actual swap (page_fault_handler)
// unless frames are from the terminated process (status = nullPage)
// so, the process can continue using the page, till actual swap


void addto_free_frame (int findex, int status) // To be done For demand paging
{
}

int select_agest_frame ()
{
  // select a frame with the lowest age
  // if there are multiple frames with the same lowest age, then choose the one
  // that is not dirty
}

int get_free_frame () // DONE
{
  // DG CODE *******************************************************************************************
  int i, ret;
  ret =freeFhead;
	if (freeFhead != nullIndex) {
		i=freeFhead+1;
		while ((memFrame[i].free == usedFrame) && (i <= freeFtail)) { i++; }
		if (i > freeFtail) {
			freeFhead = nullIndex; freeFtail = nullIndex;
		}
		else {
			freeFhead = i;
		}
	}
  return ret;
	// ***************************************************************************************************
// get a free frame from the head of the free list
// if there is no free frame, then get one frame with the lowest age
// this func always returns a frame, either from free list or get one with lowest age
}

void initialize_memory () // DONE
{ int i;

  // create memory + create page frame array memFrame
  Memory = (mType *) malloc (numFrames*pageSize*sizeof(mType));
  memFrame = (FrameStruct *) malloc (numFrames*sizeof(FrameStruct));

  // compute #bits for page offset, set pagenumShift and pageoffsetMask
  // *** ADD CODE
  pagenumShift = (int)(log((double)(OSpages -1)*pageSize)/log(2.0));
  pageoffsetMask = (OSpages -1)*pageSize-1;

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

  // DG CODE *******************************************************************************************
  // printf("DEBUG PAGING\n");
  freeFhead = OSpages;
  freeFtail = numFrames - 1;
  for (i=OSpages; i<numFrames; i++)
  {
	  memFrame[i].free = usedFrame;
	  if (i == 0) {
		  memFrame[i].prev = nullIndex;
		  memFrame[i].next = i+1;
	  }
	  else if (i == numFrames-1) {
		  memFrame[i].prev = i-1;
		  memFrame[i].next = nullIndex;
	  }
	  else {
		  memFrame[i].prev = i-1;
		  memFrame[i].next = i+1;
	  }
	memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = freeFrame;
    memFrame[i].pinned = nopinFrame;
    memFrame[i].pid = nullIndex;
  }
  // ***************************************************************************************************

}

//==========================================
// process page table manamgement
//==========================================

void init_process_pagetable (int pid) // DONE
{ int i;

  PCB[pid]->PTptr = (int *) malloc (addrSize*maxPpages);
  for (i=0; i<maxPpages; i++) PCB[pid]->PTptr[i] = nullPage;
  PCB[pid]->PC=0;
}

// frame can be normal frame number or nullPage, diskPage
void update_process_pagetable (pid, page, frame) // DONE
int pid, page, frame;
{
  // update the page table entry for process pid to point to the frame
  // or point to disk or null
  PCB[pid]->PTptr[page] = frame; //need to put code here to handle null page
}

int free_process_memory (int pid) // DONE
{ int i, j, k, f; //printf("%d \n", get_free_frame());
  for (i=0; i<ceil((double)(PCB[pid]->numData+PCB[pid]->numInstr)/(double)pageSize); i++) {
	  for(j = PCB[pid]->PTptr[i] * pageSize; j < (PCB[pid]->PTptr[i] + 1) * pageSize; j++){
		printf("Mem: 0x%032x, Data: 0x%016x,  %f\n", j, Memory[j], Memory[j].mData);
		Memory[j].mInstr=0;
	  }
	  k = PCB[pid]->PTptr[i];
	  memFrame[k].free = freeFrame;
	  memFrame[k].pid = nullIndex;
	  memFrame[k].page = nullPage;
	  if (k >freeFtail) {
		  memFrame[k].prev=freeFtail;
		  memFrame[k].next = nullIndex;
		  memFrame[freeFtail].next = k;
		  freeFtail = k;
	  }
	  else if (k < freeFhead) {
		  memFrame[k].next=freeFhead;
		  memFrame[k].prev = nullIndex;
		  memFrame[freeFhead].prev = k;
		  freeFhead = k;
	  }
	  else {
		f = freeFhead;
		while (k>f) {f = memFrame[f].next;}
		memFrame[k].prev = memFrame[f].prev;
		memFrame[k].next = f;
	  }
  }

  //printf("%d \n", get_free_frame());
  // free the memory frames for a terminated process
  // some frames may have already been freed, but still in process pagetable
}

void dump_process_pagetable (int pid) // DONE
{ int i;
  for (i=0; i<ceil((double)(PCB[pid]->numData+PCB[pid]->numInstr)/(double)pageSize); i++) {
	  printf("Page %d frame %d \n", i, PCB[pid]->PTptr[i]);
  }
  // print page table entries of process pid
}

void dump_process_memory (int pid) // DONE
{ int i;
  for (i=0; i<ceil((double)(PCB[pid]->numData+PCB[pid]->numInstr)/(double)pageSize); i++) {
	  dump_one_frame(PCB[pid]->PTptr[i]);
  }
  // print out the memory content for process pid
}

//==========================================
// the major functions for paging, invoked externally
//==========================================

#define sendtoReady 1  // has to be the same as those in swap.c
#define notReady 0
#define actRead 0
#define actWrite 1

void page_fault_handler () // To be done for Demand Paging
{
  // handle page fault
  // obtain a free frame or get a frame with the lowest age
  // if the frame is dirty, insert a write request to swapQ
  // insert a read request to swapQ to bring the new page to this frame
  // update the frame metadata and the page tables of the involved processes
}

// scan the memory and update the age field of each frame


void memory_agescan () // To be done for Demand Paging
{
}

void initialize_memory_manager () // DONE
{
  // DG CODE *******************************************************************************************
  initialize_memory();
  // ***************************************************************************************************	
  // initialize memory and add page scan event request
}

