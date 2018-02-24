/* 
 * mm-implicit.c -  Simple allocator based on implicit free lists, 
 *                  first fit placement, and boundary tag coalescing. 
 *
 * Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
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
	"Maximum Dank",
	/* First member's full name */
	"Andrew Rutherford",
	/* First member's email address */
	"anru9411@colorado.edu",
	/* Second member's full name (leave blank if none) */
	"David Olson",
	/* Second member's email address (leave blank if none) */
	"daol5181@colorado.edu"
};

/*
 * Sources: References to various pages on Github and StackOverflow, and the course textbook.
 */

/////////////////////////////////////////////////////////////////////////////
// Constants and macros
//
// These correspond to the material in Figure 9.43 of the text
// The macros have been turned into C++ inline functions to
// make debugging code easier.
//
/////////////////////////////////////////////////////////////////////////////
#define WSIZE     4       /* Word size in bytes */
#define DSIZE     8       /* Double word size in bytes */
#define CHUNKSIZE (1<<12) /* Page size in bytes */
#define MINSIZE   16      /* Minimum block size */

#define LISTS     20      /* Number of segregated lists */
#define BUFFER  (1<<7)    /* Reallocation buffer */

#define MAX(x, y) ((x) > (y) ? (x) : (y)) /* Maximum of two numbers */
#define MIN(x, y) ((x) < (y) ? (x) : (y)) /* Minimum of two numbers */

/* Pack size and allocation bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)            (*(unsigned int *)(p))
// Preserve reallocation bit
#define PUT(p, val)       (*(unsigned int *)(p) = (val) | GET_TAG(p))
// Clear reallocation bit
#define PUT_NOTAG(p, val) (*(unsigned int *)(p) = (val))

/* Store predecessor or successor pointer for free blocks */
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

/* Adjust the reallocation tag */
#define SET_TAG(p)   (*(unsigned int *)(p) = GET(p) | 0x2)
#define UNSET_TAG(p) (*(unsigned int *)(p) = GET(p) & ~0x2)

/* Read the size and allocation bit from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p)   (GET(p) & 0x2)

/* Address of block's header and footer */
#define HEAD(ptr) ((char *)(ptr) - WSIZE)
#define FOOT(ptr) ((char *)(ptr) + GET_SIZE(HEAD(ptr)) - DSIZE)

/* Address of next and previous blocks */
#define NEXT(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))
#define PREV(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE))

/* Address of free block's predecessor and successor entries */
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

/* Address of free block's predecessor and successor on the segregated list */
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

/* Check for alignment */
#define ALIGN(p) (((size_t)(p) + 7) & ~(0x7))

/* Settings for mm_check */
#define CHECK         0 
#define CHECK_MALLOC  1 
#define CHECK_FREE    1 
#define CHECK_REALLOC 1 
#define DISPLAY_BLOCK 1 
#define DISPLAY_LIST  1 
#define PAUSE         1 
                           
#define LINE_OFFSET   4 /* Line offset for referencing trace files */

/*
 * Global variables
 */
void *free_lists[LISTS]; /* Array of pointers to segregated free lists */
char *prologue_block;    /* Pointer to prologue block */

/*
 * Function prototypes
 */
static void *extend_heap(size_t size);
static void *coalesce(void *ptr);
static void place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);

//static void mm_check(char caller, void *ptr, int size);

/* 
 * mm_init - Initialize the malloc package. Construct prologue and epilogue
 *           blocks.
 */
int mm_init(void)
{
  int list;         // List counter
  char *heap_start; // Pointer to beginning of heap
  
  /* Initialize array of pointers to segregated free lists */
  for (list = 0; list < LISTS; list++) {
    free_lists[list] = NULL;
  }

  /* Allocate memory for the initial empty heap */
  if ((long)(heap_start = mem_sbrk(4 * WSIZE)) == -1)
    return -1;
  
  PUT_NOTAG(heap_start, 0);                            
  PUT_NOTAG(heap_start + (1 * WSIZE), PACK(DSIZE, 1)); 
  PUT_NOTAG(heap_start + (2 * WSIZE), PACK(DSIZE, 1)); 
  PUT_NOTAG(heap_start + (3 * WSIZE), PACK(0, 1));     
  prologue_block = heap_start + DSIZE;
  
  /* Extend the empty heap */
  if (extend_heap(CHUNKSIZE) == NULL)
    return -1;
  
  return 0;
}

/* 
 * mm_malloc - Allocate a new block by placing it in a free block, extending
 *             heap if necessary. Blocks are padded with boundary tags and
 *             lengths are changed to conform with alignment.
 */
void *mm_malloc(size_t size)
{
  size_t asize;      
  size_t extendsize; 
  void *ptr = NULL; 
  int list = 0;      
  
  /* Filter invalid block size */
  if (size == 0)
    return NULL;
  
  /* Adjust block size to include boundary tags and alignment requirements */
  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  
  /* Select a free block of sufficient size from segregated list */
  size = asize;
  while (list < LISTS) {
    if ((list == LISTS - 1) || ((size <= 1) && (free_lists[list] != NULL))) {
      ptr = free_lists[list];

      while ((ptr != NULL)
        && ((asize > GET_SIZE(HEAD(ptr))) || (GET_TAG(HEAD(ptr)))))
      {
        ptr = PRED(ptr);
      }
      if (ptr != NULL)
        break;
    }
    
    size >>= 1;
    list++;
  }
  
  /* Extend the heap if no free blocks of sufficient size are found */
  if (ptr == NULL) {
    extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize)) == NULL)
      return NULL;
  }
  
  /* Place the block */
  place(ptr, asize);
  
  /* Return pointer to newly allocated block */
  return ptr;
}

/*
 * mm_free - Free a block by adding it to the appropriate list and coalescing
 *           it.
 */
void mm_free(void *ptr)
{
  size_t size = GET_SIZE(HEAD(ptr)); /* Size of block */
  
  /* Unset the reallocation tag on the next block */
  UNSET_TAG(HEAD(NEXT(ptr)));
  
  /* Adjust the allocation status in boundary tags */
  PUT(HEAD(ptr), PACK(size, 0));
  PUT(FOOT(ptr), PACK(size, 0));
  
  /* Insert new block into appropriate list */
  insert_node(ptr, size);
  
  /* Coalesce free block */
  coalesce(ptr);
  
  return;
}

/*
 * mm_realloc - Reallocate a block in place, extending the heap if necessary.
 *              The new block is padded with a buffer to guarantee that the
 *              next reallocation can be done without extending the heap,
 *              assuming that the block is expanded by a constant number of bytes
 *              per reallocation.
 *
 *              If the buffer is not large enough for the next reallocation, 
 *              mark the next block with the reallocation tag. Free blocks
 *              marked with this tag cannot be used for allocation or
 *              coalescing. The tag is cleared when the marked block is
 *              consumed by reallocation, when the heap is extended, or when
 *              the reallocated block is freed.
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *new_ptr = ptr;    /* Pointer to be returned */
  size_t new_size = size; /* Size of new block */
  int remainder;          /* Adequacy of block sizes */
  int extendsize;         /* Size of heap extension */
  int block_buffer;       /* Size of block buffer */
	
	/* Filter invalid block size */
  if (size == 0)
    return NULL;
  
  /* Adjust block size to include boundary tag and alignment requirements */
  if (new_size <= DSIZE) {
    new_size = 2 * DSIZE;
  } else {
    new_size = DSIZE * ((new_size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  
  /* Add overhead requirements to block size */
  new_size += BUFFER;
  
  /* Calculate block buffer */
  block_buffer = GET_SIZE(HEAD(ptr)) - new_size;
  
  /* Allocate more space if overhead falls below the minimum */
  if (block_buffer < 0) {
    /* Check if next block is a free block or the epilogue block */
    if (!GET_ALLOC(HEAD(NEXT(ptr))) || !GET_SIZE(HEAD(NEXT(ptr)))) {
      remainder = GET_SIZE(HEAD(ptr)) + GET_SIZE(HEAD(NEXT(ptr))) - new_size;
      if (remainder < 0) {
        extendsize = MAX(-remainder, CHUNKSIZE);
        if (extend_heap(extendsize) == NULL)
          return NULL;
        remainder += extendsize;
      }
      
      delete_node(NEXT(ptr));
      
      //Do not split block
      PUT_NOTAG(HEAD(ptr), PACK(new_size + remainder, 1)); /* Block header */
      PUT_NOTAG(FOOT(ptr), PACK(new_size + remainder, 1)); /* Block footer */
    } else {
      new_ptr = mm_malloc(new_size - DSIZE);
      memmove(new_ptr, ptr, MIN(size, new_size));
      mm_free(ptr);
    }
    block_buffer = GET_SIZE(HEAD(new_ptr)) - new_size;
  }  

  /* Tag the next block if block overhead drops below twice the overhead */
  if (block_buffer < 2 * BUFFER)
    SET_TAG(HEAD(NEXT(new_ptr)));
  
  /* Return reallocated block */
  return new_ptr;
}

/*
 * extend_heap - Extend the heap with a system call. Insert the newly
 *               requested free block into the appropriate list.
 */
static void *extend_heap(size_t size)
{
  void *ptr;                   /* Pointer to newly allocated memory */
  size_t words = size / WSIZE; /* Size of extension in words */
  size_t asize;                /* Adjusted size */
  
  /* Allocate an even number of words to maintain alignment */
  asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  
  /* Extend the heap */
  if ((long)(ptr = mem_sbrk(asize)) == -1)
    return NULL;
  
  /* Set headers and footer */
  PUT_NOTAG(HEAD(ptr), PACK(asize, 0));   /* Free block header */
  PUT_NOTAG(FOOT(ptr), PACK(asize, 0));   /* Free block footer */
  PUT_NOTAG(HEAD(NEXT(ptr)), PACK(0, 1)); /* Epilogue header */
  
  /* Insert new block into appropriate list */
  insert_node(ptr, asize);
  
  /* Coalesce if the previous block was free */
  return coalesce(ptr);
}

/*
 * insert_node - Insert a block pointer into a segregated list. Lists are
 *               segregated by byte size, with the n-th list spanning byte
 *               sizes 2^n to 2^(n+1)-1. Each individual list is sorted by
 *               pointer address in ascending order.
 */
static void insert_node(void *ptr, size_t size) {
  int list = 0;
  void *search_ptr = ptr;
  void *insert_ptr = NULL;
  
  /* Select segregated list */
  while ((list < LISTS - 1) && (size > 1)) {
    size >>= 1;
    list++;
  }
  
  /* Select location on list to insert pointer while keeping list
     organized by byte size in ascending order. */
  search_ptr = free_lists[list];
  while ((search_ptr != NULL) && (size > GET_SIZE(HEAD(search_ptr)))) {
    insert_ptr = search_ptr;
    search_ptr = PRED(search_ptr);
  }
  
  /* Set predecessor and successor */
  if (search_ptr != NULL) {
    if (insert_ptr != NULL) {
      SET_PTR(PRED_PTR(ptr), search_ptr); 
      SET_PTR(SUCC_PTR(search_ptr), ptr);
      SET_PTR(SUCC_PTR(ptr), insert_ptr);
      SET_PTR(PRED_PTR(insert_ptr), ptr);
    } else {
      SET_PTR(PRED_PTR(ptr), search_ptr); 
      SET_PTR(SUCC_PTR(search_ptr), ptr);
      SET_PTR(SUCC_PTR(ptr), NULL);
      
      /* Add block to appropriate list */
      free_lists[list] = ptr;
    }
  } else {
    if (insert_ptr != NULL) {
      SET_PTR(PRED_PTR(ptr), NULL);
      SET_PTR(SUCC_PTR(ptr), insert_ptr);
      SET_PTR(PRED_PTR(insert_ptr), ptr);
    } else {
      SET_PTR(PRED_PTR(ptr), NULL);
      SET_PTR(SUCC_PTR(ptr), NULL);
      
      /* Add block to appropriate list */
      free_lists[list] = ptr;
    }
  }

  return;
}

/*
 * delete_node: Remove a free block pointer from a segregated list. If
 *              necessary, adjust pointers in predecessor and successor blocks
 *              or reset the list head.
 */
static void delete_node(void *ptr) {
  int list = 0;
  size_t size = GET_SIZE(HEAD(ptr));
  
  /* Select segregated list */
  while ((list < LISTS - 1) && (size > 1)) {
    size >>= 1;
    list++;
  }
  
  if (PRED(ptr) != NULL) {
    if (SUCC(ptr) != NULL) {
      SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
      SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
    } else {
      SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
      free_lists[list] = PRED(ptr);
    }
  } else {
    if (SUCC(ptr) != NULL) {
      SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
    } else {
      free_lists[list] = NULL;
    }
  }
  
  return;
}

/*
 * coalesce - Coalesce adjacent free blocks. Sort the new free block into the
 *            appropriate list.
 */
static void *coalesce(void *ptr)
{
  size_t prev_alloc = GET_ALLOC(HEAD(PREV(ptr)));
  size_t next_alloc = GET_ALLOC(HEAD(NEXT(ptr)));
  size_t size = GET_SIZE(HEAD(ptr));
  
  /* Return if previous and next blocks are allocated */
  if (prev_alloc && next_alloc) {
    return ptr;
  }
  
  /* Do not coalesce with previous block if it is tagged */
  if (GET_TAG(HEAD(PREV(ptr))))
    prev_alloc = 1;
  
  /* Remove old block from list */
  delete_node(ptr);
  
  /* Detect free blocks and merge, if possible */
  if (prev_alloc && !next_alloc) {
    delete_node(NEXT(ptr));
    size += GET_SIZE(HEAD(NEXT(ptr)));
    PUT(HEAD(ptr), PACK(size, 0));
    PUT(FOOT(ptr), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    delete_node(PREV(ptr));
    size += GET_SIZE(HEAD(PREV(ptr)));
    PUT(FOOT(ptr), PACK(size, 0));
    PUT(HEAD(PREV(ptr)), PACK(size, 0));
    ptr = PREV(ptr);
  } else {
    delete_node(PREV(ptr));
    delete_node(NEXT(ptr));
    size += GET_SIZE(HEAD(PREV(ptr))) + GET_SIZE(HEAD(NEXT(ptr)));
    PUT(HEAD(PREV(ptr)), PACK(size, 0));
    PUT(FOOT(NEXT(ptr)), PACK(size, 0));
    ptr = PREV(ptr);
  }
  
  /* Adjust segregated linked lists */
  insert_node(ptr, size);
  
  return ptr;
}

/*
 * place - Set headers and footers for newly allocated blocks. Split blocks
 *         if enough space is remaining.
 */
static void place(void *ptr, size_t asize)
{
  size_t ptr_size = GET_SIZE(HEAD(ptr));
  size_t remainder = ptr_size - asize;
  
  /* Remove block from list */
  delete_node(ptr);
  
  if (remainder >= MINSIZE) {
    /* Split block */
    PUT(HEAD(ptr), PACK(asize, 1)); /* Block header */
    PUT(FOOT(ptr), PACK(asize, 1)); /* Block footer */
    PUT_NOTAG(HEAD(NEXT(ptr)), PACK(remainder, 0)); /* Next header */
    PUT_NOTAG(FOOT(NEXT(ptr)), PACK(remainder, 0)); /* Next footer */  
    insert_node(NEXT(ptr), remainder);
  } else {
    /* Do not split block */
    PUT(HEAD(ptr), PACK(ptr_size, 1)); /* Block header */
    PUT(FOOT(ptr), PACK(ptr_size, 1)); /* Block footer */
  }
  return;
}
