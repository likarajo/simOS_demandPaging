#include <pthread.h>
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
{ int location, ret, retsize, k;

  if (pid < 2 || pid > maxProcess) 
  { printf ("Error: Incorrect pid for disk read: %d\n", pid); 
    return (-1);
  }
  location = (pid-2) * PswapSize + page*pagedataSize;
  ret = lseek (diskfd, location, SEEK_SET);
  if (ret < 0) perror ("Error lseek in read: \n");
  retsize = read (diskfd, (char *)buf, pagedataSize);
  if (retsize != pagedataSize) 
  { printf ("Error: Disk read returned incorrect size: %d\n", retsize); 
    exit(-1);
  }
  usleep (diskRWtime);
  // reference the previous code for this part
  // but previous code was not fully completed
}

int write_swap_page (int pid, int page, unsigned *buf)
{ int location, ret, retsize;

  if (pid < 2 || pid > maxProcess) 
  { printf ("Error: Incorrect pid for disk write: %d\n", pid); 
    return (-1);
  }
  location = (pid-2) * PswapSize + page*pagedataSize;
  ret = lseek (diskfd, location, SEEK_SET);
  if (ret < 0) perror ("Error lseek in write: \n");
  retsize = write (diskfd, (char *)buf, pagedataSize);
  if (retsize != pagedataSize) 
    { printf ("Error: Disk read returned incorrect size: %d\n", retsize); 
      exit(-1);
    }
  usleep (diskRWtime); 
  // reference the previous code for this part
  // but previous code was not fully completed
}

int dump_process_swap_page (int pid, int page)
{ int location, ret, retsize, k;
  int buf[pageSize];
  int tInstr, tOpcode, tOperand;
  mType *temp = (mType *) malloc (sizeof(mType));

  if (pid < 2 || pid > maxProcess) 
  { printf ("Error: Incorrect pid for disk dump: %d\n", pid); 
    return (-1);
  }
  location = (pid-2) * PswapSize + page*pagedataSize;
  ret = lseek (diskfd, location, SEEK_SET);
  //printf ("loc %d %d %d, size %d\n", pid, page, location, pagedataSize);
  if (ret < 0) perror ("Error lseek in dump: \n");
  retsize = read (diskfd, (char *)buf, pagedataSize);
  if (retsize != pagedataSize) 
  { printf ("Error: Disk dump read incorrect size: %d\n", retsize); 
    exit(-1);
  }
  printf ("Content of process %d page %d:\n", pid, page);
  for (k=0; k<pageSize; k++) {
	  //printf ("%d ", buf[k]);
	  temp->mInstr = buf[k];
	  printf ("0x%016x|%.2f ", buf[k],temp->mData);
  }
  printf ("\n");
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
  for (i=2; i<maxProcess; i++) {
    for (j=0; j<maxPpages; j++)
    { for (k=0; k<pageSize; k++) buf[k]=0;
      write_swap_page (i, j, buf); 
    }
	dump_process_swap(i);
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
{ printf ("pid,page=(%d,%d), act,ready=(%d, %d), buf=%x, buf=%x, buf=%x\n", 
           node->pid, node->page, node->act, node->finishact, node->buf[0], node->buf[1], node->buf[2]); 
  ///printf ("pid,page=(%d,%d), act,ready=(%d, %d), buf=%x,\n", 
           //node->pid, node->page, node->act, node->finishact, node->buf);
}

void dump_swapQ ()
{ SwapQnode *node;

  printf ("******************** Swap Queue Dump\n");
  node = swapQhead;
  while (node != NULL) {
	  print_one_swapnode(node);
	  node = node->next;
  }
  // dump all the nodes in the swapQ
  printf ("\n");
}

void process_one_swapQ() 
{ SwapQnode *node; 
  int i, frame, ret;
  mType *buf = (mType *) malloc (pageSize*sizeof(mType));

  sem_wait(&swap_semaq);
  sem_wait(&swapq_mutex);

  if (swapQhead == NULL)
  { printf ("No process in swap queue!!!\n"); 
    sem_post(&swapq_mutex); //sem_post(&swap_semaq);
  }
  else {
	  node = swapQhead;
	  printf ("HANDLING: pid,page=(%d,%d), act,ready=(%d, %d), buf=%x,\n", 
           node->pid, node->page, node->act, node->finishact, node->buf);
	  if (node->act == actWrite) { //write to swapDisk from memory
		  write_swap_page(node->pid, node->page, node->buf);
		  for (i=0;i<pageSize;i++) {
			  buf[i].mInstr = (int)node->buf[i];
			  if (PCB[node->pid]->dataOffset <= (node->page*pageSize+i)) {
				  printf("Data: 0x%016x %f \n", buf[i], buf[i].mData);
				  // use load data here
				  //load_instruction(&buf[i], node->page, i);
			  }
			  else {
				  printf("Instr: 0x%016x\n", buf[i]);
				  // use load instruction here
				  //load_data(&buf[i], node->page, i);
			  }
		  }
	  }
	  else if (node->act == actRead) { //read from disk, then send to load_data or load_instruction
		  node->buf = (unsigned *) malloc (pageSize*sizeof(unsigned));
		  read_swap_page(node->pid, node->page, node->buf);
		  frame = find_allocated_memory(node->pid, node->page);
		  if (frame < 0) { //if no frame returned, just remove node and exit
			  update_process_pagetable (node->pid, node->page, -1); 
			  if (Debug) printf ("Remove swap queue %d %d\n", node->pid, node->page);
			  swapQhead = node->next;
			  if (swapQhead == NULL) swapQtail = NULL;
			  free (node->buf); free (node);
			  sem_post(&swap_semaq); sem_post(&swapq_mutex);
			  if (swapQhead == NULL) sem_wait(&swap_semaq);
			  return;} 
		  update_frame_info (frame, node->pid, node->page); //maybe put this into loader
		  update_process_pagetable (node->pid, node->page, frame); //maybe put this into loader?
		  for (i=0;i<pageSize;i++) {
			  buf[i].mInstr = (int)node->buf[i];
			  if (PCB[node->pid]->dataOffset <= (node->page*pageSize+i)) {
				  printf("Data: 0x%016x %f \n", buf[i], buf[i].mData);
				  ret=load_data (&buf[i], frame, i);
				  if (ret == mError) { PCB[node->pid]->exeStatus = eError; }
				  // use load data here
				  //load_instruction(&buf[i], node->page, i);
			  }
			  else {
				  printf("Instr: 0x%016x\n", buf[i]);
				  ret=load_instruction (&buf[i], frame, i);
				  if (ret == mError) { PCB[node->pid]->exeStatus = eError; }
				  // use load instruction here
				  //load_data(&buf[i], node->page, i);
			  }
		  }
		  if (node->finishact == toReady) {
			  insert_endWait_process(node->pid);
			  endWait_moveto_ready (); }
	  }
	  
	  
	  if (Debug) printf ("Remove swap queue %d %d\n", node->pid, node->page);
	  swapQhead = node->next;
	  if (swapQhead == NULL) swapQtail = NULL;
      free (node->buf); free (node);
	  sem_post(&swap_semaq); sem_post(&swapq_mutex);
	  if (swapQhead == NULL) sem_wait(&swap_semaq);
  }
}

// act can be actRead or actWrite
// finishact indicates what to do after read/write swap disk is done, it can be:
// toReady (send pid back to ready queue), freeBuf: free buf, Both, Nothing
void insert_swapQ (pid, page, buf, act, finishact)
int pid, page, act, finishact;
unsigned *buf;
{ SwapQnode *node; unsigned *temp = (unsigned *) malloc (pageSize*sizeof(unsigned));
  int i; unsigned temp2;

  sem_wait(&swapq_mutex);
  
  if (Debug) printf ("Insert swap queue %d %d %d %d\n", pid, page, act, finishact);
  
  node = (SwapQnode *) malloc (sizeof (SwapQnode));
  node->pid = pid;
  node->page = page;
  node->act = act;
  node->finishact = finishact;
  //node->buf = buf;
  node->next = NULL;
  if (act == actWrite) {
	  node->buf = (unsigned *) malloc (pageSize*sizeof(unsigned));
	  for (i=0;i<pageSize;i++){
		  printf("0x%016x ",buf[i]);
		  temp2 = buf[i];
		  //temp[i] = temp2;
		  node->buf[i] = temp2;
	  }
  }
  else if (act == actRead) {
	  node->buf = buf;
  }
  //temp = buf;
  //node->buf = temp;
  
  if (swapQhead==NULL) {
	  swapQhead = node; swapQtail=node;
  }
  else {
	  swapQtail->next = node; swapQtail = node; 
  }
  if (Debug) dump_swapQ ();
  sem_post(&swap_semaq);
  sem_post(&swapq_mutex);
  if (swapQhead!=node) sem_wait(&swap_semaq); //so that queue only has one outstanding post;
  
  //process_one_swapQ();  //**comment this out
}

void *process_swapQ ()
{
  while (systemActive) process_one_swapQ ();
  if (Debug) printf ("Swap loop has ended\n");
  // called as the entry function for the swap thread
}

pthread_t swapThread;

void start_swap_manager ()
{ int ret;

  sem_init(&swap_semaq,0,1);
  sem_init(&swapq_mutex,0,1);
  sem_init(&disk_mutex,0,1);
  initialize_swap_space ();
  sem_wait(&swap_semaq);
  printf ("Swap space managr has been activated.\n");
  
  ret = pthread_create (&swapThread, NULL, process_swapQ, NULL);
  if (ret < 0) printf ("Swap thread creation problem\n");
  else printf ("Swap thread has been created successsfully\n");
  
  // initialize semaphores
  // initialize_swap_space ();
  // create swap thread
}

void end_swap_manager ()
{ 
  sem_post(&swap_semaq); sem_post(&swapq_mutex); //**DG
  close (diskfd);
  pthread_join (swapThread, NULL);
  printf ("Swap space managr has terminated successfully.\n");
  // terminate the swap thread 
}


