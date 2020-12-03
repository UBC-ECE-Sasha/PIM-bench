HOST_BIN:=bench
SRCS:=bench.c
DPU_INCR_SRCS:=dpu_incr.c

# define DEBUG in the source if we are debugging
ifeq ($(DEBUG_CPU), 1)
	CFLAGS+=-DDEBUG
endif

ifeq ($(DEBUG_DPU), 1)
	CFLAGS+=-DDEBUG_DPU
endif

# Default NR_TASKLETS
NR_TASKLETS = 16

COMMON_FLAGS := -Wall -Wextra -Werror -g -I${COMMON_INCLUDES}
HOST_FLAGS := ${COMMON_FLAGS} -std=c11 -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}

.PHONY= all clean run

all: ${HOST_BIN} dpu_incr.bin

clean:
	rm -f ${HOST_BIN} dpu_incr.bin

${HOST_BIN}: ${SRCS}
	clang ${HOST_FLAGS} -DDPU_BINARY='dpu_incr.bin' -o $@ $^

dpu_incr.bin: ${DPU_INCR_SRCS}
	dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_INCR_SRCS}

run: all
	./${HOST_BIN}
