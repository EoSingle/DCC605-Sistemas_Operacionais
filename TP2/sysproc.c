#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

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

int
sys_date(void)
{
  //char *ptr;
  //argptr(0, &ptr, sizeof(struct rtcdate*));
  
  struct rtcdate *r;
  if (argptr(0, (void*)&r, sizeof(struct rtcdate)) < 0)
    return -1;

  cmostime(r);
  
  return 0;
}

static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

int
sys_virt2real(void)
{
  char *va;
  if (argptr(0, &va, sizeof(char *)) < 0)
    return -1;

  pte_t *pte = walkpgdir(myproc()->pgdir, va, 0);
  if (!pte || !(*pte & PTE_P))
    return -1;

  uint pa = PTE_ADDR(*pte) | ((uint)va & 0xFFF);
  return (int)V2P(pa);
}

int
sys_num_pages(void)
{
  struct proc *p = myproc();
  int num_pages = 0;

  for (uint va=0; va < p->sz; va += PGSIZE) {
    pte_t *pte= walkpgdir(p->pgdir, (void *)va, 0);
    if (pte && (*pte & PTE_P)) // Contabiliza apenas as páginas presentes
      num_pages++;
  }

  return num_pages;
}

int
sys_forkcow(void)
{
  return forkcow();
}