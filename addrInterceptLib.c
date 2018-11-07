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
	memoryMap->start_addr = (addr_t)malloc(size);
	memoryMap->end_addr = memoryMap->start_addr + size;
	memoryMap->reference_addr = remote_addr;
}

memoryTranslate * getMemoryMap(sizeMemoryTranslate_t * size){
	memoryMap = (memoryTranslate *)malloc(sizeof(memoryTranslate)*QTY_REMOTE_REGION);

	uint32_t region = 0;

	setAddr(memoryMap, REMOTE_PERIPHERAL_SIZE, REMOTE_PERIPHERAL_ADDR);

	setAddr(memoryMap + 1, REMOTE_MEMORY_SIZE, REMOTE_MEMORY_ADDR);
	
	*size = sizeMemoryMap = QTY_REMOTE_REGION;

	printf("memoryMap = %p s = %p\n",memoryMap,size);

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
