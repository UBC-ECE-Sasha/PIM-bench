#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <dpu.h>
#include "bench.h"
#include "PIM-common/host/include/host.h"
#include "PIM-common/common/include/common.h"

static struct option options[] =
{
	{ 0, no_argument, 0, 0 }
};

typedef int(*testfn_ptr)(struct dpu_set_t dpus);
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

int test_throughput(struct dpu_set_t dpus)
{
	uint32_t dpus_to_test;
	uint32_t dpu_count, rank_count, dpus_per_rank;
	uint32_t size_per_dpu;
  	struct dpu_symbol_t symbol;
	symbol.address = 0x8000000; // MRAM start address

	printf("size,dpus,transfers,time(s),rate(GB/s)\n");

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
    		double bd = (total_size * transfers) / (total_time * GIGABYTE(1));
			printf("%u,%u,%u,%.2f,%.2f\n", size_per_dpu, dpus_to_test, transfers, total_time, bd);
		}
		free(buffer);
	}

	return 0;
}

int test_incr(struct dpu_set_t dpus)
{
	struct timespec start_1, setup_2, launch_3, gather_4, stop_5;
	//uint32_t dpu_cycles;
	//struct dpu_set_t dpu_rank;
	struct dpu_set_t dpu;
	double frequency = 800  * 1000000 / 3;
	uint32_t rank_count, dpu_count, dpus_per_rank;
	block_t blocks[NR_TASKLETS];
	uint32_t results[NR_TASKLETS + 1];
	uint64_t total_size = GIGABYTE(2UL);
	uint64_t size_per_dpu;

	dpu_get_nr_ranks(dpus, &rank_count);
	dpu_get_nr_dpus(dpus, &dpu_count);
	dpus_per_rank = dpu_count/rank_count;
	dbg_printf("Got %u dpus across %u ranks (%u dpus per rank)\n", dpu_count, rank_count, dpus_per_rank);

	printf("Processing %lu total data\n", total_size);
	size_per_dpu = total_size / dpu_count;
	printf("Processing %lu per DPU\n", size_per_dpu);

	DPU_ASSERT(dpu_load(dpus, "dpu_incr.bin", NULL));

	printf("%u DPUs at %g MHz with %u tasklets\n", dpu_count, frequency / 1000000, NR_TASKLETS);

	for (uint32_t tasklet=0; tasklet < NR_TASKLETS; tasklet++)
	{
		blocks[tasklet].start = (size_per_dpu / NR_TASKLETS) * tasklet;
		blocks[tasklet].end = blocks[tasklet].start + (size_per_dpu / NR_TASKLETS) - 1;
		dbg_printf("tasklet %i start=0x%x end=0x%x\n", tasklet, blocks[tasklet].start, blocks[tasklet].end);
	}
	printf("Total size per DPU: %u\n", blocks[NR_TASKLETS-1].end - blocks[0].start);
	if ((blocks[NR_TASKLETS-1].end - blocks[0].start) != size_per_dpu)
	{
		printf("Mismatch in size per DPU (expected %lu saw %u)\n", size_per_dpu, blocks[NR_TASKLETS-1].end - blocks[0].start);
	}

	uint64_t total_bytes = 0;
	float total_time = 0;
	uint32_t dpu_cycles = 0;
	uint32_t *buffer = (uint32_t*)calloc(sizeof(uint32_t), total_size/sizeof(uint32_t));
	uint8_t *per_dpu;

	// start the trial
	clock_gettime(CLOCK_MONOTONIC, &start_1);

	// copy data in
	int err;
	per_dpu = (uint8_t*)buffer;
	DPU_FOREACH(dpus, dpu)
	{
      err = dpu_prepare_xfer(dpu, (void*)per_dpu);
      if (err != DPU_OK)
      {
         dbg_printf("Error %u preparing xfer\n", err);
         return -1;
      }
		per_dpu += size_per_dpu;
	}
   err = dpu_push_xfer(dpu, DPU_XFER_TO_DPU, "input_buffer", 0, ALIGN(size_per_dpu, 8), DPU_XFER_DEFAULT);
   if (err != DPU_OK)
   {
      dbg_printf("Error %u pushing buffer\n", err);
      return -1;
   }

	clock_gettime(CLOCK_MONOTONIC, &setup_2);
	dpu_copy_to(dpus, "blocks", 0, &blocks, sizeof(blocks));
	clock_gettime(CLOCK_MONOTONIC, &launch_3);
	DPU_ASSERT(dpu_launch(dpus, DPU_SYNCHRONOUS));
	clock_gettime(CLOCK_MONOTONIC, &gather_4);
	per_dpu = (uint8_t*)buffer;
	DPU_FOREACH(dpus, dpu)
	{
      err = dpu_prepare_xfer(dpu, (void*)per_dpu);
      if (err != DPU_OK)
      {
         dbg_printf("Error %u preparing xfer\n", err);
         return -1;
      }
		per_dpu += size_per_dpu;
	}
   err = dpu_push_xfer(dpu, DPU_XFER_FROM_DPU, "input_buffer", 0, ALIGN(size_per_dpu, 8), DPU_XFER_DEFAULT);
   if (err != DPU_OK)
   {
      dbg_printf("Error %u getting buffer\n", err);
      return -1;
   }
	//DPU_FOREACH(dpus, dpu)
	//	prepare_xfer();
	clock_gettime(CLOCK_MONOTONIC, &stop_5);

	// add up the total bytes processed
	DPU_FOREACH(dpus, dpu)
	{
		dpu_copy_from(dpu, "results", 0, &results, sizeof(results));
		for (uint32_t tasklet=0; tasklet < NR_TASKLETS; tasklet++)
			total_bytes += (blocks[tasklet].end - blocks[tasklet].start) + 1;
			//total_bytes += results[tasklet];

		if (results[NR_TASKLETS] > dpu_cycles)
			dpu_cycles = results[NR_TASKLETS];

#ifdef DEBUG_DPU
		// get any DPU debug messages
		int err = dpu_log_read(dpu, stdout);
		if (err != DPU_OK)
		{
			dbg_printf("Error %u retrieving log\n", err);
		}
#endif // DEBUG_DPU
	}

	total_time = (float)dpu_cycles/(float)frequency;
	printf("Total bytes processed: %lu in %f s\n", total_bytes, total_time);
	printf("Throughput: %f MB/s\n", total_bytes/total_time/MEGABYTE(1));

	double host_time = TIME_DIFFERENCE(start_1, setup_2);
	printf("Copy time: %f\n", host_time);
	host_time = TIME_DIFFERENCE(setup_2, launch_3);
	printf("Setup time: %f\n", host_time);
	host_time = TIME_DIFFERENCE(launch_3, gather_4);
	printf("Launch time: %f\n", host_time);
	host_time = TIME_DIFFERENCE(gather_4, stop_5);
	printf("Gather time: %f\n", host_time);
	host_time = TIME_DIFFERENCE(start_1, stop_5);
	printf("Measured by host in %2.2f s\n", host_time);
	printf("Throughput: %f MB/s\n", total_bytes/host_time/MEGABYTE(1));
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
	struct dpu_set_t dpus;

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

	// allocate the DPUs
	int status = dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus);
	if (status != DPU_OK)
	{
		fprintf(stderr, "Error %i allocating DPUs\n", status);
		return -3;
	}


	for (unsigned long entry=0; entry < sizeof(test_table)/sizeof(test_entry); entry++)
	{
		dbg_printf("Entry: %lu %s\n", entry, test_table[entry].name);
		if (strcmp(test_table[entry].name, test_name) == 0)
		{
			test_table[entry].fn(dpus);
			break;
		}
	}

	DPU_ASSERT(dpu_free(dpus));
	return 0;
}
