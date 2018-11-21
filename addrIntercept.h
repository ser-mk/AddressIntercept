#include <stdint.h>
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NO_ERROR
} ChipErrorType;

#if defined(TARGET_IA32)
typedef uint32_t * addr_t;
#else
typedef uint64_t * addr_t;
#endif

#if defined(__LP64__) || defined(_LP64)
typedef uint64_t  AddrType;
#else
typedef uint32_t  AddrType;
#endif

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


#ifdef __cplusplus
}
#endif
