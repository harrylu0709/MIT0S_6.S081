#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "sysinfo.h"


uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
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

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int n;

  if(argint(0, &n) < 0)
    return -1;
  // struct proc *p = myproc();
  // p->tracenum = n;
  myproc()->tracenum = n;
  return 0;
}

uint64
sys_sysinfo(void)
{
  struct proc *p = myproc();
  struct sysinfo info;
  uint64 addr; // user pointer to struct stat

  info.nproc= nproc();
  info.freemem = freemem();

  //printf("proc_num=%d\n",proc_num);
  //printf("freemem_bytes=%d\n",freemem_bytes);

  if(argaddr(0, &addr) < 0)
    return -1;

  if(copyout(p->pagetable, addr, (char *)&info, sizeof(struct sysinfo)) < 0)
    return -1;

  return 0;
}
// struct sysinfo {
//   uint64 freemem;   // amount of free memory (bytes)
//   uint64 nproc;     // number of process
// };

// uint64
// sys_fstat(void)
// {
//   struct file *f;
//   uint64 st; // user pointer to struct stat

//   if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
//     return -1;
//   return filestat(f, st);
// }

// // Get metadata about file f.
// // addr is a user virtual address, pointing to a struct stat.
// int
// filestat(struct file *f, uint64 addr)
// {
//   struct proc *p = myproc();
//   struct stat st;
  
//   if(f->type == FD_INODE || f->type == FD_DEVICE){
//     ilock(f->ip);
//     stati(f->ip, &st);
//     iunlock(f->ip);
//     if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
//       return -1;
//     return 0;
//   }
//   return -1;
// }