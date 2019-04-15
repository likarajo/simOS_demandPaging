#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include "simos.h"


//======================================================================
// This module handles swap space management.
// It has the simulated disk and swamp manager.
// First part is for the simulated disk to read/write pages.
//======================================================================

#define swapFname "swap.disk"
#define itemPerLine 8
int diskfd;
int swapspaceSize;
int PswapSize;
int pagedataSize;

sem_t swap_semaq;
sem_t swapq_mutex;
sem_t disk_mutex;

//===================================================
// This is the simulated disk, including disk read, write, dump.
// The unit is a page
//===================================================
// each process has a fix-sized swap space, its page count starts from 0
// first 2 processes: OS=0, idle=1, have no swap space
// OS frequently (like Linux) runs on physical memory address (fixed locations)
// virtual memory is too expensive and unnecessary for OS => no swap needed

int read_swap_page (int pid, int page, unsigned *buf)
{ 
  // reference the previous code for this part
  // but previous code was not fully completed
}

int write_swap_page (int pid, int page, unsigned *buf)
{ 
  // reference the previous code for this part
  // but previous code was not fully completed
}

int dump_process_swap_page (int pid, int page)
{ 
  // reference the previous code for this part
  // but previous code was not fully completed
}

void dump_process_swap (int pid)
{ int j;

  printf ("****** Dump swap pages for process %d\n", pid);
  for (j=0; j<maxPpages; j++) dump_process_swap_page (pid, j);
}

// open the file with the swap space size, initialize content to 0
void initialize_swap_space ()
{ int ret, i, j, k;
  int buf[pageSize];

  swapspaceSize = maxProcess*maxPpages*pageSize*dataSize;
  PswapSize = maxPpages*pageSize*dataSize;
  pagedataSize = pageSize*dataSize;

  diskfd = open (swapFname, O_RDWR | O_CREAT, 0600);
  if (diskfd < 0) { perror ("Error open: "); exit (-1); }
  ret = lseek (diskfd, swapspaceSize, SEEK_SET); 
  if (ret < 0) { perror ("Error lseek in open: "); exit (-1); }
  for (i=2; i<maxProcess; i++)
    for (j=0; j<maxPpages; j++)
    { for (k=0; k<pageSize; k++) buf[k]=0;
      write_swap_page (i, j, buf);
    }
    // last parameter is the origin, offset from the origin, which can be:
    // SEEK_SET: 0, SEEK_CUR: from current position, SEEK_END: from eof
}


//===================================================
// Here is the swap space manager. 
//===================================================
// When a process address to be read/written is not in the memory,
// meory raises a page fault and process it (in kernel mode).
// We implement this by cheating a bit.
// We do not perform context switch right away and switch to OS.
// We simply let OS do the processing.
// OS decides whether there is free memory frame, if so, use one.
// If no free memory, then call select_aged_page to free up memory.
// In either case, proceed to insert the page fault req to swap queue
// to let the swap manager bring in the page
//===================================================

typedef struct SwapQnodeStruct
{ int pid, page, act, finishact;
  unsigned *buf;
  struct SwapQnodeStruct *next;
} SwapQnode;
// pidin, pagein, inbuf: for the page with PF, needs to be brought in
// pidout, pageout, outbuf: for the page to be swapped out
// if there is no page to be swapped out (not dirty), then pidout = nullPid
// inbuf and outbuf are the actual memory page content

SwapQnode *swapQhead = NULL;
SwapQnode *swapQtail = NULL;

void print_one_swapnode (SwapQnode *node)
{ printf ("pid,page=(%d,%d), act,ready=(%d, %d), buf=%x\n", 
           node->pid, node->page, node->act, node->finishact, node->buf);
}

void dump_swapQ ()
{ 
  // dump all the nodes in the swapQ
}

// act can be actRead or actWrite
// finishact indicates what to do after read/write swap disk is done, it can be:
// toReady (send pid back to ready queue), freeBuf: free buf, Both, Nothing
void insert_swapQ (pid, page, buf, act, finishact)
int pid, page, act, finishact;
unsigned *buf;
{ 
}

void *process_swapQ ()
{
  // called as the entry function for the swap thread
}

void start_swap_manager ()
{ 
  // initialize semaphores
  // initialize_swap_space ();
  // create swap thread
}

void end_swap_manager ()
{ 
  // terminate the swap thread 
}


