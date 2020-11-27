#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>

#include "bench.h"

// set block size
#define BLOCK_SIZE (2048) // private memory (in bytes) for each tasklet in WRAM

// WRAM global variables
__host block_t blocks[NR_TASKLETS];
__dma_aligned uint8_t caches[NR_TASKLETS][BLOCK_SIZE];
__host uint32_t results[NR_TASKLETS];

// MRAM variables
__mram_noinit uint8_t input_buffer[SIZE_PER_DPU];

int main()
{
	uint32_t tasklet_id = me();
	uint32_t *block = (uint32_t *)caches[tasklet_id];

    /* Initialize once the cycle counter */
	if (tasklet_id == 0)
	{
		perfcounter_config(COUNT_CYCLES, true);
	}


	// keep track of how much data was processed
	results[tasklet_id] = 0;
	for (uint32_t current_block=blocks[tasklet_id].start; current_block < blocks[tasklet_id].end; current_block+=BLOCK_SIZE)
	{
		//printf("[%i]: =0x%x \n", tasklet_id, current_block);
        // load cache with current mram block
        mram_read(&input_buffer[current_block], block, BLOCK_SIZE);

			// increment each word
			for (unsigned int i=0; i < (BLOCK_SIZE>>2); i++)
				block[i]++;

			// copy it back
			mram_write(block, &input_buffer[current_block], BLOCK_SIZE);

			// record the results
			results[tasklet_id] += BLOCK_SIZE;
	}

	// keep the 32-bit LSB on the 64-bit cycle counter
	//result->cycles = (uint32_t)perfcounter_get();

	printf("[%02d] done %u bytes\n", tasklet_id, results[tasklet_id]);
	return 0;
}
