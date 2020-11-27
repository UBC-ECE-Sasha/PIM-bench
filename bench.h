#ifndef _COMMON__H
#define _COMMON__H

#include "PIM-common/common/include/common.h"

#define SIZE_PER_DPU (MRAM_SIZE - MEGABYTE(1)) // how much MRAM can we process?

typedef struct {
    uint32_t checksum;
    uint32_t cycles;
    uint32_t bytes_read;
} dpu_results_t;

typedef struct block_t
{
	uint32_t start;
	uint32_t end;
} block_t;

#endif // _COMMON__H
