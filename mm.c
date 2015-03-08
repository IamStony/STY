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
 *
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
 * provide your team information in below _AND_ in the
 * struct that follows.
 *
 * === User information ===
 * Group: Veitekki 
 * User 1: kristinnj13
 * SSN: 1508932409
 * User 2: steinara13
 * SSN: 2404932259
 * === End User Information ===
 ********************************************************/
team_t team = {
    /* Group name */
    "Veitekki",
    /* First member's full name */
    "Kristinn Júlíusson",
    /* First member's email address */
    "kristinnj13@ru.is",
    /* Second member's full name (leave blank if none) */
    "Steinar Þór Árnason",
    /* Second member's email address (leave blank if none) */
    "steinara13@ru.is",
    /* Leave blank */
    "",
    /* Leave blank */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define NODESIZE 16
#define MIN_SIZE 32

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE         (ALIGN(sizeof(size_t)))
#define PACK(size, alloc)   ((size) | (alloc))          //Packs the size and allocated bit into a word
#define GET(bp)             (*(size_t *)(bp))           //Returns the word at address bp
#define PUT(bp, value)      (*(size_t *)(bp) = (value)) //Sets the word at address bp
#define GET_SIZE(bp)        (GET(bp) & ~0x7)            //Returns the size
#define IS_ALLOC(bp)        (GET(bp) & 0x1)             //Checks if node is allocated
//Node macros
#define NH(bp)              ((void *)(bp) - DSIZE)                      //Returns Node header address
#define NP(bp)              ((void *)(bp) - WSIZE)                      //Next node address (self)
#define NPA(bp)             (*(void **)(bp - WSIZE))                    //Returns next free node address
#define PP(bp)              ((void *)(bp) + GET_SIZE(NH(bp)))           //Prev node address (self)
#define PPA(bp)             (*(void **)(bp + GET_SIZE(NH(bp))))         //Returns prev free node address
#define NF(bp)              ((void *)(bp) + (GET_SIZE(NH(bp)) + WSIZE)) //Returns Node footer address

#define LNF(bp)             ((void *)(bp) - (NODESIZE - WSIZE))
#define LASTN(bp)           ((void *)(bp) - (GET_SIZE(LNF(bp)) + NODESIZE))
#define NEXTN(bp)           ((void *)(bp) + (GET_SIZE(NH(bp)) + NODESIZE))

void print_list();
void print_heap();
void print_node(void *node);
void print_pcn(void *node);
void *coalesce(void *bp);
void *find_fit(size_t size);
void split(void *bp, size_t size);
void add_node(void *bp);
void rm_node(void *bp);

static char *head = 0;   //List of free blocks

//void print_block(void *node);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* The head of the free space list starts here.
     * Head is marked as allocated with size = 0
     * The next pointer should point to the first free block
     * -New blocks are added to the front of the list
     *  -So heads next always points to the newest free'd block
     * The prev pointer always points to NULL, should never be used
     * Footer is marked as allocated with size = 0
     */
    head = mem_sbrk(NODESIZE);
    head += DSIZE;
    PUT(NH(head), PACK(0, 1));
    PUT(NP(head), 0);
    PUT(PP(head), 0);
    PUT(NF(head), PACK(0, 1));

    //print_node(head);

    return 0;
}

void print_list()
{
    void *bp;
    for(bp = head; bp != NULL; bp = NPA(bp))
    {
        print_node(bp);
    }
}

void print_heap()
{
    void *bp;
    int i = -1;
    for(bp = head; bp < (void *)mem_heap_hi(); bp = NEXTN(bp))
    {
        printf("Node: %d\n", i);
        print_node(bp);
        i = i + 1;
    }
    printf("Mem heap hi: %p\n\n", (void *)mem_heap_hi());
}

void print_node(void *node) 
{
    //printf("Printing node block:\n");
    printf("%s block.\n", IS_ALLOC(NH(node))?"Allocated":"Free");
    printf("Header: %p\n", NH(node));
    printf("H_size = %d\n", GET_SIZE(NH(node)));
    //printf("Next node: %p\n", NP(node));
    printf("Next node address: %p\n", NPA(node));
    printf("Payload: %p\n", node);
    //printf("Prev node: %p\n", PP(node));
    printf("Prev node address: %p\n", PPA(node));
    printf("Footer: %p\n", NF(node));
    printf("F_size = %d\n\n", GET_SIZE(NF(node)));
    fflush(stdout);
}

void print_pcn(void *node)
{
    printf("- - - - - - - - - - - - - - - - - - -\n");
    print_node(LASTN(node));
    printf("                - -                  \n");
    print_node(node);
    printf("                - -                  \n");
    print_node
    (NEXTN(node));
    printf("- - - - - - - - - - - - - - - - - - -\n");
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; //Aligned size
    void *bp;

    //Ignore spurious requests
    if(size < (size_t)1)
    {
        return NULL;
    }

    asize = ALIGN(size);

    bp = find_fit(asize);

    if(bp != NULL)
    {
        size_t leftover;
        leftover = GET_SIZE(NH(bp)) - asize;          //Size of leftover space
        //If leftover space is enough to create a new node 
        //AND has a size >= 16 then we make a new free block
        if((int)leftover >= (int)MIN_SIZE)
        {
            split(bp, asize);
        }
        else
        {
            rm_node(bp);
            PUT(NH(bp), PACK(GET_SIZE(NH(bp)), 1));
            PUT(NF(bp), PACK(GET_SIZE(NH(bp)), 1));
        }
    }
    else
    {
        size_t nnsize; //New Node Size
        nnsize = asize + NODESIZE;
        if(nnsize % 8 != 0)
        {
            printf("Not aligning correctly you numnuts!(malloc loves you)");
            fflush(stdout);
            return NULL;
        }
        bp = mem_sbrk(nnsize);
        if((long)bp == -1)
        {
            return NULL;
        }
        else
        {
            bp += DSIZE;    //Puts us at payload
            PUT(NH(bp), PACK(asize, 1));
            PUT(NP(bp), 0);
            PUT(PP(bp), 0);
            PUT(NF(bp), PACK(asize, 1));
        }
    }
    
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if(bp != NULL)
    {
        size_t size = GET_SIZE(NH(bp));

        PUT(NH(bp), PACK(size, 0));
        PUT(NF(bp), PACK(size, 0));
        coalesce(bp);
        //add_node(bp);
    }
}

void *coalesce(void *bp)
{
    //printf("Called coalesce.\n");
    void *p, *n;
    p = LASTN(bp);  //Previous node
    n = NEXTN(bp);  //Next node

    size_t prev_alloc = IS_ALLOC(NH(p));    //Previous node alloc bit
    size_t next_alloc = IS_ALLOC(NH(n));    //Next node alloc bit
    size_t size = GET_SIZE(NH(bp));         //Size of current node payload
    if(n > (void *)mem_heap_hi())
    {
        //printf("n is outside mem_heap_hi()\n");
        next_alloc = 1;
    }
    if(!next_alloc)     //If next node is free
    {
        size += GET_SIZE(NH(n)) + NODESIZE;
        rm_node(n);                 //Removes next node from free list
        PUT(NH(bp), PACK(size, 0));
        PUT(NF(bp), PACK(size, 0));
    }
    if(!prev_alloc)
    {
        size += GET_SIZE(NH(p)) + NODESIZE;
        rm_node(p);                 //Removes previous node from free list
        bp = p;                     //Moves us the the previous nodes payload address
        PUT(NH(bp), PACK(size, 0));
        PUT(NF(bp), PACK(size, 0));
    }

    add_node(bp);   //Adds coalesced node to free list
    return NULL;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    //return NULL;
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * size is the payload
 * */
void *find_fit(size_t size)
{
    void *bp;

    for(bp = NPA(head); bp != NULL; bp = NPA(bp))
    {
        if(size <= GET_SIZE(NH(bp)))
        {
            return bp;
        }
    }

    return NULL;
}

void split(void *bp, size_t size)
{
    rm_node(bp);    //Removes the node from the free list
    
    size_t losize;  //Leftover size
    void *node;

    losize = GET_SIZE(NH(bp)) - size - NODESIZE; //Just the payload, ignoring the space used for node creation

    //Allocate the split node, using the space we need
    PUT(NH(bp), PACK(size, 1));
    PUT(NP(bp), 0);
    PUT(PP(bp), 0);
    PUT(NF(bp), PACK(size, 1));

    //Create the new 'empty' node
    node = bp + (size + NODESIZE);  //Puts us at the payload of the new node
    PUT(NH(node), PACK(losize, 0));
    PUT(NP(node), 0);
    PUT(PP(node), 0);
    PUT(NF(node), PACK(losize, 0));

    coalesce(node);
    //add_node(node); //Add the new free node to the list
}

void add_node(void *bp)
{
    if(NPA(head) == NULL)       //If the free list is empty
    {
        NPA(head) = bp;         //head->next = bp;
    }
    else                        //Put it to the front of the list
    {
        NPA(bp) = NPA(head);    //bp->next = head->next
        NPA(head) = bp;         //head-> next = bp
        PPA(NPA(bp)) = bp;      //bp->next->prev = bp
    }
    PUT(PP(bp), 0);             //bp->prev = NULL
}

void rm_node(void *bp)
{
    if(PPA(bp) == NULL)         //If bp->prev is NULL we're the first free node
    {
        NPA(head) = NPA(bp);    //head->next = bp->next
        if(NPA(bp) != NULL)     //If bp is pointing to another free node
        {
            PPA(NPA(bp)) = NULL;//bp->next->prev = NULL
        }
    }
    else if(NPA(bp) == NULL)    //Last in list
    {
        NPA(PPA(bp)) = NULL;    //bp->prev->next = NULL
    }
    else                        //Somewhere inbetween
    {
        NPA(PPA(bp)) = NPA(bp); //bp->prev->next = current->next
        PPA(NPA(bp)) = PPA(bp); //bp->next->prev = current->prev
    }
    PUT(NP(bp), 0);             //bp->next = NULL
    PUT(PP(bp), 0);             //bp->prev = NULL
}
