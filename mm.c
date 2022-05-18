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
    "Week06-Team 6\n",
    /* First member's full name */
    "Woosik Sim\n",
    /* First member's email address */
    "woosick9292@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4 // Word and header/footer size(bytes)
#define DSIZE 8 // Double word size(bytes)
#define CHUNKSIZE (1<<12)   

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // size와 alloc 연산 or로 합침  -> header나 footer에서 사용

/* Read and write a word at address p*/
/* 캐스팅이 중요. 인자 p는 대게 (void *) 포인터이며, 
이것은 직접적으로 역참조할 수 없다. 캐스팅 해야 역참조 가능. */
#define GET(p) (*(unsigned int *)(p))              // p가 가리키는 곳에 있는 값 반환
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 곳에 val을 저장

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)                // header or footer & 연산으로 뒤에 3비트 구할 수 있다. (할당 여부)
#define GET_ALLOC(p) (GET(p) & 0x1)                // header or footer & 0b1 => 블록의 할당 여부음

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)                      // header 포인터 반환
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // footer 포인터 반환

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)bp + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 블록의 header 포인터 반환 (bp + 현재 블록 사이즈)
#define PREV_BLKP(bp) ((char *)bp - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록의 header 포인터 반환 (bp - 이전 블록 사이즈)

static char *heap_listp; // 프롤로그 블록 가리키는 포인터

static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    /* Create the initial empty heap */


    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1) // mem_sbrk 추가적인 heap영역 요청하는 함수, -1이면 메모리할당 실패했다는 뜻
        return -1;
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // Epilogue header
    heap_listp += (2*WSIZE);                        // 프롤로그 블록의 footer

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize; // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE) // size가 8byte보다 작을 때
        asize = 2*DSIZE; // header 4byte + footer 4byte + size => 16byte(2*DSIZE) 8의 배수로 align 해주려고
    else    
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); // 8의 배수로 만들어 주기 위한 과정(나보다 크고 가장 가까운 8의 배수로 올림처리)

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) { // 들어갈 사이즈의 블록이 있으면
        place(bp, asize);                 // 블록 할당하고 남은 블록은 다시 free블록으로 잘라두기
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE); // 맞는 사이즈의 블록이 없으면 추가로 얼마나 요청할지 max값 구하기
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) // 더 이상 힙 크기 늘릴수 없으면 NULL반환
        return NULL;
    place(bp, asize); // 힙 사이즈 늘어났으면 다시 블록 할당
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp)); // 블록 사이즈 구하기

    PUT(HDRP(bp), PACK(size, 0));     // header 더 정보 수정(free)
    PUT(FTRP(bp), PACK(size, 0));     // footer 정보 수정(free)
    coalesce(bp);                     // coalesce (연결)
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    size_t oldsize;
    void *newptr;

    /* If size == 0 the this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched. */
    if (!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize)
        oldsize = size;
    memcpy(newptr, ptr, oldsize); 

    /* Free the old block. */
    mm_free(ptr);
    return newptr;
}

/* Boundary tag coalescing. Return ptr ro coalesced block */


static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 할당 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 할당 여부
    size_t size = GET_SIZE(HDRP(bp));                   // 현재 블록의 사이즈

    if (prev_alloc && next_alloc) {                     // Case 1 이전 & 다음 블록 둘 다 alloc 상태
        return bp;
    }
    else if (prev_alloc && !next_alloc) {               // Case 2 이전 블록 alloc, 다음 블록 free 상태
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); 

    }
    else if (!prev_alloc && next_alloc) {               // Case 3 이전 블록 free, 다음 블록 alloc 상태
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 0)); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    }
    else {                                              // Case 4 이전 & 다음 블록 모두 free 상태
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); 
        bp = PREV_BLKP(bp);

    }
    return bp;
}

/* 
 * extend_heap - Extends the heap with a free block and return its block pointer
*/
static void *extend_heap(size_t words) { // heap 초기화 or heap 공간 부족할 때 요청 호출
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */

    // 8의 배수로 만들기, 짝수면 WSIZE 곱하고, 홀수면 +1 해서 WISZE 곱하기
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;                        // 더이상 힙을 늘릴수 없으면 return NULL

    /* Initialize free block header/footer and the epilogue header */

    PUT(HDRP(bp), PACK(size, 0));           // Free block header
    PUT(FTRP(bp), PACK(size, 0));           // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // New epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *find_fit(size_t asize) {
    /* First-fit search */


    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) { // prologue 블록부터 블록마다 검사
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {    // 아직 free상태이고 필요한 사이즈보다 크면
            return bp;                                                  // 현재 free블록의 bp반환
        }
    }
    return NULL;                                                        // No fit, 맞는 블록이 없으면 NULL반환
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 블록 사이즈 구하기

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));         // 필요한 부분만큼 alloc 처리, header 갱신
        PUT(FTRP(bp), PACK(asize, 1));         // 필요한 부분만큼 alloc 처리, footer 갱신
        bp = NEXT_BLKP(bp);                    // 필요한 부분쓰고 남은 블록으로 bp갱신
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 남은 블록은 사이즈 줄이고, free => header 갱신
        PUT(FTRP(bp), PACK(csize - asize, 0)); // 남은 블록은 사이즈 줄이고, free => footer 갱신
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));         // header 정보 alloc처리
        PUT(FTRP(bp), PACK(csize, 1));         // footer 정보 alloc처리
    }
}
