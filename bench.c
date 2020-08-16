#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <dpu.h>

#define GIGA ((double)(1 << 30))

static double
my_clock()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (1.0e-9 * t.tv_nsec + t.tv_sec);
}

int main(int argc, char** argv)
{
	uint32_t dpu_count=2;
	uint32_t size_per_dpu, dpus_to_test;
	struct dpu_set_t dpus;

	// get all of the DPUs

  	struct dpu_symbol_t symbol;
	symbol.address = 0x8000000; // MRAM start address

	// measure different sizes, up to the maximum per DPU
	for (size_per_dpu = 1<<20; size_per_dpu <= (64 << 20); size_per_dpu <<= 1)
	{ 
		uint8_t *buffer = malloc(size_per_dpu);
		symbol.size = size_per_dpu;
		for (dpus_to_test=1; dpus_to_test <= dpu_count; dpus_to_test++)
		{
    		double total_time=0;
			int transfers=0;

			DPU_ASSERT(dpu_alloc(dpus_to_test, NULL, &dpus));
			DPU_ASSERT(dpu_get_nr_dpus(dpus, &dpu_count));
			if (dpu_count != dpus_to_test) 
			{
				printf("Got %u DPUs, expected %u\n", dpu_count, dpus_to_test);
				return -1;
			}
			double start_time = my_clock();
			while (total_time < 1)
			{
				dpu_copy_to_symbol(dpus, symbol, 0, buffer, size_per_dpu);
				transfers++;
			}
			double end_time = my_clock();
    		total_time = end_time - start_time;
    		double total_size = (double)size_per_dpu * (double)dpu_count;
    		double bd = (total_size * transfers) / (total_time * GIGA);
			printf("transferred %u on %u DPUs (%u times) in %.2fs %.2fGB/s\n", size_per_dpu, dpus_to_test, transfers, total_time, bd);
			DPU_ASSERT(dpu_free(dpus));
		}
		free(buffer);
	}

	return 0;
}
