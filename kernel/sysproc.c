#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 base;  // 起始虚拟地址
  uint64 mask;  // 地址掩码（用于存储结果）
  int len;      // 需要检查的页数

  pagetable_t pagetable = 0;  // 页表
  unsigned int procmask = 0;  // 处理后的页访问标志
  pte_t *pte;  // 页表条目指针

  struct proc *p = myproc();  // 获取当前进程结构体

  // 从系统调用参数中获取 base、len 和 mask 的值
  if(argaddr(0, &base) < 0 || argint(1, &len) < 0 || argaddr(2, &mask) < 0)
    return -1;  // 参数获取失败，返回错误代码 -1

  // 限制 len 的值为 int 类型的位数
  if (len > sizeof(int) * 8) 
    len = sizeof(int) * 8;

  // 遍历每一页
  for(int i = 0; i < len; i++) {
    pagetable = p->pagetable;  // 获取当前进程的页表
      
    // 检查 base 是否超出最大虚拟地址范围
    if(base >= MAXVA)
      panic("pgaccess");

    // 遍历页表的不同层级
    for(int level = 2; level > 0; level--) {
      pte = &pagetable[PX(level, base)];  // 获取页表条目
      if(*pte & PTE_V) {  // 如果页表条目有效
        pagetable = (pagetable_t)PTE2PA(*pte);  // 更新页表为下一层的物理地址
      } else {
        return -1;  // 如果无效，返回错误代码 -1
      }      
    }
    pte = &pagetable[PX(0, base)];  // 获取最底层页表条目
    if(pte == 0)
      return -1;  // 如果页表条目为空，返回错误代码 -1
    if((*pte & PTE_V) == 0)
      return -1;  // 如果页表条目无效，返回错误代码 -1
    if((*pte & PTE_U) == 0)
      return -1;  // 如果页表条目不允许用户访问，返回错误代码 -1
    if(*pte & PTE_A) {  // 如果页表条目被访问
      procmask = procmask | (1L << i);  // 设置对应位表示页被访问
      *pte = *pte & (~PTE_A);  // 清除页表条目的访问位
    }
    base += PGSIZE;  // 移动到下一个页
  }

  pagetable = p->pagetable;  // 恢复原始页表
  // 将 procmask 的结果复制到用户空间指定的地址 mask
  return copyout(pagetable, mask, (char *)&procmask, sizeof(unsigned int));
}

#endif

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
