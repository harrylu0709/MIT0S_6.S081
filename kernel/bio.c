// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//https://zhuanlan.zhihu.com/p/609771386

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"



struct {
  // struct spinlock biglock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  struct buf buckets[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

extern uint ticks;

static int
myhash(int x)
{
  return x%NBUCKET;
}


void
binit(void)
{
  struct buf *b;
  for(int i = 0; i < NBUCKET ; i++){
    initlock(&bcache.locks[i], "bcache");
  }
  
  // Create linked list of buffers
  for(int i = 0; i < NBUCKET ; i++){
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].next;
    b->prev = &bcache.buckets[0];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next = b;
  }
}

static void
bufinit(struct buf *b, uint dev, uint blockno)
{
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = myhash(blockno);

  acquire(&bcache.locks[id]);

  // Is the block already cached?
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.locks[id]);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  struct buf *victm = 0;
  uint minticks = ticks;
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->refcnt == 0 && b->lastuse <= minticks){
      minticks = b->lastuse;
      victm = b;
      //return b;
    }
  }

  if(!victm)
    goto steal;

  /** 
   * 直接覆盖待淘汰的buf，无需再将其中的旧内容写回至disk中 
   * 标记位valid置0，为了保证能够读取到最新的数据 
   * */
  bufinit(victm, dev, blockno);
  release(&bcache.locks[id]);
  acquiresleep(&victm->lock);
  return victm;

steal:
  /** 到别的哈希桶挖 buf */

  for(int i = 0; i < NBUCKET; i++){
    if(i == id) 
      continue;

    acquire(&bcache.locks[i]);
    minticks = ticks;
    for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
      if(b->refcnt == 0 && b->lastuse <= minticks){
        minticks = b->lastuse;
        victm = b;
      }
    }

    if(!victm){
      release(&bcache.locks[i]);
      continue;
    }
    bufinit(victm, dev, blockno);

    /** 将 victm 从第 i 号哈希桶中取出来 */

    victm->next->prev = victm->prev;
    victm->prev->next = victm->next;
    release(&bcache.locks[i]);

    /** 将 victm 接入第 id 号中 */

    victm->next = bcache.buckets[id].next;
    bcache.buckets[id].next->prev = victm;
    bcache.buckets[id].next = victm;
    victm->prev = &bcache.buckets[id];
    release(&bcache.locks[id]);
    acquiresleep(&victm->lock);
    return victm;
  }
  release(&bcache.locks[id]);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = myhash(b->blockno);

  acquire(&bcache.locks[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse = ticks;
  }
  
  release(&bcache.locks[id]);
}

void
bpin(struct buf *b) {
  int id = myhash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt++;
  release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
  int id = myhash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt--;
  release(&bcache.locks[id]);
}


