/*
 * mm.c - The fastest, most memory-efficient malloc package.
 * 
 * -------------------------- OVERVIEW -------------------------------------
 * In this approach, the approach chosen is explicit free list combined with segregated free list for maximized utilization and throughput. 
 * In details, we maintain 17 segregated free lists, each of which is a doubly linked list comprised of free blocks. For example, list1 contains blocks
 * smaller than 8 bytes in size, list2 contains blocks smaller than 16 bytes, etc... In other words, each size class of blocks has its own free list.
 * Each block at least has a header and footer in which header and footer contain size and allocation info (last bit), 
 * and if the block is allocated it also has a pointer to the previous free block and a pointer to the next free block (in the same segregated list). 
 * -------------------------- OVERVIEW ------------------------------------
 
 ----------------------- A visualization of the block organization --------------------------------
 
 A: Allocated? (1: true, 0: false)
 
 <Allocated Block>
 
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              Size of the block                                       |  |  | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                                                                                               |
            |                                                                                               |
            .                              Payload and padding                                              .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              Size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
 <Free block>
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              Size of the block                                       |     | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        Pointer to its predecessor in segregated list                          |
bp+WSIZE--> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        Pointer to its successor in segregated list                            |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            .                                                                                               .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+


----------------------- A visualization of the block organization --------------------------------


 * ------------- BELOW ARE SOME POLICIES I ADOPT (MORE DETAILS INLINE) -----------------
 * 
 * In order to conform to the alignment requirement (16 bytes), aach of the header, footer, pointer to its predecessor and pointer to its successor 
 * is 8 bytes in size (WSIZE), which means the free block is at least 16 bytes in size, and the allocated block has at least 32 bytes. 
 *
 * In segregated free list, I adopt the first-fit policy to optimize for perfomance. It's basically a trade-off between the memory utilization and
 * throughput, and I decide to choose the first-fit policy when finding the free block. In the worst case, it can take linear time depending on the 
 * total number of blocks in the list (bucket) we are searching. 
 * 
 * About insertion policy, I adopt LIFO, which is simple and constant time but causes worse fragmentation (trade-off again).
 * 
 * About coalescing, immediate coalescing is chosen: when a block is freed, it's immediately coalesced, and the new freed, coalesced block is put into
 * the appropriate class size (bucket) of segregated free lists. 
 *
 * A place for optimizing is mm_realloc. More detailed comments will be at the actual mm_realloc function, but basically I need to avoid copying
 * data over and over by trying to extend the current block whenever possible. A useful trick I adopt is to insert a small padding bytes (realloc_padding)
 * to the size of each block when mm_realloc is called, which increases the size of the block to make space for future realloc.
 * It's again a trade-off: more fragmentation, but fewer times mm_realloc needs to actually copy the data over. It proves to be very useful in this case. 
 * 
 * -------------------------------------- END -------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "CSCI 2021 - UMN",
    /* First member's full name */
    "Khiem Vuong",
    /* First member's email address */
    "vuong067@umn.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic constants and macros */

#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0xf)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE               8        /* word size (bytes) */
#define DSIZE               16       /* doubleword size (bytes) */
#define INIT_CHUNKSIZE      (1<<6)
#define CHUNKSIZE           (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD            16       /* overhead of header and footer (bytes) */
#define NUM_BUCKET          17
#define REALLOC_BUFFER      (1<<7)

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) > (y)? (y) : (x))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define PRED(bp)       ((char *) (bp))
#define SUCC(bp)       ((char *) (bp + WSIZE))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define PRED_BLKP(bp)  ((char *) GET(PRED(bp)))
#define SUCC_BLKP(bp)  ((char *) GET(SUCC(bp)))

/* Global variables */
static char *heap_listp;  /* pointer to first block */
static char **free_listp; /* pointer to pointers to segregated free lists */

/* Internal helper functions */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insert(void *bp);
static int getSeglistSize();
static int isSeglistPointer(void *ptr);
static void delete(void *bp);
static void printBlock(void *bp);
static void checkBlock(void *bp);
static void printSeglist();
static void checkSeglist();

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    /* Initialize the heap, which contains 17 pointers to 17 segregated free lists (initial value = 0),
    prologue, and epilogue
     * Total size: (17 * WSIZE) + (2 * WSIZE) + (WSIZE) = WSIZE * 20
     */
    if ((heap_listp = mem_sbrk(WSIZE*(NUM_BUCKET + 3))) == NULL)
        return -1;

    memset(heap_listp, 0, NUM_BUCKET * WSIZE);
    free_listp = (char **) heap_listp;

    /* Next, initialize the prologue and epilogue block and move the heap_listp */
    heap_listp += NUM_BUCKET * WSIZE;
    PUT(heap_listp, PACK(DSIZE, 1));            /* prologue header */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));    /* prologue footer */
    PUT(heap_listp + 2*WSIZE, PACK(0, 1));      /* epilogue header */
    heap_listp += WSIZE;                        /* heap_listp points at the prologue */
    
    /* Extend the empty heap with a free block of INIT_CHUNKSIZE bytes */
    if (extend_heap(INIT_CHUNKSIZE/WSIZE) == NULL)
        return -1;

    // mm_check(0);
    return 0;
}

/*
 * mm_malloc
 * - We always allocate a block whose size is a multiple of the alignment.
 * - We search the free list for a fit using find_fit function (first-fit policy). If no fit is found, we get more memory by using extend_heap
 * and place the block. Splitting occurs in place function.
 */
void *mm_malloc(size_t size) {
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0)
	    return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
	    asize = DSIZE + OVERHEAD;
    else
	    asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
	    place(bp, asize); // Found the fit for the free list, place and return the pointer to the allocated block
	    return bp; 
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	    return NULL;
    
    place(bp, asize);

    // mm_check(0);
    return bp;
}

/*
 * mm_free - Freeing a block. Adopt immediate coalescing, and insert the newly freed, coalesced block into the appropriate free list.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0)); // zero-ed the allocated bit of header and footer
    PUT(FTRP(ptr), PACK(size, 0));
    
    PUT(PRED(ptr), 0); // Also zero-ed the predecessor and successor pointer (optional)
    PUT(SUCC(ptr), 0);

    insert(coalesce(ptr)); // insert the freed and coalesed block into the free list
  
//    mm_check(0);
}

/*
 * mm_realloc - Avoid copying data over and over again by trying to coalesce with the next block whenever possible. Also, a subtle trick is to
 * maintain the block larger than the normal block (by adding realloc_padding) in order to avoid extending the heap/malloc over and over again.
 */
void *mm_realloc(void *ptr, size_t size) {
    void *new_ptr = ptr;                                                    /* Pointer to be returned */
    size_t new_size = size;                                                 /* Adjusted size of the new block */
    int remainder;                                                          
    size_t extendsize;                                                      /* Size of heap extension if needed */
    int block_buffer = 0;

    // Size 0 is just like freeing the block
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    } 
    else if (ptr == NULL) {
        mm_malloc(size);
        return NULL;
    }
    
    // Add the overhead and alignment requirements
    if (new_size <= DSIZE) {
        new_size = 2 * DSIZE;
    } else {
        new_size = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    }

    /* Add realloc padding to block size to optimize realloc */
    new_size += REALLOC_BUFFER;

    block_buffer = GET_SIZE(HDRP(ptr)) - new_size;
    
    /* Allocate more space if not sufficient memory at the current block */
    if (block_buffer < 0) {
        /* If next block is a free block or the epilogue block, then extend the block without copying the data over */
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr))) ) {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
            
            if (remainder < 0) {
                extendsize = MAX(-remainder, CHUNKSIZE);
                if ((extend_heap(extendsize/WSIZE)) == NULL)                /* Request more memory by extend_heap */
                    return NULL;
                remainder += extendsize;
            }
                
            delete(NEXT_BLKP(ptr));                                         /* Do the coalescing with the next block (free) */

            PUT(HDRP(ptr), PACK(new_size + remainder, 1)); 
            PUT(FTRP(ptr), PACK(new_size + remainder, 1)); 
        } 
        else {        /* Not sufficient size and the next block is allocated, then use malloc to request the new block of memory and copy the data over */
            new_ptr = mm_malloc(new_size - DSIZE);
            memcpy(new_ptr, ptr, MIN(size, new_size));
            mm_free(ptr);
        }
    }
//    mm_check(0); 
    return new_ptr;     // Return the reallocated block 
}

/*
 * mm_check - Return 1 if the heap is consistent. Do the checking by calling checkSeglist and checkBlock. Otherwise, print specific error messages.
 */
int mm_check(int verbose)
{
    char *bp = heap_listp;
    
    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) { // check for bad prologue
	    printf("Bad prologue header\n");
    }
    
    if (verbose) printf("-------Heap--------\n");
   
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {         // check each block in the heap (and print if verbose)
	    if (verbose) printBlock(bp);                                    
	    checkBlock(bp);
    }
    
    if (verbose) printf("-------Heap--------\n");

    
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {                  // check for bad epilogue
	    printf("Bad epilogue header\n");
    }
    
    if (verbose) printSeglist();                                                // check the seglist (and print if verbose)
    checkSeglist();
    
    return 1;
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((bp = mem_sbrk(size)) == (void *)-1) // Request more memory
	    return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
    bp = coalesce(bp); /* Coalesce if the previous/next block is free */
    insert(bp); // insert the block into the appropriate segregated list
    return bp;
}

/*
 * place - Place block of asize bytes at the start of free block bp
 * and do the splitting if the remainder is at least the minimum size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (DSIZE + OVERHEAD)) { // if the remainder is at least the minimum size
        /* Place the block by setting header and footer for the block */
        delete(bp);                              // delete the original block from the free list
	    PUT(HDRP(bp), PACK(asize, 1));
	    PUT(FTRP(bp), PACK(asize, 1));
        
        /* Do the splitting: set header and footer for the next block */
	    bp = NEXT_BLKP(bp);
	    PUT(HDRP(bp), PACK(csize-asize, 0));
	    PUT(FTRP(bp), PACK(csize-asize, 0));

        PUT(PRED(bp), 0);                       // also zero-ed the predecessor and successor pointer of the block (optional)
        PUT(SUCC(bp), 0);
        
        insert(bp);                             // insert the splitted block into the appropriate free list
    }
    else {                                      // the remainder is not sufficient for splitting
        delete(bp);                             // delete the block from the free list
	    PUT(HDRP(bp), PACK(csize, 1));          // and allocate by setting header and footer
	    PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * find_fit - Find a fit for a block with asize bytes. Adopt first-fit policy.
 */
static void *find_fit(size_t asize)
{
    int bucket = getSeglistSize(asize);     // get the appropriate bucket
    void *class_p, *bp;
    size_t blk_size;
    
    while (bucket < NUM_BUCKET) {
        class_p = free_listp + bucket;
        if (GET(class_p) != 0) {
            bp = (void *) GET(class_p);
            while (bp) {
                blk_size = GET_SIZE(HDRP(bp));
                if (asize <= blk_size) {        // found the first fit: return the pointer to the block
                    return bp;
                }
                bp = SUCC_BLKP(bp);             // continue iterating through the free list
            }
        }
        bucket++;                               // fit not found: go to the next bucket
    }
    return NULL;                                // return NULL if no fit is found
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block. There are 4 cases when coalescing.
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));             // check if the previous block is allocated
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));             // check if the next block is allocated
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {                                 /* Case 1: both blocks already allocated, no coalescing */
        return bp;
    }
    else if (prev_alloc && !next_alloc) {                           /* Case 2: combine with the next block */
        delete(NEXT_BLKP(bp));                              // delete the next block from the free list, prepare for coalescing
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));              
        PUT(HDRP(bp), PACK(size, 0));                       // get the new size, then update the footer and header
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {                           /* Case 3: combine with the previous block */
        delete(PREV_BLKP(bp));                              // delete the previous block from the free list, prepare for coalescing
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));                       // get the new size, then update the footer and header
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                                 // move the pointer to the start of the new block
    }
    else {                                                          /* Case 4: combine with the both next and previous blocks */
        delete(PREV_BLKP(bp));                              // delete both blocks from the free list, prepare for coalescing
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));            // get the new size, then update the appropriate footer and header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                                 // move the pointer to the start of the new block
    }
    return bp;
}

/*
 * isSeglistPointer - return 1 if the pointer ptr is the seglist pointer (a pointer to a doubly linked list of free blocks of a class size)
 */
static int isSeglistPointer(void *ptr) {
    size_t ptr_val = (size_t) ptr;
    size_t start = (size_t) free_listp;
    size_t end = start + WSIZE*(NUM_BUCKET-1);

    if (ptr_val > end || ptr_val < start)
        return 0;
    if ((end - ptr_val) % WSIZE)
        return 0;
    return 1;
}

/*
 * delete - delete a block from the free list. There are also 4 cases.
 */
static void delete(void *bp) {
    int pre = !isSeglistPointer(PRED_BLKP(bp));             // if bp is not the first block (the previous block is not the seglist pointer)      
    int suc = (SUCC_BLKP(bp) != NULL);
    
    if (GET_ALLOC(HDRP(bp))) {
        printf("ERROR: Calling delete on an allocated block!\n");
        return;
    }

    if (!pre && suc) {                                      // if bp is the first block and has successors
        PUT(PRED_BLKP(bp), (size_t) SUCC_BLKP(bp));
        PUT(PRED(SUCC_BLKP(bp)), (size_t) PRED_BLKP(bp));
    }
    else if (!pre && !suc) {                                // if bp is both the first and the last block of the list
        PUT(PRED_BLKP(bp), (size_t) SUCC_BLKP(bp));
    }
    else if (pre && suc) {                                  // if bp is a block in the middle with successors and predecessors
        PUT(SUCC(PRED_BLKP(bp)), (size_t) SUCC_BLKP(bp));
        PUT(PRED(SUCC_BLKP(bp)), (size_t) PRED_BLKP(bp));
    }
    else {                                                  // if bp is the last block
        PUT(SUCC(PRED_BLKP(bp)), 0);
    }

    PUT(PRED(bp), 0);                                       // as always, we also zero-ed the predecessor and successor pointer (optional)
    PUT(SUCC(bp), 0);
}

/*
 * insert - insert a free block pointed at by bp into the appropriate free list (bucket) at the beginning.
 */
static void insert(void *bp) {

    size_t size = GET_SIZE(HDRP(bp));                       // size of the block at bp
    char **bucket_ptr;                                      // the pointer to the bucket (class size)
    size_t bp_val = (size_t) bp;

    bucket_ptr = free_listp + getSeglistSize(size);         // move the bucket pointer to the right place
    if (GET(bucket_ptr) == 0) {                             // if this bucket is empty
        PUT(bucket_ptr, bp_val);                            // bucket points to block at bp
        PUT(PRED(bp), (size_t) bucket_ptr);                 // also set the predecessor and successor of block at bp
        PUT(SUCC(bp), 0);
    }
    else {                                                  // if this bucket is not empty, insert the free block at the beginning of the bucket
        PUT(PRED(bp), (size_t) bucket_ptr);
        PUT(SUCC(bp), GET(bucket_ptr));
        PUT(PRED(GET(bucket_ptr)), bp_val);
        PUT(bucket_ptr, bp_val);
    }
}

/*
 * getSeglistSize - get the appropriate bucket number for the block size (17 buckets numbered from 0 to 16)
 */

static int getSeglistSize(size_t blksize) {
    if (blksize <= 8) 			    return 0;
	else if (blksize <= 32) 	    return 1;
	else if (blksize <= 64) 	    return 2;
	else if (blksize <= 128) 	    return 3;
	else if (blksize <= 256) 	    return 4;
	else if (blksize <= 512) 	    return 5;
	else if (blksize <= 1024) 	    return 6;
	else if (blksize <= 2048) 	    return 7;
	else if (blksize <= 4096) 	    return 8;
	else if (blksize <= 8192) 	    return 9;
	else if (blksize <= 16384) 	    return 10;
	else if (blksize <= 32769) 	    return 11;
	else if (blksize <= 65536) 	    return 12;
	else if (blksize <= 131072)     return 13;
	else if (blksize <= 262144)     return 14;
	else if (blksize <= 524288)     return 15;
	else   					        return 16;
}

/* 
 * ------------------------- BELOW HERE ARE FUNCTIONS FOR CHECKING THE CONSISTENCY OF THE HEAP, USED BY MM_CHECKHEAP() --------------------------
*/
static void printBlock(void *bp) {                                          /* Print the block information at bp */
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
	    printf("%p: EOL\n", bp);
	    return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c] pred: [%p] succ: [%p]\n", bp,
	   hsize, (halloc ? 'a' : 'f'),
	   fsize, (falloc ? 'a' : 'f'),
       (void *) GET(PRED(bp)),
       (void *) GET(SUCC(bp)));
}

static void printSeglist() {                                                /* Print the segregated list */
    void *ptr, *bp;
    printf("\n------Beginning of Segregated Free List-------\n");
    for (int i = 0; i < NUM_BUCKET; i++) {
        ptr = free_listp + i;
        if (GET(ptr) == 0) {
            printf("- [%p] Bucket %d: (empty)\n", ptr, i);
        } 
        else {
            printf("- [%p] Bucket %d: (not empty)\n", ptr, i);
            bp = (void *) GET(ptr);
            while (bp != ((void *) 0)) {
                printBlock(bp);
                bp = SUCC_BLKP(bp);
            }
        }
    }
    printf("\n------End of Segregated Free List--------\n");
}


static void checkBlock(void *bp) {                                          /* Check if the block at bp is conformed to our requirements */
    if (!(bp <= mem_heap_hi() && bp >= mem_heap_lo())) {
        printf("Error: %p is not in heap\n", bp);
    }
    
    if ((size_t)bp % DSIZE) {
	    printf("Error: %p is not doubleword aligned\n", bp);
    }

    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
        printf("Error: header does not match footer, block (%p):\n", bp);
    }
}

static void checkSeglist() {                                                /* Check if the seglist is conformed to our requirements */
	char *bp;
	int freeInSeglist = 0;
	int freeInHeap = 0;

	for (int i = 0; i < NUM_BUCKET; ++i){
		for (bp = free_listp[i]; bp != NULL; bp = SUCC_BLKP(bp)) {
            freeInSeglist++;                                                            /* increment free blocks in seglist */
			checkBlock(bp);
			
			if (GET_ALLOC(HDRP(bp))) {                                                  /* check if all the blocks in the seglist are free */
			    printf("ERROR: allocated block (%p) appeared in seg list.\n", bp);
			}

			if (getSeglistSize(GET_SIZE(HDRP(bp))) != i) {                              /* check if there is a block in a wrong bucket */
			    printf("ERROR: block (%p) located in wrong bucket.\n", bp);
			}
	    }	
	}

	/* Computer total number of free blocks in heap */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp))) {
			freeInHeap++;
	    }
	}

    if (freeInSeglist != freeInHeap){
    	printf("ERROR: number of free blocks in seglist is inconsistent with in heap.\n");
    }
}
