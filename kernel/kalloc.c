// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
    struct spinlock lock;
    int arr[NPAGE];
} refcr;

inline void
refinc(uint64 pa) {
    acquire(&refcr.lock);
    ++refcr.arr[INDEX(pa)];
    release(&refcr.lock);
}
inline void
refdes(uint64 pa) {
    acquire(&refcr.lock);
    --refcr.arr[INDEX(pa)];
    release(&refcr.lock);
}
inline void
refset(uint64 pa, int n) {
    acquire(&refcr.lock);
    refcr.arr[INDEX(pa)] = n;
    release(&refcr.lock);
}
inline uint64
refget(uint64 pa) {
    uint64 ref;
    acquire(&refcr.lock);
    ref = refcr.arr[INDEX(pa)];
    release(&refcr.lock);
    return ref;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  memset(refcr.arr, 0, NPAGE);
  initlock(&refcr.lock, "refcr");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
   if ((int)refget((uint64)pa) > 1) {
        refdes((uint64)pa);
        return;
    }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  refset((uint64)pa, 0);
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
        kmem.freelist = r->next;
        refset((uint64)r, 1);
    }
   release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
