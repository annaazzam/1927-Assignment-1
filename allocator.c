//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h> 

#define HEADER_SIZE    sizeof(struct free_list_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
    u_int32_t magic;  // ought to contain MAGIC_FREE
    vsize_t size;     // # bytes in this block (including header)
    vlink_t next;     // memory[] index of next free block
    vlink_t prev;     // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]

int makePow2(int i);
int merge(); //returns number of merges executed
free_header_t * iToP(int index); //index to pointer
int pToI(void *pointer); //pointer to index


int makePow2(int i) {
    int x = 2;
    while (x < i) x *= 2;
    return x;
}

free_header_t * iToP(int index) {
    return (free_header_t *)((byte *)memory + index);
}


int pToI (void *pointer) {
    return ((byte *)pointer - (byte *)memory);   
}

// Input: size - number of bytes to make available to the allocator
// Output: none              
// Precondition: Size is a power of two.
// Postcondition: `size` bytes are now available to the allocator
// 
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{
    // TODO
    if (memory == NULL) { //don't run if allocator already initialised
        if (size < 512) size = 512;
        size = makePow2(size); //make size a power of 2
        memory = malloc(size);
        if (memory != NULL) { 
            free_list_ptr = 0;
            memory_size = size;
        	((free_header_t *)memory)->magic = MAGIC_FREE;
        	((free_header_t *)memory)->size = size;
        	((free_header_t *)memory)->prev = 0;
        	((free_header_t *)memory)->next = 0;
        } else { //if malloc failed
            fprintf(stderr, "vlad_init: insufficient memory");
            abort();
        }
    }
}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n is < size of memory available to the allocator
// Postcondition: If a region of size n or greater cannot be found, p = NULL 
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >= 
//                      n + header size.

void *vlad_malloc(u_int32_t n)
{
	int required_mem_size = n + HEADER_SIZE;
    required_mem_size = makePow2(required_mem_size);
	if ((required_mem_size <= memory_size) && (n >= 8)) { //make sure there's enough memory
        free_header_t *curr = iToP(free_list_ptr);
        int i = 0;
        while ((curr != iToP(free_list_ptr)) || (i == 0)) {
            i = 1;
            if (curr->size == required_mem_size) {  
                if ((curr->prev == curr->next) && (curr->prev == pToI(curr))) return NULL;
                else {
                    iToP(curr->prev)->next = curr->next;
                    iToP(curr->next)->prev = curr->prev;
                    curr->size = required_mem_size;
                    curr->magic = MAGIC_ALLOC;
                    if (curr == iToP(free_list_ptr)) { 
                        free_list_ptr = curr->next;
                    }
                    return (void *)((byte *)curr + HEADER_SIZE);
                }
			}
            if (curr->next == free_list_ptr) {
                break;
            } else {
                curr = iToP(curr->next);
            }
		}
        
        curr = iToP(free_list_ptr);
        i = 0;
    	while ((curr != iToP(free_list_ptr)) || (i == 0)) { //CASE: there's a block of memory larger than reqd 
            i = 1;
            if (curr->size > required_mem_size) {
                free_header_t *new; 
                while (curr->size > required_mem_size) { //while size of curr space is bigger, half it  
                    new = (free_header_t *)((byte *)curr + (curr->size)/2); 
                    new->magic = MAGIC_FREE;
                    new->prev = pToI(curr);
                    new->next = curr->next;
                    if (curr->next == free_list_ptr) {
                        iToP(free_list_ptr)->prev = pToI(new);
                    } else { 
                        iToP(curr->next)->prev = pToI(new);
                    }
                    curr->next = pToI(new);
                    curr->size = (curr->size)/2;
                    new->size = curr->size;
                    curr = new;
                }

                if ((new->prev == new->next) && (new->prev == pToI(new))) 
                    return NULL;
                else 
                    new->magic = MAGIC_ALLOC;
                    iToP(new->prev)->next = new->next;
                    iToP(new->next)->prev = new->prev;
                    if (curr == iToP(free_list_ptr)) {
                        free_list_ptr = curr->next;
                    }
                    return (void *)((byte *)new + HEADER_SIZE);
			}
            if (curr->next == free_list_ptr) {
                curr = iToP(free_list_ptr);
            } else {
                curr = iToP(curr->next);
            }
		}
		
	}
    return NULL;
}


// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object can be re-allocated by 
//                vlad_malloc

void vlad_free(void *object)
{ 
    free_header_t *block = (free_header_t *)((byte *)object - HEADER_SIZE);
    if (block->magic == MAGIC_ALLOC) { //make sure memory being freed is allocated
        free_header_t *curr = iToP(free_list_ptr);
        block->magic = MAGIC_FREE;
        if (block < curr) {
            block->next = free_list_ptr;
            block->prev = iToP(free_list_ptr)->prev;
            iToP(iToP(free_list_ptr)->prev)->next = pToI(block);
            iToP(free_list_ptr)->prev = pToI(block);
            
            free_list_ptr = pToI(block);
        } else {
            while (block > curr) {
                if (curr->next == free_list_ptr) break; //reached end
                else curr = iToP(curr->next);
            }
            
            iToP(curr->next)->prev = pToI(block);
            
            block->next = curr->next;
            curr->next = pToI(block);
            block->prev = pToI(curr);
        }
        
        int num_merges = merge();
        while (num_merges != 0) {
            num_merges = merge();
        }
    } else {
        fprintf(stderr, "Attempt to free non-allocated memory");
        abort();
    }
}
                
int merge () {
    free_header_t *curr = (free_header_t *)memory;
    while (1) {
        if ((curr->magic == MAGIC_FREE) && (pToI((byte *)curr + curr->size) < memory_size)) {
            if ((pToI(curr)/curr->size) % 2 == 0) { //merge with block ahead
                if ((pToI((byte *)curr + curr->size) != memory_size) && (curr->size == ((free_header_t *)((byte *)curr + curr->size))->size)) {
                    if (((free_header_t *)((byte *)curr + curr->size))->magic == MAGIC_FREE) {
                        //MERGE CURR AND CURR->NEXT:
                        if (curr->next == curr->prev) {
                            curr->prev = pToI(curr);
                        }
			            curr->next = ((free_header_t *)((byte *)curr + curr->size))->next;
                        curr->size = curr->size * 2;
                        if (pToI((byte *)curr + curr->size) >= memory_size) {
                            iToP(free_list_ptr)->prev = pToI(curr);
                        } else {
                            ((free_header_t *)((byte *)curr + curr->size))->prev = pToI(curr);
                        }
                        return 1;
                    } 
                } 
            }/* else { //merge with block after
                if ((curr->prev != 0) && (curr->size == iToP(curr->prev)->size)) {
                    if (iToP(curr->prev)->magic == MAGIC_FREE) {
                        //MERGE CURR AND CURR->PREV
                        iToP(curr->prev)->size = curr->size * 2;
                        iToP(curr->prev)->next = curr->next;
                        iToP(curr->next)->prev = curr->prev;
                        return 1;
                    }  
                }
            }*/
        }
        if (curr->magic == MAGIC_ALLOC) {
            if (pToI((byte *)curr + curr->size) == memory_size) {
                break;
            } else {
                curr = (free_header_t *)((byte *)curr + curr->size);
            }
        } else if (curr->next != free_list_ptr) {
            curr = (free_header_t *)((byte *)curr + curr->size);  
        } else break;
    }
    return 0;
}

// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
    if (memory != NULL) {
        free(memory);
        memory = NULL;
        memory_size = 0;
    }
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{
    printf("free_list_ptr: %d\n", free_list_ptr);
    free_header_t *curr = (free_header_t *)memory;
    int i = 1;
    while (pToI(curr) < memory_size) {
        printf("=====HEADER %d======\n", i);
        printf("Index: %d\n", pToI(curr));
        if (curr->magic == MAGIC_FREE)  {
            printf("MAGIC_FREE\n");
            printf("Size: %d\n", curr->size);
            printf("Previous free block: %d\n", curr->prev);
            printf("Next free block: %d\n", curr->next);
        } else if (curr->magic == MAGIC_ALLOC) {
            printf("MAGIC_ALLOC\n");
            printf("Size: %d\n", curr->size);
        }
        
        curr = (free_header_t *)((byte *)curr + curr->size);
        i++;
    }

    return;
}


//
// All of the code below here was written by Alen Bou-Haidar, COMP1927 14s2
//

//
// Fancy allocator stats
// 2D diagram for your allocator.c ... implementation
// 
// Copyright (C) 2014 Alen Bou-Haidar <alencool@gmail.com>
// 
// FancyStat is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or 
// (at your option) any later version.
// 
// FancyStat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>


#include <string.h>

#define STAT_WIDTH  32
#define STAT_HEIGHT 16
#define BG_FREE      "\x1b[48;5;35m" 
#define BG_ALLOC     "\x1b[48;5;39m"
#define FG_FREE      "\x1b[38;5;35m" 
#define FG_ALLOC     "\x1b[38;5;39m"
#define CL_RESET     "\x1b[0m"


typedef struct point {int x, y;} point;

static point offset_to_point(int offset,  int size, int is_end);
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label);



// Print fancy 2D view of memory
// Note, This is limited to memory_sizes of under 16MB
void vlad_reveal(void *alpha[26])
{
    int i, j;
    vlink_t offset;
    char graph[STAT_HEIGHT][STAT_WIDTH][20];
    char free_sizes[26][32];
    char alloc_sizes[26][32];
    char label[3]; // letters for used memory, numbers for free memory
    int free_count, alloc_count, max_count;
    free_header_t * block;

	// TODO
	// REMOVE these statements when your vlad_malloc() is done
    //printf("vlad_reveal() won't work until vlad_malloc() works\n");
    //return;

    // initilise size lists
    for (i=0; i<26; i++) {
        free_sizes[i][0]= '\0';
        alloc_sizes[i][0]= '\0';
    }

    // Fill graph with free memory
    offset = 0;
    i = 1;
    free_count = 0;
    while (offset < memory_size){
        block = (free_header_t *)(memory + offset);
        if (block->magic == MAGIC_FREE) {
            snprintf(free_sizes[free_count++], 32, 
                "%d) %d bytes", i, block->size);
            snprintf(label, 3, "%d", i++);
            fill_block(graph, offset,label);
        }
        offset += block->size;
    }

    // Fill graph with allocated memory
    alloc_count = 0;
    for (i=0; i<26; i++) {
        if (alpha[i] != NULL) {
            offset = ((byte *) alpha[i] - (byte *) memory) - HEADER_SIZE;
            block = (free_header_t *)(memory + offset);
            snprintf(alloc_sizes[alloc_count++], 32, 
                "%c) %d bytes", 'a' + i, block->size);
            snprintf(label, 3, "%c", 'a' + i);
            fill_block(graph, offset,label);
        }
    }

    // Print all the memory!
    for (i=0; i<STAT_HEIGHT; i++) {
        for (j=0; j<STAT_WIDTH; j++) {
            printf("%s", graph[i][j]);
        }
        printf("\n");
    }

    //Print table of sizes
    max_count = (free_count > alloc_count)? free_count: alloc_count;
    printf(FG_FREE"%-32s"CL_RESET, "Free");
    if (alloc_count > 0){
        printf(FG_ALLOC"%s\n"CL_RESET, "Allocated");
    } else {
        printf("\n");
    }
    for (i=0; i<max_count;i++) {
        printf("%-32s%s\n", free_sizes[i], alloc_sizes[i]);
    }

}

// Fill block area
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label)
{
    point start, end;
    free_header_t * block;
    char * color;
    char text[3];
    block = (free_header_t *)(memory + offset);
    start = offset_to_point(offset, memory_size, 0);
    end = offset_to_point(offset + block->size, memory_size, 1);
    color = (block->magic == MAGIC_FREE) ? BG_FREE: BG_ALLOC;

    int x, y;
    for (y=start.y; y < end.y; y++) {
        for (x=start.x; x < end.x; x++) {
            if (x == start.x && y == start.y) {
                // draw top left corner
                snprintf(text, 3, "|%s", label);
            } else if (x == start.x && y == end.y - 1) {
                // draw bottom left corner
                snprintf(text, 3, "|_");
            } else if (y == end.y - 1) {
                // draw bottom border
                snprintf(text, 3, "__");
            } else if (x == start.x) {
                // draw left border
                snprintf(text, 3, "| ");
            } else {
                snprintf(text, 3, "  ");
            }
            sprintf(graph[y][x], "%s%s"CL_RESET, color, text);            
        }
    }
}

// Converts offset to coordinate
static point offset_to_point(int offset,  int size, int is_end)
{
    int pot[2] = {STAT_WIDTH, STAT_HEIGHT}; // potential XY
    int crd[2] = {0};                       // coordinates
    int sign = 1;                           // Adding/Subtracting
    int inY = 0;                            // which axis context
    int curr = size >> 1;                   // first bit to check
    point c;                                // final coordinate
    if (is_end) {
        offset = size - offset;
        crd[0] = STAT_WIDTH;
        crd[1] = STAT_HEIGHT;
        sign = -1;
    }
    while (curr) {
        pot[inY] >>= 1;
        if (curr & offset) {
            crd[inY] += pot[inY]*sign; 
        }
        inY = !inY; // flip which axis to look at
        curr >>= 1; // shift to the right to advance
    }
    c.x = crd[0];
    c.y = crd[1];
    return c;
}
