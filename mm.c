/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
#define NUM_SIZE_CLASS      17
#define MIN_BLOCK_SIZE      32
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
static char **freelist_p; /* pointer to pointers to segregated free lists */

/* Internal helper functions */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insert(void *bp);
static int get_size_class();
static int is_list_ptr(void *ptr);
static void delete(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);
static void printseglist();
static void checkseglist();


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
// create initial empty heap, no alignment padding needed
    // array (13 * 4) + prologue (2 * 4) + epilogue (4)
    if ((heap_listp = mem_sbrk(WSIZE*(NUM_SIZE_CLASS + 3))) == NULL)
        return -1;

    // first, initialize an array of pointers (with initial value 0)
    // each pointer in the array points to the first block of a certain
    // class size
    // size here means adjusted size.
    // minimum size of a size class is 16 bytes

    /* there are 17 size classes
     * [1-2^4], [2^4+1 - 2^5] ... [2^18+1, 2^19], [2^19+1 - +inf]
     */
    memset(heap_listp, 0, NUM_SIZE_CLASS*WSIZE);
    freelist_p = (char **) heap_listp;

    // next, initialize the prologue and epilogue block
    heap_listp += NUM_SIZE_CLASS * WSIZE;
    PUT(heap_listp, PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + 2*WSIZE, PACK(0, 1)); // epilogue header
    heap_listp += WSIZE; // set heap_listp as block pointer to prologue block
    
    // extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(INIT_CHUNKSIZE/WSIZE) == NULL)
        return -1;

    // mm_check(0);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
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
	    place(bp, asize);
	    return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	    return NULL;
    
    place(bp, asize);

    // mm_check(0);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    PUT(PRED(bp), 0);
    PUT(SUCC(bp), 0);

    insert(coalesce(bp)); // insert the freed and coalesed block into the free list
  
//    mm_check(0);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    void *new_ptr = ptr;    /* Pointer to be returned */
    size_t new_size = size; /* Size of new block */
    int remainder;          /* Adequacy of block sizes */
    size_t extendsize;         /* Size of heap extension */
    int block_buffer;       /* Size of block buffer */
    
    // Ignore size 0 cases
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    
    // Align block size
    if (new_size <= DSIZE) {
        new_size = 2 * DSIZE;
    } else {
        new_size = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    }

    /* Add overhead requirements to block size */
    new_size += REALLOC_BUFFER;

    /* Calculate block buffer */
    block_buffer = GET_SIZE(HDRP(ptr)) - new_size;

    /* Allocate more space if overhead falls below the minimum */
    if (block_buffer < 0) {
        /* Check if next block is a free block or the epilogue block */
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr))) ) {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
            if (remainder < 0) {
                extendsize = MAX(-remainder, CHUNKSIZE);
                if ((extend_heap(extendsize/WSIZE)) == NULL)
                    return NULL;
                remainder += extendsize;
            }
                
            delete(NEXT_BLKP(ptr)); 

            PUT(HDRP(ptr), PACK(new_size + remainder, 1)); 
            PUT(FTRP(ptr), PACK(new_size + remainder, 1)); 
        } else {
            new_ptr = mm_malloc(new_size - DSIZE);
            memcpy(new_ptr, ptr, MIN(size, new_size));
            mm_free(ptr);
        }
        block_buffer = GET_SIZE(HDRP(new_ptr)) - new_size;
    }
    
//    mm_check(0); 
    // Return the reallocated block 
    return new_ptr;
}

/*
 * mm_check - Return 1 if the heap is consistent.
 */
int mm_check(int verbose)
{
    char *bp = heap_listp;
    
    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
	    printf("Bad prologue header\n");
    }
    
    if (verbose) printf("-------Heap--------\n");
    if (verbose) printf("-------Heap--------\n");
   
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	    if (verbose) printblock(bp);
	    checkblock(bp);
    }
    
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
	    printf("Bad epilogue header\n");
    }
    
    if (verbose) printseglist();
    checkseglist();
    
    return 1;
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((bp = mem_sbrk(size)) == (void *)-1)
	    return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
    bp = coalesce(bp); /* Coalesce if the previous block was free */
    insert(bp); // insert the block into segregated list
    return bp;
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (DSIZE + OVERHEAD)) {
        delete(bp); // delete the block from free list
	    PUT(HDRP(bp), PACK(asize, 1));
	    PUT(FTRP(bp), PACK(asize, 1));
        
	    bp = NEXT_BLKP(bp);
	    PUT(HDRP(bp), PACK(csize-asize, 0));
	    PUT(FTRP(bp), PACK(csize-asize, 0));

        PUT(PRED(bp), 0);
        PUT(SUCC(bp), 0);
        insert(bp);
    }
    else {
        delete(bp);
	    PUT(HDRP(bp), PACK(csize, 1));
	    PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    int size_class = get_size_class(asize);
    void *class_p, *bp;
    size_t blk_size;
    
    while (size_class < NUM_SIZE_CLASS) {
        class_p = freelist_p + size_class;
        if (GET(class_p) != 0) {
            bp = (void *) GET(class_p);
            while (bp != ((void *) 0)) {
                blk_size = GET_SIZE(HDRP(bp));
                if (asize <= blk_size) {
                    return bp;
                }
                bp = SUCC_BLKP(bp);
            }
        }
        size_class++;
    }
    return NULL;
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        // combine with next block
        // first, delete next block from free list
        delete(NEXT_BLKP(bp));
        // then, update the size information
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        // combine with previous block
        // first, delete previous block from free list
        delete(PREV_BLKP(bp));
        // then, update the size information
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        // combine with both previous and next block
        // first, delete both previous and next block
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        // then, update the size information
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * delete a block from free list
 */
static void delete(void *bp) {
    int pre = !is_list_ptr(PRED_BLKP(bp));
    int suc = (SUCC_BLKP(bp) != (void *) 0);
    if (GET_ALLOC(HDRP(bp))) {
        printf("ERROR: Calling delete on an allocated block!\n");
        return;
    }

    // if bp is the first block of a free list and has successors
    if (!pre && suc) {
        PUT(PRED_BLKP(bp), (size_t) SUCC_BLKP(bp));
        PUT(PRED(SUCC_BLKP(bp)), (size_t) PRED_BLKP(bp));
    }
    // if bp is both the first and the last block of a list
    else if (!pre && !suc) {
        PUT(PRED_BLKP(bp), (size_t) SUCC_BLKP(bp));
    }
    // if bp is an intermediate block
    else if (pre && suc) {
        PUT(SUCC(PRED_BLKP(bp)), (size_t) SUCC_BLKP(bp));
        PUT(PRED(SUCC_BLKP(bp)), (size_t) PRED_BLKP(bp));
    }
    // if bp is the last block
    else {
        PUT(SUCC(PRED_BLKP(bp)), 0);
    }

    // finally, zero out the pred and succ of bp to be safe
    PUT(PRED(bp), 0);
    PUT(SUCC(bp), 0);
}

static int is_list_ptr(void *ptr) {
    size_t ptr_val = (size_t) ptr;
    size_t start = (size_t) freelist_p;
    size_t end = start + WSIZE*(NUM_SIZE_CLASS-1);

    if (ptr_val > end || ptr_val < start)
        return 0;
    if ((end - ptr_val) % WSIZE)
        return 0;
    return 1;
}

/*
 * insert a free block at bp into the segregated list
 */
static void insert(void *bp) {

    size_t size = GET_SIZE(HDRP(bp)); // adjusted size
    char **size_class_ptr; // the pointer to the address of the first free block of the size class
    size_t bp_val = (size_t) bp;

    // get appropriate size class
    size_class_ptr = freelist_p + get_size_class(size);
    // the appropriate size class is empty
    if (GET(size_class_ptr) == 0) {
        PUT(size_class_ptr, bp_val);
        // set pred/succ of new free block
        PUT(PRED(bp), (size_t) size_class_ptr);
        PUT(SUCC(bp), 0);
    }
    // the appropriate size class is not empty
    // insert the free block at the beginning of the size class
    else {
        // set pred/succ of new free block
        PUT(PRED(bp), (size_t) size_class_ptr);
        PUT(SUCC(bp), GET(size_class_ptr));
        // connect the previous head of the size class
        PUT(PRED(GET(size_class_ptr)), bp_val);
        // change heap array
        PUT(size_class_ptr, bp_val);
    }
}

static int get_size_class(size_t blksize) {
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

static void printblock(void *bp) {
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

static void printseglist() {
    void *ptr, *bp;
    printf("\n------Beginning of Segregated Free List-------\n");
    for (int i = 0; i < NUM_SIZE_CLASS; i++) {
        ptr = freelist_p + i;
        if (GET(ptr) == 0) {
            printf("- [%p] Bucket %d: empty\n", ptr, i);
        } 
        else {
            printf("- [%p] Bucket %d: not empty\n", ptr, i);
            bp = (void *) GET(ptr);
            while (bp != ((void *) 0)) {
                printblock(bp);
                bp = SUCC_BLKP(bp);
            }
        }
    }
    printf("\n------End of Segregated Free List--------\n");
}


static void checkblock(void *bp) {
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

static void checkseglist() { 
	char *bp;
	int freeInSeglist = 0;
	int freeInHeap = 0;

	for (int i = 0; i < NUM_SIZE_CLASS; ++i){
		for (bp = freelist_p[i]; bp != NULL; bp = SUCC_BLKP(bp)) {
            freeInSeglist++;
			checkblock(bp);

			/* check if block is free */
			if (GET_ALLOC(HDRP(bp))) {
			    printf("ERROR: allocated block (%p) appeared in seg list.\n", bp);
			}

			/* check block-bucket consistency */
			if (get_size_class(GET_SIZE(HDRP(bp))) != i) {
			    printf("ERROR: block (%p) located in wrong bucket.\n", bp);
			}
	    }	
	}

	/* check total number of free blocks in heap */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp))) {
			freeInHeap++;
	    }
	}

    if (freeInSeglist != freeInHeap){
    	printf("ERROR: number of free blocks in seglist is inconsistent with in heap.\n");
    }
}


