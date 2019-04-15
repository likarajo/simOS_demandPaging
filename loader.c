#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "simos.h"

// need to be consistent with paging.c: mType and constant definitions
#define opcodeShift 24
#define operandMask 0x00ffffff
#define diskPage -2

FILE *progFd;

//==========================================
// load program into memory and build the process, called by process.c
// a specific pid is needed for loading, since registers are not for this pid
//==========================================

// may return progNormal or progError (the latter, if the program is incorrect)
int load_instruction (mType *buf, int page, int offset) //DONE
{ // DG CODE *******************************************************************************************
  // load instruction to buffer
  Memory[page*pageSize+offset].mInstr = buf->mInstr;
  //int t = Memory[page*pageSize+offset].mInstr;
  //printf("DEBUG address: %d; INSTR %d, %d, %d\n", page*pageSize+offset, t, t >> opcodeShift, t & operandMask);
}

int load_data (mType *buf, int page, int offset) //DONE
{ // DG CODE *******************************************************************************************
  // load data to buffer (same as load instruction, but use the mData field
  Memory[page*pageSize+offset].mData = buf->mData;
  //printf("DEBUG address: %d; DATA %.0f\n", page*pageSize+offset, Memory[page*pageSize+offset].mData);
}

// load program to swap space, returns the #pages loaded
int load_process_to_swap (int pid, char *fname) //DONE for Simple Paging
{
// DG CODE *******************************************************************************************
  int msize, numinstr, numdata;
  int ret, i, j, p,opcode, operand;
  float data;
  mType *buf = (mType *) malloc (sizeof(mType));
  int frame;
  int tInstr, tOpcode, tOperand;
  // read from program file "fname" and call load_instruction & load_data
  // to load the program into the buffer, write the program into
  // swap space by inserting it to swapQ
  // update the process page table to indicate that the page is not empty
  // and it is on disk (= diskPage)

  init_process_pagetable(pid);

  progFd = fopen (fname, "r");
  if (progFd == NULL)
  { printf ("Submission Error: Incorrect program name: %s!\n", fname);
    return;
  }
  ret = fscanf (progFd, "%d %d %d\n", &msize, &numinstr, &numdata);
  if (ret < 3)   // did not get all three inputs
  { printf ("Submission failure: missing %d program parameters!\n", 3-ret);
    return;
  }
  PCB[pid]->numInstr=numinstr; // just used to find out where data is;
  PCB[pid]->numData=numdata; printf("NUM of PAGES %d\n", (int)ceil((double)(PCB[pid]->numData+PCB[pid]->numInstr)/(double)pageSize));
  //PCB[pid]->numPF=ceil((double)msize/(double)pageSize); printf("NUM of PAGES %d\n", PCB[pid]->numPF);
  p=0;
  j=0; //j is offset
  frame = get_free_frame(); //need to check if this value is negative; if so, no free frames
  update_frame_info (frame, pid, p);
  update_process_pagetable (pid, p, frame);
  for (i=0; i<numinstr; i++)
  { fscanf (progFd, "%d %d\n", &opcode, &operand);
	//fscanf (progFd, "%d %.0f\n", &buf->mInstr, &buf->mData);
    if (Debug) printf ("Process %d load instruction: %d, %d, %d\n",
                                   pid, i, opcode, operand); //opcode, operand
	opcode = opcode << opcodeShift;
    operand = operand & operandMask;
	buf->mInstr = opcode | operand;
	// ************************************************************
	// code below is how to get opcode and operand from memory structure
	//
	//tInstr = buf->mInstr;
	//tOpcode = tInstr >> opcodeShift;
	//tOperand = tInstr & operandMask;
    //if (Debug) printf ("Process*** %d load instruction: %d, %d, %.0f\n",
    //                               pid, i, tOpcode, tOperand);

	// ********************************************
	ret = load_instruction (buf, frame, j);
    //if (ret == mError) { PCB[pid]->exeStatus = eError; return; } //definitely need to redo this
	j++;
	if (j==pageSize) {
		j=0; //dump_one_frame (frame);
		frame = get_free_frame(); p++;
		update_frame_info (frame, pid, p);
		update_process_pagetable (pid, p, frame);
	}
  }
  //j=0;  //dump_one_frame (frame);
  //frame = get_free_frame(); //need to check if this value is negative; if so, no free frames
  //p++;
  //update_frame_info (frame, pid, p);
  PCB[pid]->dataOffset = p*pageSize + j; //printf("offset %d\n",PCB[pid]->dataOffset);
  for (i=0; i<numdata; i++)
  { fscanf (progFd, "%f\n", &buf->mData);
	ret = load_data (buf, frame, j);
    if (Debug) printf ("Process %d load data: %d, %.2f\n", pid, i, buf->mData);
    //if (ret == mError) { PCB[pid]->exeStatus = eError; return; } //definitely need to redo this
	j++;
	if (j==pageSize) {
		j=0;  //dump_one_frame (frame);
		frame = get_free_frame();p++;
		update_frame_info (frame, pid, p);
		update_process_pagetable (pid, p, frame);
	}
  }
   //dump_one_frame (frame);
   dump_process_pagetable(pid);
   //free_process_memory(pid);
// ***************************************************************************************************
}

int load_pages_to_memory (int pid, int numpage) // To be done For Demand Paging
{
  // call insert_swapQ to load the pages of process pid to memory
  // #pages to load = min (loadPpages, numpage = #pages loaded to swap for pid)
  // ask swap.c to place the process to ready queue only after the last load
  // do not forget to update the page table of the process
  // this function has some similarity with page fault handler
}

int load_process (int pid, char *fname) // DONE
{ int ret;

  ret = load_process_to_swap (pid, fname);   // return #pages loaded
  if (ret != progError) load_pages_to_memory (pid, ret);
  return (ret);
}

// load idle process, idle process uses OS memory
// We give the last page of OS memory to the idle process
#define OPifgo 5   // has to be consistent with cpu.c
void load_idle_process ()
{ int page, frame;
  int instr, opcode, operand, data;

  init_process_pagetable (idlePid);
  page = 0;   frame = OSpages - 1;
  update_process_pagetable (idlePid, page, frame);
  update_frame_info (frame, idlePid, page);

  // load 1 ifgo instructions (2 words) and 1 data for the idle process
  opcode = OPifgo;   operand = 0;
  instr = (opcode << opcodeShift) | operand;
  direct_put_instruction (frame, 0, instr);   // 0,1,2 are offset
  direct_put_instruction (frame, 1, instr);
  direct_put_data (frame, 2, 1);
  PCB[idlePid]->numInstr=2;
  PCB[idlePid]->dataOffset=2;
}

