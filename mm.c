/*
 * doublylinked free list
 *
 *  We're using 'nodes' to keep a record of our memory and we always point directly to the payload
 * of each node.
 *          
 * <------Header------>                 <------Footer----->
 *  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 * |         |         |               |         |         |
 * |  Size   | Pointer |               | Pointer |  Size   |
 * |         |   to    |               |   to    |         |
 * |         |  next   |    Payload    |  next   |         |
 * |_ _ _ _ _|  free   |               |  free   |_ _ _ _ _|
 * |Allocated|  node   |               |  node   |Allocated|
 * |_ _ _ _ _|_ _ _ _ _|_ _ _ _ _ _ _ _|_ _ _ _ _|_ _ _ _ _|
 *   4bytes    4bytes                    4bytes    4bytes
 *
 *  There's always a 'head' node iniated in mm_init. It's solely there to point 
 * to the start of the free list and make sure we don't coalesce beyond 
 * mem_heap_lo(). It has no payload so it takes up exactly 16bytes. 
 *  The first 4 bytes of a node contain both the size of the payload and
 * the allocated bit. The last 4 words also contain these information.
 *  If malloc doesn't find a node in the free list to use then we sbrk for more memory
 * and add an extra page to it so we don't have to call it as often. This is of course added
 * to the free list.
 *  Free nodes are split up if there's an excess of 32 or more bytes, 
 * 16 reserved for the node header/footer and the rest is always the payload.
 * Keeping the newly split node in the free list (adds it to the front) and allocating
 * the space we need.
 *  Newly freed blocks are inserted at the front of the list, behind the 'head'
 *  -e.g. 
 *            ->|             ->         ->         -> NULL
 *         head |       node(c)    node(b)    node(a) 
 *   NULL <-    | NULL <-         <-         <-
 *
 *
 * */
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
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)               //Align size to 8 bytes
#define SIZE_T_SIZE         (ALIGN(sizeof(size_t)))
#define PACK(size, alloc)   ((size) | (alloc))                              //Packs the size and allocated bit into a word
#define GET(bp)             (*(size_t *)(bp))                               //Returns the word at address bp
#define PUT(bp, value)      (*(size_t *)(bp) = (value))                     //Sets the word at address bp
#define GET_SIZE(bp)        (GET(bp) & ~0x7)                                //Returns the size
#define IS_ALLOC(bp)        (GET(bp) & 0x1)                                 //Checks if node is allocated
//Node macros
#define NH(bp)              ((void *)(bp) - DSIZE)                          //Returns Node header address
#define NP(bp)              ((void *)(bp) - WSIZE)                          //Next node address (self)
#define NPA(bp)             (*(void **)(bp - WSIZE))                        //Returns next free node address
#define PP(bp)              ((void *)(bp) + GET_SIZE(NH(bp)))               //Prev node address (self)
#define PPA(bp)             (*(void **)(bp + GET_SIZE(NH(bp))))             //Returns prev free node address
#define NF(bp)              ((void *)(bp) + (GET_SIZE(NH(bp)) + WSIZE))     //Returns Node footer address

#define LNF(bp)             ((void *)(bp) - (NODESIZE - WSIZE))             //Prev node footer (on heap)
#define LASTN(bp)           ((void *)(bp) - (GET_SIZE(LNF(bp)) + NODESIZE)) //Returns prev free node address (on heap)
#define NEXTN(bp)           ((void *)(bp) + (GET_SIZE(NH(bp)) + NODESIZE))  //Returns next free node address (on heap)

//All print functions we're used for debuggin purposes
void print_list();
void print_heap();
void print_node(void *node);
void print_pcn(void *node);
void coalesce(void *bp);
void *find_first(size_t size);
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
    head = mem_sbrk(NODESIZE);  //Extends the heap to initialize our head
    head += DSIZE;              //Puts us at the payload
    PUT(NH(head), PACK(0, 1));  //Puts the size of the header as 0 and marks it as allocated
    PUT(NP(head), 0);           //head->next = NULL
    PUT(PP(head), 0);           //head->prev = NULL
    PUT(NF(head), PACK(0, 1));  //Puts the size of the footer as 0 and marks it as allocated

    return 0;
}

/*
 * print_list - prints the whole free list, head included
 * */
void print_list()
{
    void *bp;
    for(bp = head; bp != NULL; bp = NPA(bp))
    {
        print_node(bp);
    }
}

/*
 * print_heap - prints the whole heap, head included
 * */
void print_heap()
{
    void *bp;
    for(bp = head; bp < (void *)mem_heap_hi(); bp = NEXTN(bp))
    {
        print_node(bp);
    }
    //Just to check if the last node is occupying the same end of the heap
    printf("Mem heap hi: %p\n\n", (void *)mem_heap_hi());
}

/*
 * print_node - prints the node information
 * */
void print_node(void *node) 
{
    //printf("Printing node block:\n");
    printf("%s block.\n", IS_ALLOC(NH(node))?"Allocated":"Free");   //Is the block allocated or free?
    printf("Header: %p\n", NH(node));                               //Header address
    printf("H_size = %d\n", GET_SIZE(NH(node)));                    //Size in the header
    //printf("Next node: %p\n", NP(node));                          //Next node address (just to check nodes are set up correctly)
    printf("Next node address: %p\n", NPA(node));                   //Gets the address stored at Next node
    printf("Payload: %p\n", node);                                  //Payload start address
    //printf("Prev node: %p\n", PP(node));                          //Previous node address (just to check nodes are set up correctly)
    printf("Prev node address: %p\n", PPA(node));                   //Gets the address stored at Previous node
    printf("Footer: %p\n", NF(node));                               //Footer address
    printf("F_size = %d\n\n", GET_SIZE(NF(node)));                  //Size in the footer
}

/*
 * print_pcn - Prints previous, current and next node based on current from heap
 * */
void print_pcn(void *node)
{
    printf("- - - - - - - - - - - - - - - - - - -\n");
    print_node(LASTN(node));                                        //Prints the previous node on the heap
    printf("                - -                  \n");
    print_node(node);                                               //Prints the current node we're on in the heap
    printf("                - -                  \n");
    print_node(NEXTN(node));                                        //Prinst the next node on the heap
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

    asize = ALIGN(size);    //Aligs size of the requested payload to 8

    bp = find_first(asize);   //Searches the free list for a node with enough payload

    if(bp != NULL) //If a node is found
    {
        size_t leftover;
        leftover = GET_SIZE(NH(bp)) - asize;    //Size of leftover/unused space in the node's payload
        
        //If leftover space is enough to create a new node 
        //AND has a payload size >= 16 then we make a new free block
        if((int)leftover >= (int)MIN_SIZE)
        {
            split(bp, asize);
        }
        else
        {
            //Remove the node from the free list and mark it as allocated
            rm_node(bp);
            PUT(NH(bp), PACK(GET_SIZE(NH(bp)), 1));
            PUT(NF(bp), PACK(GET_SIZE(NH(bp)), 1));
        }
    }
    else
    {
        size_t nnsize;                  //New Node Size
        size_t page = mem_pagesize();   //Extra node payload
        nnsize = asize + NODESIZE;

        bp = mem_sbrk(nnsize + page + NODESIZE);    //Extends the heap by the new node size and extra node
        //if sbrk was unsuccessful
        if((long)bp == -1)
        {
            return NULL;
        }
        else
        {
            bp += DSIZE;                  //Puts us at payload (past the header)
            PUT(NH(bp), PACK(asize, 1));  //Sets the header
            PUT(NP(bp), 0);               //bp->next = NULL
            PUT(PP(bp), 0);               //bp->prev = NULL
            PUT(NF(bp), PACK(asize, 1));  //Sets the footer
            
            //printf("mem_page = %d\n", mem_pagesize());
            void *extra;                  //Extra node
            extra = NEXTN(bp);            //Place us at its' payload
            PUT(NH(extra), PACK(page, 0));//Sets the header
            PUT(NP(extra), 0);            //extra->next = NULL
            PUT(PP(extra), 0);            //extra->prev = NULL
            PUT(NF(extra), PACK(page, 0));//Sets the footer
            add_node(extra);              //Adds the extra node to the free list
        }
    }
    
    return bp;  //Returns the pointer to the payload for allocation
}   

/*
 * mm_free - Calls coalesce and free's the node.
 */
void mm_free(void *bp)
{
    //Check if called correctly
    if(bp != NULL)
    {
        //Mark the node as free before coalescing
        size_t size = GET_SIZE(NH(bp));
        PUT(NH(bp), PACK(size, 0));
        PUT(NF(bp), PACK(size, 0));
        
        coalesce(bp);
    }
}

void coalesce(void *bp)
{
    void *p, *n;
    p = LASTN(bp);  //Previous node
    n = NEXTN(bp);  //Next node

    size_t prev_alloc = IS_ALLOC(NH(p));    //Previous node alloc bit
    size_t next_alloc = IS_ALLOC(NH(n));    //Next node alloc bit
    size_t size = GET_SIZE(NH(bp));         //Size of current node payload
    //If the next node is outside the heap
    if(n > (void *)mem_heap_hi())
    {
        next_alloc = 1; //Mark the non existance node as allocated to skip coalescing to the right
    }
    //If node to the right of bp on the heap is free
    if(!next_alloc)
    {
        size += GET_SIZE(NH(n)) + NODESIZE;
        rm_node(n);                 //Removes next node from free list
        PUT(NH(bp), PACK(size, 0));
        PUT(NF(bp), PACK(size, 0));
    }
    //If node to the left of bp on the heap is free
    if(!prev_alloc)
    {
        size += GET_SIZE(NH(p)) + NODESIZE;
        rm_node(p);                 //Removes previous node from free list
        bp = p;                     //Moves us the the previous nodes payload address
        PUT(NH(bp), PACK(size, 0)); //Update header size and marks us as a free node
        PUT(NF(bp), PACK(size, 0)); //Update footer size and marks us as a free node
    }

    add_node(bp);   //Adds coalesced node to free list
}

/*
 * mm_realloc - First fit version of realloc
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
 * find_fit - Finds the first free node that can hold the asked for payload (size)
 * */
void *find_first(size_t size)
{
    void *bp;
    //Starts search at the first free node head points to
    for(bp = NPA(head); bp != NULL; bp = NPA(bp))
    {
        //As soon as we find a node that has a big enough payload we return it
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

    //Create the new 'empty' node with the leftover size
    node = bp + (size + NODESIZE);  //Puts us at the payload of the new node
    PUT(NH(node), PACK(losize, 0));
    PUT(NP(node), 0);
    PUT(PP(node), 0);
    PUT(NF(node), PACK(losize, 0));

    coalesce(node); //Coalesce the new free block
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
