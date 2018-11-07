#include <stdint.h>
#pragma once


typedef enum {
	NO_ERROR
} ChipErrorType;


typedef uint64_t * addr_t;

typedef struct
{
 addr_t start_addr;
 addr_t end_addr;
 addr_t reference_addr;
} memoryTranslate;


typedef uint32_t sizeMemoryTranslate_t;



memoryTranslate * getMemoryMap(sizeMemoryTranslate_t * size);
void freeMemoryMap();

void resetChip();

ChipErrorType showError();


#define NAME_MEMORY_MAP_FUNCTION  		"getMemoryMap"
#define NAME_MEMORY_MAP_FREE_FUNCTION  	"freeMemoryMap"

#define NAME_RESET_CHIP_FUNCTION  		"resetChip"
#define NAME_ERROR_FUNCTION  			"showError"
