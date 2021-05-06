#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <dpu.h>

#define GIGA ((double)(1 << 30))

#define NB_SIZES (18)
#define FIRST_SIZE_LOG2 (3)
#define FOREACH_SIZE(each_size, size)                                                                                            \
    for (uint32_t each_size = 0, size = (1 << FIRST_SIZE_LOG2); each_size < NB_SIZES;                                            \
         each_size++, size = (1 << (each_size + FIRST_SIZE_LOG2)))

#define NB_THREADS (5)
const uint32_t threads[NB_THREADS] = { 0, 1, 2, 4, 8 }; // 0 is for WRAM

double mesures[NB_SIZES][NB_THREADS];

static double
my_clock()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (1.0e-9 * t.tv_nsec + t.tv_sec);
}

static void
benchmark(uint32_t total_size,
    uint32_t xfer_size,
    uint32_t nr_thread_per_pool,
    uint32_t addr,
    double *bd,
    double *time,
    double *xfered_size)
{
    struct dpu_set_t dpu_set, rank;
    struct dpu_symbol_t symbol;
    uint32_t nb_loop = total_size / xfer_size;
    uint32_t nr_dpus;
    char profile[FILENAME_MAX];
    uint8_t *buffer = malloc(xfer_size);
    assert(buffer != NULL);

    sprintf(profile,
        "poolThreshold1Thread=0,poolThreshold2Threads=0,"
        "poolThreshold4Threads=0,nrThreadPerPool=%u",
        nr_thread_per_pool);
    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, profile, &dpu_set));
    DPU_RANK_FOREACH(dpu_set, rank) break;
    DPU_ASSERT(dpu_get_nr_dpus(rank, &nr_dpus));
    symbol.address = addr;
    symbol.size = xfer_size;

    double start_time = my_clock();
    for (uint32_t each_loop = 0; each_loop < nb_loop; each_loop++) {
        dpu_copy_to_symbol(rank, symbol, 0, buffer, xfer_size);
    }
    double end_time = my_clock();

    *time = end_time - start_time;
    *xfered_size = (double)total_size * (double)nr_dpus;
    *bd = *xfered_size / (*time * GIGA);

    free(buffer);
    DPU_ASSERT(dpu_free(dpu_set));
}

static void
benchmark_thread(uint32_t total_size, uint32_t thread, uint32_t thread_id, uint32_t addr)
{
    FOREACH_SIZE(each_size, size)
    {
        printf("[%u, %u]:\n", thread, size);
        if (addr == 0 && size > (64 << 10)) {
            printf("\tN/A\n");
            mesures[each_size][thread_id] = 0.0;
            continue;
        }
        double bd, time, xfered_size;
        while (true) {
            benchmark(total_size, size, thread, addr, &bd, &time, &xfered_size);
            if (time < 1.0)
                total_size *= 2;
            else
                break;
        }
        printf("\t%.3fGB/s, %.2fs, %.3fGB\n", bd, time, xfered_size / GIGA);
        mesures[each_size][thread_id] = bd;
    }
}

int
main()
{
    // WRAM
    benchmark_thread(1 << 20, 0, 0, 0x00000000);
    // MRAM
    for (uint32_t each_thread = 1; each_thread < NB_THREADS; each_thread++) {
        benchmark_thread(1 << 20, threads[each_thread], each_thread, 0x08000000);
    }

    printf("GB/s");
    FOREACH_SIZE(each_size, size) { printf(", %u", size); }
    printf("\n");

    for (uint32_t each_thread = 0; each_thread < NB_THREADS; each_thread++) {
        printf("%u", threads[each_thread]);
        FOREACH_SIZE(each_size, size) { printf(", %.3f", mesures[each_size][each_thread]); }
        printf("\n");
    }

    return 0;
}
