#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define MAX_VARIABLES 32 
#define MAX_VAL_CHARS 128



char keys [MAX_VARIABLES][MAX_VARIABLES];
char values [MAX_VARIABLES][MAX_VAL_CHARS];
int activate=-1;

  
struct spinlock vars_lock;


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int set_priority(int priority){
  struct proc *curproc = myproc();
  if(priority<=0||priority>3)
  {
    return-1;
  }
  else{
    switch (priority){
      case 1:{
        curproc->decay_factor = 0.75;
        break;
      }
      case 2:{
        curproc->decay_factor = 1;
        break;
      }
      case 3:{
        curproc->decay_factor = 1.25;
        break;
      }
    }
  return 0;
  }
}

void update_proces_timers(void)
{
  acquire(&ptable.lock);
  struct proc *p;

  
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state==RUNNING)
      {
        p->rtime++;
      }
      if(p->state==SLEEPING)
      {
        
        p->iotime++;
      }
  }
  release(&ptable.lock);
}

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

int strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

//extend wait but wait for a specific pid
int wait2(int pid,int* wtime,int* rtime,int* iotime)
{
  struct proc *p;
  int havekids;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        //waitime = endtime - createtime - iotime - runtime
        continue;
      havekids = 1;
      //if son finish to run
      if(p->pid==pid && p->state == ZOMBIE){
        // Found one.
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        *rtime=p->rtime;
        *iotime=p->iotime;
        *wtime=p->etime - p->ctime - p->iotime - p->rtime;
        
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}



int setVariable(char* variable, char* value){
    //if didn't init yet
  acquire(&vars_lock);
    int i;
    if(activate==-1){
        for(i=0;i<MAX_VARIABLES; i++){
            memset(keys[i],0,32);
            memset(values[i],0,128);   
        }
        activate=0;
    }
    
    
    //check legal variable
    // value cant get '\n'
    if(*variable==0 || *value==0||*value=='\n'){ return -2;}
    
    for(i=0; variable[i]!=0; i++){
        if ((variable[i] >='a'&&variable[i]<='z')||(variable[i]<='Z'&&variable[i]>='A')){
            continue;
        }
        else
        {   release(&vars_lock);
            return -2;
          }
    }
    
    //check if key is exist
    for(i=0; keys[i][0]!=0; i++){
        if(strcmp(keys[i],variable)==0){
            safestrcpy(values[i],value ,MAX_VAL_CHARS+1); 
            release(&vars_lock);
            return 0;
        }
    }
    
    //check for room
    int j=0;
    while(j<MAX_VARIABLES&&keys[j][0]!=0)
    {j++;}

    //No room for additional variable
    
    if(j>=MAX_VARIABLES)
      {     release(&vars_lock);
            return -1;
      }
    
    safestrcpy(keys[j],variable, MAX_VARIABLES+1);
    safestrcpy(values[j],value ,MAX_VAL_CHARS+1);
    int tmp=0;
    for(;tmp<MAX_VAL_CHARS;tmp++)//remove newline its bad when doing subtition.
    {
        if(values[j][tmp]=='\n'&&values[j][tmp+1]==0)
        {
            values[j][tmp]=0;
        }
    
    } 
    release(&vars_lock); 
    return 0;   
}

int getVariable(char* variable, char* value){
    int i;
    acquire(&vars_lock);
    for(i=0; i<MAX_VARIABLES; i++){
        if(keys[i][0]!=0){
        
        if(strcmp(keys[i],variable)==0)
        {//found
            memset(value,0,MAX_VAL_CHARS);
            safestrcpy(value,values[i] ,MAX_VAL_CHARS);
            release(&vars_lock);
            return 0;
        }
        }
    }
    release(&vars_lock);
    return -1;
    
}
int remVariable(char* variable){
    int i;
    acquire(&vars_lock);
    for(i=0; keys[i][0]!=0; i++){
        if(strcmp(keys[i],variable)==0){
            memset(keys[i],0,MAX_VARIABLES);
            memset(values[i],0,MAX_VAL_CHARS);
            //Variable removed successfully
            release(&vars_lock);
            return 0;
        }
    }

    //No variable with the given name
    //cprintf("no variable with name was found\n %s",variable);
    release(&vars_lock);
    return -1;
    
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&vars_lock,"variables");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
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
  
  p->decay_factor=1.0;
  
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->ctime=ticks;
  p->iotime=0;
  p->rtime=0;
  p->decay_factor=1;
  p->state = RUNNABLE;
//init 0 

  #ifdef FCFS 
  p->entrytime=ticks;
  #else
  #ifdef SRT
  p->aprox_time=QUANTUM;
  #endif
  #endif


  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n){
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;
    
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->ctime=ticks;//creation time
  np->iotime=0;
  np->rtime=0;
  np->decay_factor=curproc->decay_factor;
  #ifdef SRT
  curproc->aprox_time=QUANTUM;
  #endif

  //to son - the new process
  np->state = RUNNABLE;
  #ifdef FCFS 
  np->entrytime=ticks;
  #else
  #ifdef SRT
  np->aprox_time=QUANTUM;
  #endif
  #endif
  np->decay_factor=curproc->decay_factor;


  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);
  
  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

 
  // Jump into the scheduler, never to return.
  curproc->rtime+=(ticks- curproc->curr_rtime);
  curproc->state = ZOMBIE;

  curproc->etime=ticks;//endtime Task2
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
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      //if son finish to run
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
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

#ifdef DEFAULT
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->curr_rtime = ticks;
      p->qtime = ticks;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
#else
#ifdef FCFS
void
scheduler(void)
{
  struct proc *p;
  struct proc *chosen_proc=0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    //task 3
    int min = 0;
    int found_flag = 1;
    //    panic("FCFS");
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      else if(p->entrytime < min || found_flag==1){
        found_flag=0;
        min=p->entrytime;
        chosen_proc=p;
      }
    }
    //if didn't find 
    if (found_flag!=1){

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = chosen_proc;
      switchuvm(chosen_proc);
      chosen_proc->state = RUNNING;
      chosen_proc->curr_rtime = ticks;
      chosen_proc->qtime = ticks;
      swtch(&(c->scheduler), chosen_proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      found_flag=1;
      }
    release(&ptable.lock);

  }
}
#else
#ifdef SRT  
void
scheduler(void)
{
  struct proc *p;
  struct proc *chosen_proc=0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    //task 3
    int min = 0;
    int found_flag = 1;
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      else if(p->aprox_time < min || found_flag==1){
        found_flag=0;
        min=p->aprox_time;
        chosen_proc=p;
      }
    }
    //if find PROCESS
    if (found_flag!=1){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = chosen_proc;
      switchuvm(chosen_proc);
      chosen_proc->state = RUNNING;
      chosen_proc->curr_rtime = ticks;
      chosen_proc->qtime = ticks;
      swtch(&(c->scheduler), chosen_proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      found_flag=1;

      }
    release(&ptable.lock);

  }
}
#else

#ifdef CFSD
void
scheduler(void)
{
  struct proc *p;
  struct proc *chosen_proc=0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    //task 3
    float min = 0;
    int found_flag = 1;
    //    panic("FCFS");
    // Loop over process table looking for process to run.

    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      else
        {
        float tmpr_time=ticks-p->ctime;
        float waiting_time=ticks-p->ctime-p->iotime-p->rtime;
        if((tmpr_time*p->decay_factor)/(tmpr_time+waiting_time)< min || found_flag==1)
        {
        min=((tmpr_time*p->decay_factor)/(tmpr_time+waiting_time));
        found_flag=0;
        chosen_proc=p;
      }
    }
    }
    //if didn't find 
    if (found_flag!=1){

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = chosen_proc;
      switchuvm(chosen_proc);
      chosen_proc->state = RUNNING;
      chosen_proc->curr_rtime=ticks;
      chosen_proc->qtime = ticks;
      swtch(&(c->scheduler), chosen_proc->context);
      switchkvm();

      found_flag=1;
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      found_flag=1;

      }
    release(&ptable.lock);

  }
}
#endif
#endif
#endif
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  p->rtime +=(ticks- p->curr_rtime);
  p->state = RUNNABLE;
  #ifdef FCFS
  p->entrytime=ticks;
  #else
  #ifdef SRT
  if(p->rtime>=p->aprox_time)
  {
    p->aprox_time=p->aprox_time+ALPHA*p->aprox_time;
  }
  #endif
  #endif
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
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->rtime +=(ticks- p->curr_rtime);
  p->state = SLEEPING;
  p->sleeptime = ticks;
  #ifdef SRT
  if(p->rtime>=p->aprox_time)
  {
    p->aprox_time=p->aprox_time+ALPHA*p->aprox_time;
  }
  #endif
  sched();

  // Tidy up.
  p->chan = 0;

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
    if(p->state == SLEEPING && p->chan == chan)
     { p->state = RUNNABLE;
      p->iotime += (ticks - p->sleeptime);
      #ifdef FCFS 
       p->entrytime=ticks;
       #endif
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
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        #ifdef FCFS 
        p->entrytime=ticks;
        #endif
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
