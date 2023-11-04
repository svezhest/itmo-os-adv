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

#define rc_start (uint64)end
#define rc_end rc_start + (uint64)((PHYSTOP - rc_start + 1) >> 12)

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct ref_counter {
    uint8 *counter;
    struct spinlock lock;
} rc;

inline uint64 pind(void *pa) {
    return ((uint64)pa - rc_start) >> 12;
}

void inc_ref(void *pa) {
    if ((uint64)pa < rc_end || (uint64)pa >= PHYSTOP) {
        panic("inc_ref: incorrect address");
    }
    acquire(&rc.lock);
    ++rc.counter[pind(pa)];
    release(&rc.lock);
}

void dec_ref(void *pa) {
    if ((uint64)pa < rc_end || (uint64)pa >= PHYSTOP) {
        panic("dec_ref: incorrect address");
    }
    acquire(&rc.lock);
    if (--rc.counter[pind(pa)] == 0) {
        release(&rc.lock);
        kfree(pa);
    } else {
        release(&rc.lock);
    }
}

uint get_ref_num(void *pa) {
    if ((uint64)pa < rc_end || (uint64)pa >= PHYSTOP) {
        panic("get_ref_num: incorrect address");
    }
    return rc.counter[pind(pa)];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&rc.lock, "ref counter");
  rc.counter = (uint8 *) end;
  freerange((void *)rc_end, (void*)PHYSTOP);
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

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < rc_end || (uint64)pa >= PHYSTOP) {
      panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

    acquire(&rc.lock);
    rc.counter[pind(pa)] = 0;
    release(&rc.lock);

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
      acquire(&rc.lock);
      rc.counter[pind(r)] = 1;
      release(&rc.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}