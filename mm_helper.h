#ifndef __MM_HELPER__
#define __MM_HELPER__

#if LOG_TO_STDERR
#define DebugStr(args...)   fprintf(stderr, args);
#else
#define DebugStr(args...)
#endif

#if HEAP_CHECK
#include <string.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
typedef void handler_t(int);
// record the allocated block only
typedef struct HeapStruct {
  char *bk_head;  // block head
  char *bk_tail;  // block tail
  char *pl_head;  // payload head
  char *pl_tail;  // payload tail, pl_tail is possible to equal to bk_tail
  size_t bk_size; // block size
  size_t pl_size; // payload size;
  int index;
  struct HeapStruct *next;
} HeapStruct;

static HeapStruct *alloc_list = NULL;
static HeapStruct *free_list = NULL;

static int writable = 1;

void add_to_alloc_list(const void *ptr, const size_t pl_size, const size_t bk_size);
void delete_from_alloc_list(const void *ptr);
HeapStruct* delete_from_list(HeapStruct *list, const void *ptr);
HeapStruct* search_list(const HeapStruct *list, const void *ptr);
int addr_is_allocated(const char *addr);
int addr_is_payload(const char *addr);
int within_heap(const void *addr);
void show_heap(void);
void show_alloc_list(void);
handler_t *Signal(int signum, handler_t *handler);
void print_stack_trace(int signum);
void to_hex_str(size_t num, int sep);
void to_binary_str(size_t num, int sep);
size_t heap_size(void);
int segregated_free_list_valid(void);
#endif

const size_t CURR_ALLOC = (1 << 0);
const size_t PREV_ALLOC = (1 << 1);
const size_t SIZE_MASK = (~(ALIGNMENT-1));

inline static size_t align(size_t size) {
  return (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1));
}

inline static size_t align_with_min_bk_size(size_t size) {
  return (((size) + (MIN_BK_SIZE-1)) & ~(MIN_BK_SIZE-1));
}

inline static size_t align_chunksize(size_t size) {
  return ((size + (CHUNKSIZE-1)) / CHUNKSIZE) * (CHUNKSIZE);
}

inline static size_t align_realloc_chunksize(size_t size) {
  return (size + (REALLOC_CHUNKSIZE - 1)) / (REALLOC_CHUNKSIZE) * (REALLOC_CHUNKSIZE);
}

inline static int is_align(size_t size) {
  return !(size & (ALIGNMENT-1));
}

inline static int is_align_with_min_bk_size(size_t size) {
  return !(size & (MIN_BK_SIZE-1));
}

inline static int is_align_with_chunksize(size_t size) {
  return !(size % CHUNKSIZE);
}

inline static int is_align_with_rechunksize(size_t size) {
  return !(size % REALLOC_CHUNKSIZE);
}

inline static uint32_t read_word(uint32_t *p) {
#if HEAP_CHECK
  if (!within_heap(p)) {
    DebugStr("%x lies outside heap [%x, %x)\n", p, heap_head, heap_tail);
    abort();
  }
#endif
  return *p;
}

inline static void write_word(uint32_t *p, uint32_t val) {
#if HEAP_CHECK
  if (!within_heap(p)) {
    DebugStr("%x lies outside heap [%x, %x)\n", p, heap_head, heap_tail);
    abort();
  }

  if (addr_is_payload(p)) {
    DebugStr("%x is inside payload\n", p);
    abort();
  }
#endif
  *p = val;
}

inline static void set_alloc_bit(uint32_t *p) {
  *p |= CURR_ALLOC;
}

inline static void clr_alloc_bit(uint32_t *p) {
  *p &= ~CURR_ALLOC;
}

inline static void set_prev_alloc_bit(uint32_t *p) {
  *p |= PREV_ALLOC;
}

inline static void clr_prev_alloc_bit(uint32_t *p) {
  *p &= ~PREV_ALLOC;
}

inline static size_t pack(size_t size, size_t val) {
  return (size | val);
}

inline static void set_size(uint32_t *p, size_t size) {
  *p = (((*p) & ~(SIZE_MASK)) | size);
}

inline static size_t get_size(uint32_t *hdrp) {
  return ((*hdrp) & SIZE_MASK);
}

inline static size_t get_alloc(uint32_t *hdrp) {
  return ((*hdrp) & CURR_ALLOC);
}

inline static size_t get_prev_alloc(uint32_t *hdrp) {
  return ((*hdrp) & PREV_ALLOC);
}

inline static void *get_hdrp(void *ptr) {
  return ((char*)ptr - WSIZE);
}

inline static void *get_ftrp(void *ptr) {
  return (((char*)ptr - DSIZE) + get_size(get_hdrp(ptr)));
}

inline static void *next_ptr(void *hdrp) {
  return ((char*)hdrp + DSIZE);
}

inline static void *prev_ptr(void *hdrp) {
  return ((char*)hdrp + WSIZE);
}

inline static void *get_prev_ptr(void *hdrp) {
  return (void*)(*((uint32_t*)prev_ptr(hdrp)));
}

inline static void *get_next_ptr(void *hdrp) {
  return (void*)(*(uint32_t*)next_ptr(hdrp));
}

inline static void set_prev_ptr(void *hdrp, void *ptr) {
  *(uint32_t*)(prev_ptr(hdrp)) = ptr;
}

inline static void set_next_ptr(void *hdrp, void *ptr) {
  *(uint32_t*)(next_ptr(hdrp)) = ptr;
}

#endif
