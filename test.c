#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "addrIntercept.h"


static const size_t QTY_REMOTE_REGION = 2U;
static const addr_t REMOTE_PERIPHERAL_ADDR = (addr_t)0x40000000U;
static const size_t REMOTE_PERIPHERAL_SIZE = 0x30000U;

static const addr_t REMOTE_MEMORY_ADDR = (addr_t)0x20000000U;
static const size_t REMOTE_MEMORY_SIZE = 96*1024;


static memoryTranslate * memoryMap = NULL;
static sizeMemoryTranslate_t sizeMemoryMap = 0;

static void setAddr(memoryTranslate *map, size_t size, addr_t remote_addr){
	map->start_addr = malloc(size);
	map->end_addr = memoryMap->start_addr + size;
	map->reference_addr = remote_addr;
}

memoryTranslate * getMemoryMap(sizeMemoryTranslate_t * size){
	memoryMap = (memoryTranslate *)malloc(sizeof(memoryTranslate)*QTY_REMOTE_REGION);

	uint32_t region = 0;

	setAddr(memoryMap, REMOTE_PERIPHERAL_SIZE, REMOTE_PERIPHERAL_ADDR);

	setAddr(memoryMap + 1, REMOTE_MEMORY_SIZE, REMOTE_MEMORY_ADDR);
	
	*size = sizeMemoryMap = QTY_REMOTE_REGION;

	printf("memoryMap = %p %p s = %p\n", memoryMap, memoryMap + 1, size);

	return memoryMap;
}

void freeMemoryMap(){
	for(uint32_t i = 0; i < sizeMemoryMap; i++){
		free(memoryMap[i].start_addr);
	}

	sizeMemoryMap = 0;
	free(memoryMap);
	memoryMap = NULL;
}

int
main(void)
{

	unsigned int s = 111;

	printf(" addr : %p\n", &s);

	memoryTranslate * p = getMemoryMap(&s); 

	printf("size : %d  p : %p\n", s, p);

        printf("test %p *:%x\n", p + 1, *(int*)p[1].start_addr);

    *(int*)p[1].start_addr = 9;


	return 0;
}


