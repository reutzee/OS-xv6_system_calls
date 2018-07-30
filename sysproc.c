#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
int sys_set_priority(void){
  int priority;
  argint(0,&priority);
  return set_priority(priority);
}
int sys_setVariable(void){ //key val
    char* key;
    char* val;
    argstr(0,&key);
    argstr(1,&val);
    return setVariable(key,val);
}

int sys_getVariable(void){   //key,val
    char* key;
    char* val;
    argstr(0,&key);
    argstr(1,&val);
    
    return getVariable(key,val);
}

int sys_remVariable(void){ //key to remove
    char* key;
    argstr(0,&key);
    return remVariable(key);
}

int sys_wait2(void)//      wait2(int pid,int* wtime,int* rtime,int* iotime);
{
  int pid;
  int* wtime;
  int* rtime;
  int* iotime;
  argint(0,&pid);
  argint(1,(int*)&wtime);
  argint(2,(int*)&rtime);
  argint(3,(int*)&iotime);
return wait2(pid,wtime,rtime,iotime);
}


int sys_yield(void)
{
  yield(); 
  return 0;
}

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
