#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


/* CMPT 332 GROUP 06 Change, Fall 2016 */
/* List, ListNode, freeNodes, and prioQs structs */

typedef struct listNode {
	struct proc* process;
	struct listNode* next;
} listNode;

typedef struct list { 
	listNode* head;
	listNode* tail;
	int count;
} list;  

struct {
	/* Array of available nodes */
	listNode actualNodes[NPROC];
	listNode* nodePointers[NPROC];
	int cursor; 
} freeNodes; 

/* Array of priority queues with spinlock */
struct {
	list prioArr[5];
	struct spinlock prioQsLock;
} prioQs;

/* Initialize freeNodes array to the pointers of the actual nodes */
void nodeInit() {
	int i;	
	for (i = 0; i < NPROC; i++) {
		freeNodes.nodePointers[i] = &(freeNodes.actualNodes[i]);
	}	
}


/* Process table struct, has a lock and array of processes */
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


/* CMPT 332 GROUP 06 Add, Fall 2016 */
/* enqueue():
 * Puts process on a priority queue 
 */
int enqueue(int pid) {
	
	/*cprintf("Got to enqueue function\n");*/
	
	int prio;
	listNode *new;
	struct proc *p;	
	
	/* If there are no more available nodes */
	if (freeNodes.cursor == NPROC) {
		cprintf("No more available nodes\n");
		return -1;
	}
	
	acquire(&prioQs.prioQsLock);
	
	/* Find the desired process in page table */	
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if (p->pid == pid) { 
			prio = p->priority;
			break; 
		}
	}
	
	/* Check if current process is already on the queue */
	if (p->inQ == 1) {
		cprintf("Proc already in the queue\n");
		release(&prioQs.prioQsLock);
		return -1;
	}
	
	/*cprintf("pid of p is %d\n", p->pid);
	cprintf("prio of p is %d\n", p->priority);*/
	
	/* Get a node from the freeNodes array */
	new = freeNodes.nodePointers[freeNodes.cursor];
	freeNodes.cursor++;
	new->process = p;	
	prio = p->priority;
	p->inQ = 1;
	
	/* If the priority queue is empty */
	if (prioQs.prioArr[prio].count == 0) {
		prioQs.prioArr[prio].head = new;
		prioQs.prioArr[prio].tail = new;
		new->next = 0;
	}
	/* If the queue is populated */
	else{
		prioQs.prioArr[prio].tail->next = new;
		prioQs.prioArr[prio].tail = new;
		new->next = 0;
	}
	
	prioQs.prioArr[prio].count++;
	release(&prioQs.prioQsLock);
	return 0;
}

/* dequeue():
 * Removes a process from a priority queue 
 */
struct proc* dequeue(int prio) {
	
	/*cprintf("Got to dequeue function\n");*/
	
	struct proc* p;
	
	acquire(&prioQs.prioQsLock);
	
	/* Check if the priority queue is empty */
	if (prioQs.prioArr[prio].count == 0) {
		cprintf("Queue is empty\n");
		release(&prioQs.prioQsLock);
		return 0;
	}
	
	/* Take the head off the queue and readjust it */
	freeNodes.cursor--;
	freeNodes.nodePointers[freeNodes.cursor] = prioQs.prioArr[prio].head;
	
	p = prioQs.prioArr[prio].head->process;
	/*cprintf("p's pid is %d, priority is %d, inQ = %d\n", p->pid, p->priority, p->inQ);*/
	prioQs.prioArr[prio].head = prioQs.prioArr[prio].head->next;
	prioQs.prioArr[prio].count--;
	p->inQ = 0;
	
	if (prioQs.prioArr[prio].count == 0) {
		prioQs.prioArr[prio].tail = 0;		
	}

	release(&prioQs.prioQsLock);
	return p;
}




static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  
  /* CMPT 332 GROUP 06 Add, Fall 2016 */
  p->inQ = 0;
  if (p->parent != 0) {
	p->priority = p->parent->priority;
  }
  else {
	  p->priority = 0;
  }
  
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  
  /* CMPT 332 GROUP 06 Change, Fall 2016 */
  enqueue(p->pid);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
  
  /* CMPT 332 GROUP 06 Change, Fall 2016 */
  /* Setting priority to parent's priority */
  np->priority = np->parent->priority; 

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  
  np->state = RUNNABLE;
  
  /* CMPT 332 GROUP 06 Change, Fall 2016 */
  enqueue(np->pid);
  
  release(&ptable.lock);
  
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
	  release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
	/* Define process struct p */  
	struct proc *p;
	int i;

	for(;;){
		// Enable interrupts on this processor.
		sti();
		acquire(&ptable.lock);
		
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->state == RUNNABLE && p->inQ == 0) {
				enqueue(p->pid);
				cprintf("pid of enqueued proc: %d\n", p->pid);
			}
		}		
		
		/* Take current process off the queue by searching the 
		 * priority queues and getting the highest one that is not
		 * empty. Then, pop the first item off.  	
		 */
		for (i = 0; i < 5; i++) {
			
			if (prioQs.prioArr[i].count != 0) {
				/*cprintf("Count of priority queue #%d is %d\n", i, prioQs.prioArr[i].count);
				 Free the node */
				p = dequeue(i);				
					
				// Switch to chosen process.  It is the process's job
				// to release ptable.lock and then reacquire it
				// before jumping back to us. 
				proc = p;
				switchuvm(p);
				p->state = RUNNING;
				swtch(&cpu->scheduler, proc->context);
				switchkvm();

				// Process is done running for now.
				// It should have changed its p->state before coming back.
				proc = 0;						
			}				
		}
		release(&ptable.lock);
	}	
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  
  /* CMPT 332 GROUP 06 Change, Fall 2016 */
  /* Where does the scheduler get called from here??? */
  enqueue(proc->pid);
  
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
	  
	  /* CMPT 332 GROUP 06 Change, Fall 2016 */
	  enqueue(p->pid);
	}
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
		
		/* CMPT 332 GROUP 06 Change, Fall 2016 */
	    enqueue(p->pid);
	  }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


/* CMPT 332 GROUP 06 Change, Fall 2016 */
/* Functions in kernel space yo */
int nice(int incr) {
	
	struct proc *p;
	int prio;
	
	/* Aquire the locks */
	acquire(&ptable.lock);
	
	/* Find the currently running process in page table */	
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if (p->state == RUNNING) {
			prio = p->priority;
			break; 
		}
	}
	
	if (prio + incr > 4 || prio + incr < 0) {
		cprintf("Increment puts priority out of range\n");
		return -1;
	}
	
	p->priority = prio + incr;
	
	release(&ptable.lock);
    
	return 0;
}

int getpriority (int pid) {
	
	struct proc *p;
	int prio;
	
	/* Aquire the ptable lock */
	acquire(&ptable.lock);
	
	/* Find the desired process in page table and return its priority*/	
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if (p->pid == pid) {
			prio = p->priority;
			break; 
		}
	}
	
	release(&ptable.lock);
	
	/* If process not found, return -1 */
	if (p->pid != pid){
		cprintf("Process not found");
		return -1;
	}
	
    return prio;
}


/* CMPT 332 GROUP 06 Add, Fall 2016 */
/* setpriority():
 * Takes the desired process off of its current priority queue 
 * and places it on a different one
 */
int setpriority (int pid, int newPriority) { 

	struct proc *p;
	int oldPrio;
	listNode* prev;
	listNode* curr;	
	
	/* Aquire the ptable lock */
	acquire(&ptable.lock);
	
	/* Find the desired process in page table */	
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if (p->pid == pid) {
			oldPrio = p->priority;
			p->priority = newPriority;
			break; 
		}
	}
	
	release(&ptable.lock);
	
	cprintf("Old priority is %d\n", oldPrio);
	
	/* Aquire the prioQsLock */	
	acquire(&prioQs.prioQsLock);
	
	/* If old queue is empty */
	if(prioQs.prioArr[oldPrio].count == 0) {
		cprintf("Old priority queue is empty\n");
		return -1;
	}
	
	/* If process not found, return -1 */
	if (p->pid != pid){
		cprintf("Process not found\n");
		return -1;
	}
	
	/* Track curr's previous node */
	prev = 0;
	/* Set curr to the head of the old priority queue */
	curr = prioQs.prioArr[oldPrio].head;
	 
	/* Find the process in its old queue and move it to its new queue */
	while (curr != 0) { 
		
		/* If process is found */
		if (curr->process->pid == pid) {
			
			/* If the curr is the only process in the old queue */
			if(prioQs.prioArr[oldPrio].count == 1) {				
				/* Adjust old priority queue */
				prioQs.prioArr[oldPrio].head = 0;
				prioQs.prioArr[oldPrio].tail = 0;				
			}
			
			/* Else if curr is the head of the old priority queue */
			else if (curr == prioQs.prioArr[oldPrio].head) {
				 prioQs.prioArr[oldPrio].head = curr->next;
			}
			
			/* Else if curr is the tail of the old priority queue */
			else if (curr == prioQs.prioArr[oldPrio].tail) {
				 prioQs.prioArr[oldPrio].tail = prev;
				 prev->next = 0;
			}
			
			/* Else curr is in the middle of old priority queue */
			else{
				prev->next = curr->next;
			}
			release(&prioQs.prioQsLock);
			
			/* Add curr to new priority queue */
			enqueue(pid);
			
			acquire(&prioQs.prioQsLock);
			
			/* Decrement the count of the old priority queue */
			prioQs.prioArr[oldPrio].count -= 1;
			
			break;
		}
		
		/* Move to next node */
		prev = curr;
		curr = curr->next;
	}
	
	release(&prioQs.prioQsLock);
	
	cprintf("Old priority is %d\n", newPriority);
	
	return 0;
}



/* CMPT 332 GROUP 06 Change, Fall 2016 */
void create_kernel_process(const char *name, void (*entrypoint)()){
    
  struct proc *p;
 
   // Allocate process.
  if((p = allocproc()) == 0)
      panic("allocproc really messed up good");

  p->context->eip = (uint)entrypoint;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");

  //p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));

  safestrcpy(p->name, name, sizeof(p->name));
  //p->cwd = namei("/");
  acquire(&ptable.lock);               
  p->state = RUNNABLE;
  release(&ptable.lock);
  
  /*CONCH :
   Add this to the queue right here.*/
}

/* CMPT 332 GROUP 06 Change, Fall 2016 */

/*Implement clock algorithm here*/
void swap_in(void){
    //inable inerrupts
    sti();
    //release(&ptable.lock);
   cprintf("Kernel process successfully started execution.\n");
    for(;;){
        
        if(0){
            cprintf("yo b");
        }
    }
       
    
    return;
}


/* CMPT 332 GROUP 06 Change, Fall 2016 */
void swap_out(void){
    
   //implement swap out function here
    
    
    return;
}
