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
int load_instruction (mType *buf, int page, int offset)
{ // load instruction to buffer
  Memory[page*pageSize+offset].mInstr = buf->mInstr;
  //int t = Memory[page*pageSize+offset].mInstr;
  //printf("DEBUG address: %d; INSTR %d, %d, %d\n", page*pageSize+offset, t, t >> opcodeShift, t & operandMask);
}

int load_data (mType *buf, int page, int offset)
{ // load data to buffer (same as load instruction, but use the mData field
  Memory[page*pageSize+offset].mData = buf->mData;
  //printf("DEBUG address: %d; DATA %.0f\n", page*pageSize+offset, Memory[page*pageSize+offset].mData);
}

// load program to swap space, returns the #pages loaded
int load_process_to_swap (int pid, char *fname)
{
int msize, numinstr, numdata;
  int ret, i, j, p,opcode, operand;
  float data; 
  mType *buf = (mType *) malloc (pageSize*sizeof(mType));
  mType *buf2 = (mType *) malloc (pageSize*sizeof(mType));
  int frame;
  int tInstr, tOpcode, tOperand;
  unsigned *temp = (unsigned *) malloc (pageSize*sizeof(unsigned));
  // read from program file "fname" and call load_instruction & load_data
  // to load the program into the buffer, write the program into
  // swap space by inserting it to swapQ
  // update the process page table to indicate that the page is not empty
  // and it is on disk (= diskPage)
  
  init_process_pagetable(pid);
  
  progFd = fopen (fname, "r");
  if (progFd == NULL)
  { printf ("Submission Error: Incorrect program name: %s!\n", fname);
    return progError;
  }
  ret = fscanf (progFd, "%d %d %d\n", &msize, &numinstr, &numdata);
  if (ret < 3)   // did not get all three inputs
  { printf ("Submission failure: missing %d program parameters!\n", 3-ret);
    return progError;
  }
  p=0;
  j=0; //j is offset
  PCB[pid]->dataOffset = numinstr; //must be at least this big an offset
  //frame = get_free_frame(); //need to check if this value is negative; if so, no free frames
  //update_frame_info (frame, pid, p);
  //update_process_pagetable (pid, p, frame);
  for (i=0; i<numinstr; i++)
  { fscanf (progFd, "%d %d\n", &opcode, &operand);
    //if (Debug) printf ("Process %d load instruction: %d, %d, %d\n",
    //                               pid, i, opcode, operand); //opcode, operand
	opcode = opcode << opcodeShift;
    operand = operand & operandMask;
	buf[j].mInstr = opcode | operand;
	//ret = load_instruction (&buf[j], frame, j);
	temp[j] = (unsigned)buf[j].mInstr; //printf("UNSIGNED: 0x%016x 0x%016x\n", buf[j],temp[j]);
    //if (ret == mError) { PCB[pid]->exeStatus = eError; return; } //definitely need to redo this
	j++;
	if (j==pageSize) {
		//write_swap_page (pid, p, buf); 
		insert_swapQ(pid,p,temp,actWrite,Nothing);
		update_process_pagetable (pid, p, -2); //written to disk space
		j=0; p++;
		//dump_one_frame (frame);
		//frame = get_free_frame();
		//update_frame_info (frame, pid, p);
		//update_process_pagetable (pid, p, frame);
	}
  }
  while (j<pageSize) { buf[j].mInstr=0; temp[j] = 0; j++; }
  //write_swap_page (pid, p, buf); 
  insert_swapQ(pid,p,temp,actWrite,Nothing);
  update_process_pagetable (pid, p, -2); //written to disk space
  j=0;  //dump_one_frame (frame);
  //frame = get_free_frame(); //need to check if this value is negative; if so, no free frames
  p++;
  //update_frame_info (frame, pid, p);
  PCB[pid]->dataOffset = p*pageSize + j; //printf("offset %d\n",PCB[pid]->dataOffset);
  for (i=0; i<numdata; i++)
  { fscanf (progFd, "%f\n", &buf[j].mData); 
	//int test=buf->mInstr; buf2[j].mInstr=test; printf("0x%016x 0x%016x %.2f\n" ,buf, test, buf2[j].mData); //test transferring in and out of mType struct
	//ret = load_data (&buf[j], frame, j);
	temp[j] = (unsigned)buf[j].mInstr; //printf("UNSIGNED: 0x%016x 0x%016x\n", buf[j],temp[j]);
    //if (Debug) printf ("Process %d load data: %d, %.2f\n", pid, i, buf[j].mData);
    //if (ret == mError) { PCB[pid]->exeStatus = eError; return; } //definitely need to redo this
	j++;
	if (j==pageSize) {
		//write_swap_page (pid, p, buf);
		insert_swapQ(pid,p,temp,actWrite,Nothing);
		update_process_pagetable (pid, p, -2); //written to disk space
		j=0; p++; //dump_one_frame (frame);
		//frame = get_free_frame();p++;
		//update_frame_info (frame, pid, p);
		//update_process_pagetable (pid, p, frame);
	}
  }
  
   //write_swap_page (pid, p, buf);
   insert_swapQ(pid,p,temp,actWrite,Nothing);
   update_process_pagetable (pid, p, -2); //written to disk space
   
   //update_frame_info(8, pid, 6);
   //int test = find_allocated_memory(pid,6); printf("****** FOUND ALLOCATED MEMORY %d \n", test);
   //for (i=0;i<=p;i++) {
	//   insert_swapQ(pid,i, temp, actRead, Nothing);
   //}
   //insert_swapQ(pid,0, temp, actRead, Nothing);
   //insert_swapQ(pid,1, temp, actRead, Nothing);
   //insert_swapQ(pid,2, temp, actRead, Nothing);
   //code directly below just used for testing; 
   //read_swap_page (pid, 0, buf2); printf ("test Read %d load data: %d, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, \n", pid, 0, buf2[0].mInstr, buf2[1].mInstr, buf2[2].mInstr, buf2[3].mInstr, buf2[4].mInstr, buf2[5].mInstr, buf2[6].mInstr, buf2[7].mInstr);
   //read_swap_page (pid, 1, buf2); printf ("test Read %d load data: %d, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, 0x%016x, \n", pid, 1, buf2[0].mInstr, buf2[1].mInstr, buf2[2].mInstr, buf2[3].mInstr, buf2[4].mInstr, buf2[5].mInstr, buf2[6].mInstr, buf2[7].mInstr);
   //read_swap_page (pid, 2, buf2); printf ("test Read %d load data: %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n", pid, 2, buf2[0].mData, buf2[1].mData, buf2[2].mData, buf2[3].mData, buf2[4].mData, buf2[5].mData, buf2[6].mData, buf2[7].mData);
   
   //dump_one_frame (frame);
   dump_process_pagetable(pid);
   //dump_process_swap(pid);
   return 1; //p;
}

int load_pages_to_memory (int pid, int numpage)
{ unsigned *temp = (unsigned *) malloc (pageSize*sizeof(unsigned));
  int i;
  int *frameNum = (int *) malloc (numpage*sizeof(int));
  for (i=0;i<=numpage;i++) {
	   frameNum[i]=get_free_frame();
	   update_frame_info(frameNum[i], pid, i);
	   insert_swapQ(pid,i, temp, actRead, Nothing);
	   update_process_pagetable(pid, i, -3); //-3 is pending page
   }
   for (i=0;i<=numpage;i++) {
	   update_process_pagetable(pid, i, frameNum[i]);
	   update_frame_info(frameNum[i], pid, i);
   }
   //update page table is in swap code; can be brought back here;
   
  // call insert_swapQ to load the pages of process pid to memory
  // #pages to load = min (loadPpages, numpage = #pages loaded to swap for pid)
  // ask swap.c to place the process to ready queue only after the last load
  // do not forget to update the page table of the process
  // this function has some similarity with page fault handler
}

int load_process (int pid, char *fname)
{ int ret;
  dump_events ();
  ret = load_process_to_swap (pid, fname);   // return #pages loaded
  if (ret != progError) load_pages_to_memory (pid, ret); //dump_events ();
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
  PCB[idlePid]->dataOffset=2;
}

