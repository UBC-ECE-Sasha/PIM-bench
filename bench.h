#ifndef _COMMON__H
#define _COMMON__H

#define BUFFER_SIZE (64 << 20)

typedef struct {
    uint32_t checksum;
    uint32_t cycles;
    uint32_t bytes_read;
} dpu_results_t;

#endif // _COMMON__H
