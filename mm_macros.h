#ifndef __MM_MACROS_H__
#define __MM_MACROS_H__

#define NUM_STACK_TRACE     20
#define ALIGNMENT           8
#define MIN_BK_SIZE         16
#define WSIZE               4
#define DSIZE               8
#define CHUNKSIZE           176
#define REALLOC_CHUNKSIZE   304

#define ALIGN(size)                           (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define ALIGN_WITH_MIN_BK_SIZE(size)          (((size) + (MIN_BK_SIZE-1)) & ~(MIN_BK_SIZE-1))
#define IS_ALIGN(size)                        (!(size & (ALIGNMENT - 1)))
#define IS_ALIGN_WITH_MIN_BK_SIZE(size)       (!(size & (MIN_BK_SIZE-1)))
#define IS_ALIGN_WITH_CHUNKSIZE(size)         (!(size & (CHUNKSIZE-1)))

#define ALIGN_CHUNKSIZE(size)                 (((size) + (CHUNKSIZE-1)) / (CHUNKSIZE) * (CHUNKSIZE))
#define ALIGN_RECHUNKSIZE(size)               (((size) + (REALLOC_CHUNKSIZE-1)) / (REALLOC_CHUNKSIZE) * (REALLOC_CHUNKSIZE))

#ifdef __HEAP_CHECK__
uint32_t READ_WORD(uint32_t *p) {
  if (!within_heap(p)) {
    fprintf(stderr, "%x lies outside heap [%x, %x)\n", p, heap_head, heap_tail);
    abort();
  }
  return *p;
}
void WRITE_WORD(uint32_t *p, uint32_t val) {
  if (!writable) {
    fprintf(stderr, "current function is not writable\n");
    abort();
  }

  if (!within_heap(p)) {
    fprintf(stderr, "%x lies outside heap [%x, %x)\n", p, heap_head, heap_tail);
    abort();
  }

  if (addr_is_payload(p)) {
    fprintf(stderr, "%x is inside payload\n", p);
    abort();
  }
  *p = val;
}
#else
#define READ_WORD(p)                                (*(uint32_t*)(p))
#define WRITE_WORD(p, val)                          (*(uint32_t*)(p) = (val))
#endif

#define CURR_ALLOC                                  (1 << 0)
#define PREV_ALLOC                                  (1 << 1)
#define SET_CURR_ALLOC_BIT(p)                       (WRITE_WORD(p, READ_WORD(p) | CURR_ALLOC))
#define CLR_CURR_ALLOC_BIT(p)                       (WRITE_WORD(p, READ_WORD(p) & ~CURR_ALLOC))
#define SET_PREV_ALLOC_BIT(p)                       (WRITE_WORD(p, READ_WORD(p) | PREV_ALLOC))
#define CLR_PREV_ALLOC_BIT(p)                       (WRITE_WORD(p, READ_WORD(p) & ~PREV_ALLOC))
#define PACK(size, alloc)                           ((size) | (alloc))
#define SIZE_MASK                                   (~(ALIGNMENT - 1))
#define SET_SIZE(hdrp, size)                        (WRITE_WORD(hdrp, (READ_WORD(hdrp) & (ALIGNMENT-1)) | size))
#define GET_SIZE(hdrp)                              (READ_WORD(hdrp) & SIZE_MASK)  // get block size from hdrp
#define GET_ALLOC(hdrp)                             (READ_WORD(hdrp) & 0x1)  // get alloc bit from hdrp
#define GET_PREV_ALLOC(hdrp)                        (READ_WORD(hdrp) & 0x2)  // get alloc bit of previous block
#define HDRP_USE_PLDP(pldp)                         ((char*)(pldp) - WSIZE)  // get hdrp using pldp
#define FTRP_USE_PLDP(pldp)                         ((char*)(pldp) + GET_SIZE(HDRP_USE_PLDP(pldp)) - DSIZE)  // get ftrp using pldp

#define PREV_PTR(hdrp)                              ((char*)hdrp + WSIZE)
#define NEXT_PTR(hdrp)                              ((char*)hdrp + DSIZE)
#define GET_PREV_PTR(hdrp)                          (READ_WORD(PREV_PTR(hdrp)))
#define GET_NEXT_PTR(hdrp)                          (READ_WORD(NEXT_PTR(hdrp)))
#define SET_PREV_PTR(hdrp, prev_ptr)                (WRITE_WORD(PREV_PTR(hdrp), prev_ptr))
#define SET_NEXT_PTR(hdrp, next_ptr)                (WRITE_WORD(NEXT_PTR(hdrp), next_ptr))

#endif
