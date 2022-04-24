#include "bootpack.h"
#include <stdio.h>

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int cntLeadingZeros(unsigned int i)
{
    unsigned int ret = 0;
    unsigned int temp = ~i;

    while(temp & 0x80000000)
    {
        temp <<= 1;
        ret++;
    }
    return ret;
}

void *memSet(void *s, int c, unsigned long n)
{
	if (0 == s || n < 0)
		return 0;
	char * tmpS = (char *)s;
	while(n-- > 0)
		*tmpS++ = c;
		return s; 
}

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) < (b) ? (b) : (a))

#define GET_NEXT(head) *(unsigned int*)(head)

unsigned int buddy_check(buddy_alloc_t* buddy, unsigned int a, unsigned int b, unsigned int size) {
	unsigned int lower = min(a, b) - buddy->base;
	unsigned int upper = max(a, b) - buddy->base;

	return (lower ^ size) == upper;
}


void buddy_add_free_item(buddy_alloc_t* buddy, unsigned int address, unsigned int order, int new) {
	unsigned int head = buddy->free_lists[order - MIN_ORDER];
	GET_NEXT(address) = 0;
	unsigned int size = 1 << order;
	if ( !new && head != 0) {
		unsigned int prev = 0;
		while ( 1 ) {
			if ( buddy_check(buddy, head, address, size) ) {
				if ( prev != 0 ) {
					GET_NEXT(prev) = GET_NEXT(head);
				}
				else {
					buddy->free_lists[order - MIN_ORDER] = GET_NEXT(head);
				}
				buddy_add_free_item(buddy, min(head, address), order + 1, 0);
				return;
			}
			if ( GET_NEXT(head) == 0 ) {
				GET_NEXT(head) = address;
				break;
			}
			prev = head;
			head = GET_NEXT(head);
		}
	}
	else {
		GET_NEXT(address) = head;
		buddy->free_lists[order - MIN_ORDER] = address;
	}
}

unsigned int buddy_alloc(buddy_alloc_t* buddy, unsigned int size) {
	int original_order = (32 - cntLeadingZeros(size - 1));
	if ( original_order >= MAX_ORDER ) {
		return 0;
	}
	unsigned int want_size = 1 << original_order;
    int order = original_order;
	for ( order; order < MAX_ORDER; order++ ) {
		if ( buddy->free_lists[order - MIN_ORDER] != 0 ){
			unsigned int address = buddy->free_lists[order - MIN_ORDER];
			buddy->free_lists[order - MIN_ORDER] = GET_NEXT(address);
			unsigned int found_size = 1 << order;
			if ( found_size != want_size ) {
				while ( found_size > want_size ) {
					found_size = found_size >> 1;
					buddy_add_free_item(buddy, address + found_size, 32 - cntLeadingZeros(found_size - 1), 1);
				}
			}
			return address;
		}
	}
	return 0;
}

void buddy_dealloc(buddy_alloc_t* buddy, unsigned int address, unsigned int size) {
	int order = 32 - cntLeadingZeros(size - 1);
	buddy_add_free_item(buddy, address, order, 0);
}

unsigned int buddy_status(buddy_alloc_t* buddy) {
    unsigned int free_mem = 0;
    int order;
	for ( order = MIN_ORDER; order < MAX_ORDER; order++ ) {

		unsigned int head = buddy->free_lists[order - MIN_ORDER];

		while ( head != 0 ) {
			free_mem+= 1<<order;
			head = GET_NEXT(head);
		}
	}
    return free_mem;
}

void buddy_list_status(buddy_alloc_t* buddy){
	int order;
	for(order = MIN_ORDER;order < MAX_ORDER;order++){
		unsigned int head = buddy->free_lists[order-MIN_ORDER];
		buddy->free_lists_num[order-MIN_ORDER]=0;
		while(head!=0){
			buddy->free_lists_num[order-MIN_ORDER] += 1;
			head = GET_NEXT(head);
		}
	}
}

buddy_alloc_t* buddy_init(unsigned int address,unsigned int start_add, unsigned int size) {
	if ( size <= (1 << MIN_ORDER) ) {
		return NULL;
	}
	buddy_alloc_t* buddy = (buddy_alloc_t*) address;
    address = start_add;
	memSet(buddy, 0, sizeof(buddy_alloc_t));
	buddy->base = address;
	while ( size > (1 << MIN_ORDER) ) {
        int order;
		for ( order= MAX_ORDER - 1; order >= MIN_ORDER; order-- ) {
			if ( size >= (1 << order) ) {
				buddy_add_free_item(buddy, address, order, 1);
				address += 1 << order;
				size -= 1 << order;	
				break;
			}
		}
	}
	return buddy;
}

unsigned int memtest(unsigned int start, unsigned int end) 
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; 
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) {
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT; 
	io_store_eflags(eflg);
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; 
		store_cr0(cr0);
	}
	i = memtest_sub(start, end);
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; 
		store_cr0(cr0);
	}
	return i;
}