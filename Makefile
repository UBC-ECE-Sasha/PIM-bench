HOST_BIN:=bandwidth_benchmark
SRCS:=$(wildcard *.c)

.PHONY= all clean run

all: ${HOST_BIN}

clean:
	rm -f ${HOST_BIN}

${HOST_BIN}: ${SRCS}
	clang -O3 -Wall -Wextra `dpu-pkg-config --libs --cflags dpu` -o $@ $^

run: all
	./${HOST_BIN}
