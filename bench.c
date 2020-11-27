#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <dpu.h>
#include "bench.h"
#include "PIM-common/common/include/common.h"
#include "PIM-common/host/include/host.h"

#define GIGA ((double)(1 << 30))

static struct option options[] =
{
	{ 0, no_argument, 0, 0 }
};

typedef int(*testfn_ptr)(void);
typedef struct test_entry
{
	char name[32];
	testfn_ptr fn;
} test_entry;

static double
my_clock()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (1.0e-9 * t.tv_nsec + t.tv_sec);
}

int test_throughput(void)
{
	uint32_t dpus_to_test;
	struct dpu_set_t dpus;
	uint32_t dpu_count, rank_count, dpus_per_rank;
	uint32_t size_per_dpu;
  	struct dpu_symbol_t symbol;
	symbol.address = 0x8000000; // MRAM start address

	printf("size,dpus,transfers,time(s),rate(GB/s)\n");

	int status = dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus);
	if (status != DPU_OK)
	{
		fprintf(stderr, "Error %i allocating DPUs\n", status);
		return -3;
	}

	dpu_get_nr_ranks(dpus, &rank_count);
	dpu_get_nr_dpus(dpus, &dpu_count);
	dpus_per_rank = dpu_count/rank_count;
	dbg_printf("Got %u dpus across %u ranks (%u dpus per rank)\n", dpu_count, rank_count, dpus_per_rank);

	// measure different sizes, up to the maximum per DPU
	for (size_per_dpu = 1<<20; size_per_dpu <= (64 << 20); size_per_dpu <<= 1)
	{ 
		fprintf(stderr, "Measuring size %u\n", size_per_dpu);
		uint8_t *buffer = malloc(size_per_dpu);
		symbol.size = size_per_dpu;
		for (dpus_to_test=1; dpus_to_test <= dpu_count; dpus_to_test++)
		{
    		double total_time=0;
			int transfers=0;

			double start_time = my_clock();
			while (total_time < 1)
			{
				dpu_copy_to_symbol(dpus, symbol, 0, buffer, size_per_dpu);
				transfers++;
				double end_time = my_clock();
				total_time += end_time - start_time;
			}
			double total_size = (double)size_per_dpu * (double)dpus_to_test;
    		double bd = (total_size * transfers) / (total_time * GIGA);
			printf("%u,%u,%u,%.2f,%.2f\n", size_per_dpu, dpus_to_test, transfers, total_time, bd);
		}
		free(buffer);
	}

	DPU_ASSERT(dpu_free(dpus));
	return 0;
}

int test_incr(void)
{
	struct timespec start,stop;
	//uint32_t dpu_cycles;
	struct dpu_set_t dpus;
	double frequency = 800  * 1000000 / 3;
	uint32_t rank_count, dpu_count, dpus_per_rank;

	int status = dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus);
	if (status != DPU_OK)
	{
		fprintf(stderr, "Error %i allocating DPUs\n", status);
		return -3;
	}

	dpu_get_nr_ranks(dpus, &rank_count);
	dpu_get_nr_dpus(dpus, &dpu_count);
	dpus_per_rank = dpu_count/rank_count;
	dbg_printf("Got %u dpus across %u ranks (%u dpus per rank)\n", dpu_count, rank_count, dpus_per_rank);

	DPU_ASSERT(dpu_load(dpus, "dpu_incr.bin", NULL));

	printf("%u DPUs at %g MHz with %u tasklets\n", dpu_count, frequency / 1000000, NR_TASKLETS);
	printf("size,DPUs,time(s),rate(MB/s)\n");

	//for (uint32_t dpus_to_test=1; dpus_to_test <= dpu_count; dpus_to_test++)
	{
		clock_gettime(CLOCK_MONOTONIC, &start);
		DPU_ASSERT(dpu_launch(dpus, DPU_SYNCHRONOUS));
		clock_gettime(CLOCK_MONOTONIC, &stop);

/*
		double max_time = 0;
		unsigned long int total_bytes = 0;
		DPU_FOREACH (dpu_set, dpu)
		{
			dpu_cycles = 0;

			// Retrieve tasklet results and compute the final checksum.
			for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++)
			{
            dpu_results_t result;
            dpu_copy_from(dpu, XSTR(dpu_wram_results), each_tasklet * sizeof(dpu_results_t), &result, sizeof(dpu_results_t));

            if (result.cycles > dpu_cycles)
                dpu_cycles = result.cycles;
			total_bytes += result.bytes_read;
			}

			if ((dpu_cycles / frequency) > max_time)
				max_time = dpu_cycles/frequency;
		}

		unsigned long total_mb = total_bytes / (1024 * 1024);
		printf("Total count: %lu\n", total_mb);
*/
		double rank_time = TIME_DIFFERENCE(start, stop);
		printf("Rank processed in %2.2f s\n", rank_time);
	}
	return 0;
}

static test_entry test_table[] =
{
	{ "throughput", test_throughput },
	{ "incr", test_incr },
};

static void usage(void)
{
	printf("usage:\n");
	for (unsigned long entry=0; entry < sizeof(test_table)/sizeof(test_entry); entry++)
		printf("%lu: %s\n", entry, test_table[entry].name);
}

int main(int argc, char** argv)
{
	char *test_name;
	int c;

	// read any command-line options
	while (1)
	{
		c = getopt_long(argc, argv, "", options, 0);
		if (c == -1)
			break;
	}

	if (argc <= optind)
	{
		fprintf(stderr, "Missing test name\n");
		usage();
		return -1;
	}

	test_name = argv[optind];

	fprintf(stderr, "Got test name: %s\n", test_name);

	for (unsigned long entry=0; entry < sizeof(test_table)/sizeof(test_entry); entry++)
	{
		dbg_printf("Entry: %lu %s\n", entry, test_table[entry].name);
		if (strcmp(test_table[entry].name, test_name) == 0)
		{
			test_table[entry].fn();
			break;
		}
	}
	return 0;
}
