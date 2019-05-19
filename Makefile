# NOTE: Update the following variables for your system
CC=gcc
LD=gcc
MPICC=mpicc
RTLD_PATH=/lib64/ld-2.27.so
MPI_INCLUDE_PATH=/usr/local/include

FILE=kernel-loader
KERNEL_LOADER_OBJS=${FILE}.o procmapsutils.o custom-loader.o mmap-wrapper.o sbrk-wrapper.o mpi-lh-if.o mem-restore.o utils.o trampoline_setup.o
TARGET_OBJS=target.o
TARGET_PRELOAD_LIB_OBJS=upper-half-wrappers.o upper-half-mpi-wrappers.o mem-ckpt.o procmapsutils.o utils.o trampoline_setup.o

CFLAGS=-g3 -O0 -fPIC -I. -I${MPI_INCLUDE_PATH} -c -std=gnu11 -Wall -Werror -DDEBUG_LEVEL=ERROR
CFLAGS_SIMGRID=-g3 -O0 -fPIC -I. -c -std=gnu11 -Wall -Werror -DDEBUG_LEVEL=ERROR
KERNEL_LOADER_CFLAGS=-DSTANDALONE
MPICC_FLAGS=-Wl,-Ttext-segment -Wl,0xF00000

KERNEL_LOADER_BIN=kernel-loader.exe
TARGET_BIN=t.exe
TARGET_PRELOAD_LIB=libuhwrappers.so

SIMGRID_OBJS=${FILE}-simgrid.o procmapsutils.o custom-loader.o mmap-wrapper.o sbrk-wrapper.o mpi-lh-if.o mem-restore.o utils.o trampoline_setup.o
SIMGRID_BIN=kernel-loader-dyn.exe

STATIC_TARGET_BIN_RANK0=static-t-rank0.exe
STATIC_TARGET_BIN_RANK1=static-t-rank1.exe
STATIC_TARGET_BIN_RANK0_LDFLAGS=-static -Wl,-Ttext-segment -Wl,0xE000000
STATIC_TARGET_BIN_RANK1_LDFLAGS=-static -Wl,-Ttext-segment -Wl,0xF000000
STATIC_TARGET_BIN_RANK0_HEAP=0xA0000000
STATIC_TARGET_BIN_RANK1_HEAP=0xB0000000
STATIC_TARGET_BIN_RANK0_STACK=0xC0000000
STATIC_TARGET_BIN_RANK1_STACK=0xD0000000
STATIC_TARGET_OBJS=${TARGET_OBJS} ${TARGET_PRELOAD_LIB_OBJS}

run: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0} ${STATIC_TARGET_BIN_RANK1} disableASLR
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_EXE=${STATIC_TARGET_BIN_RANK0} RANK_ID=0 RANK_HEAP=${STATIC_TARGET_BIN_RANK0_HEAP} RANK_STACK=${STATIC_TARGET_BIN_RANK0_STACK} ./$< arg1 arg2 arg3 &
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_EXE=${STATIC_TARGET_BIN_RANK1} RANK_ID=1 RANK_HEAP=${STATIC_TARGET_BIN_RANK1_HEAP} RANK_STACK=${STATIC_TARGET_BIN_RANK1_STACK} ./$< arg1 arg2 arg3

run0: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0} disableASLR
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_EXE=${STATIC_TARGET_BIN_RANK0} RANK_ID=0 RANK_HEAP=${STATIC_TARGET_BIN_RANK0_HEAP} RANK_STACK=${STATIC_TARGET_BIN_RANK0_STACK} ./$< arg1 arg2 arg3

run1: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK1} disableASLR
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_EXE=${STATIC_TARGET_BIN_RANK1} RANK_ID=1 RANK_HEAP=${STATIC_TARGET_BIN_RANK1_HEAP} RANK_STACK=${STATIC_TARGET_BIN_RANK1_STACK} ./$< arg1 arg2 arg3

gdb: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0}
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_EXE=${STATIC_TARGET_BIN_RANK0} RANK_ID=0 RANK_HEAP=${STATIC_TARGET_BIN_RANK0_HEAP} RANK_STACK=${STATIC_TARGET_BIN_RANK0_STACK} gdb --args ./$< arg1 arg2 arg3

restart: ${KERNEL_LOADER_BIN} rank_0_ckpt.img
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=0 ./$< --restore ./rank_0_ckpt.img

restart-simgrid: ${SIMGRID_BIN} ./rank_0_ckpt.img ./rank_1_ckpt.img
	smpirun -gdb -np 2 -platform cluster_backbone.xml kernel-loader-dyn.exe --restore ./rank_0_ckpt.img ./rank_1_ckpt.img

disableASLR:
	@- [ `cat /proc/sys/kernel/randomize_va_space` == 0 ] || sudo sh -c 'echo 0 > /proc/sys/kernel/randomize_va_space'

enableASLR:
	@- [ `cat /proc/sys/kernel/randomize_va_space` -ne 2 ] && sudo sh -c 'echo 2 > /proc/sys/kernel/randomize_va_space'

.c.o:
	${CC} ${CFLAGS} $< -o $@

${FILE}.o: ${FILE}.c
	${CC} ${CFLAGS} ${KERNEL_LOADER_CFLAGS} $< -o $@

${FILE}-simgrid.o: ${FILE}.c
	smpicc ${CFLAGS_SIMGRID} ${KERNEL_LOADER_CFLAGS} $< -o $@

${TARGET_BIN}: ${TARGET_OBJS}
	${LD} $< -o $@

${STATIC_TARGET_BIN_RANK0}: ${STATIC_TARGET_OBJS}
	${LD} ${STATIC_TARGET_BIN_RANK0_LDFLAGS} $^ -o $@

${STATIC_TARGET_BIN_RANK1}: ${STATIC_TARGET_OBJS}
	${LD} ${STATIC_TARGET_BIN_RANK1_LDFLAGS} $^ -o $@

${TARGET_PRELOAD_LIB}: ${TARGET_PRELOAD_LIB_OBJS}
	${LD} -shared $^ -o $@

${KERNEL_LOADER_BIN}: ${KERNEL_LOADER_OBJS}
	${MPICC} -static ${MPICC_FLAGS} $^ -o $@ -lpthread

${SIMGRID_BIN}: ${SIMGRID_OBJS}
	smpicc $^ -o $@

vi vim:
	vim ${FILE}.c

tags:
	gtags .

dist: clean
	(dir=`basename $$PWD` && cd .. && tar zcvf $$dir.tgz $$dir)
	(dir=`basename $$PWD` && ls -l ../$$dir.tgz)

tidy:
	rm -f *.img *.bin *.so smpitmp-*

clean: tidy
	rm -f ${KERNEL_LOADER_OBJS} ${TARGET_OBJS} ${KERNEL_LOADER_BIN} \
	      ${TARGET_BIN} ${TARGET_PRELOAD_LIB_OBJS} ${TARGET_PRELOAD_LIB} \
				${SIMGRID_BIN} ${STATIC_TARGET_BIN_RANK0} ${STATIC_TARGET_BIN_RANK1} \
		  GTAGS GRTAGS GPATH

.PHONY: dist vi vim clean gdb tags tidy restart run run0 enableASLR disableASLR restart-simgrid
