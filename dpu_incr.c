#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>

#include "bench.h"

// set block size
#define BLOCK_SIZE (2048) // private memory (in bytes) for each tasklet in WRAM

__dma_aligned uint8_t WRAM[NR_TASKLETS][BLOCK_SIZE];
__mram_noinit uint8_t DPU_BUFFER[BUFFER_SIZE];

int main()
{
	uint32_t tasklet_id = me();
	uint32_t *block = (uint32_t *)WRAM[tasklet_id];

    /* Initialize once the cycle counter */
	if (tasklet_id == 0)
	{
		perfcounter_config(COUNT_CYCLES, true);
	}

	for (uint32_t buffer_idx = tasklet_id * BLOCK_SIZE; buffer_idx < BUFFER_SIZE;
         buffer_idx += (NR_TASKLETS * BLOCK_SIZE))
	{
        // load cache with current mram block
        mram_read(&DPU_BUFFER[buffer_idx], block, BLOCK_SIZE);

			// increment each byte
			for (unsigned int i=0; i < (BLOCK_SIZE>>2); i++)
				block[i]++;

			// copy it back
			mram_write(block, &DPU_BUFFER[buffer_idx], BLOCK_SIZE);
	}

	// keep the 32-bit LSB on the 64-bit cycle counter
	//result->cycles = (uint32_t)perfcounter_get();

	//printf("[%02d] done\n", tasklet_id);
	return 0;
}
