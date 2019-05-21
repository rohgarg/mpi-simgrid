# NOTE: Update the following variables for your system
CC=gcc
LD=gcc
MPICC=mpicc
RTLD_PATH=/lib64/ld-2.27.so
MPI_INCLUDE_PATH=/usr/local/include

FILE=kernel-loader
KERNEL_LOADER_OBJS=${FILE}.o procmapsutils.o custom-loader.o mmap-wrapper.o sbrk-wrapper.o mpi-lh-if.o mem-restore.o utils.o trampoline_setup.o lower-half-mpi.o
TARGET_OBJS=target.o
TARGET_PRELOAD_LIB_OBJS=upper-half-wrappers.o upper-half-mpi-wrappers.o mem-ckpt.o procmapsutils.o utils.o trampoline_setup.o

CFLAGS=-g3 -O0 -fPIC -I. -I${MPI_INCLUDE_PATH} -c -std=gnu11 -Wall -Werror -DDEBUG_LEVEL=ERROR
CFLAGS_SIMGRID=-g3 -O0 -fPIC -I. -c -std=gnu11 -Wall -Werror -DDEBUG_LEVEL=ERROR
KERNEL_LOADER_CFLAGS=-DSTANDALONE
MPICC_FLAGS=-Wl,-Ttext-segment -Wl,0xF00000

KERNEL_LOADER_BIN=kernel-loader.exe
TARGET_BIN=t.exe
TARGET_PRELOAD_LIB=libuhwrappers.so

SIMGRID_OBJS=${FILE}-simgrid.o procmapsutils.o custom-loader.o mmap-wrapper.o sbrk-wrapper.o mpi-lh-if.o mem-restore.o utils.o trampoline_setup.o lower-half-mpi-simgrid.o
SIMGRID_BIN=kernel-loader-dyn.exe

STATIC_TARGET_BIN=static-t
STATIC_TARGET_BIN_RANK0=${STATIC_TARGET_BIN}-rank0.exe
STATIC_TARGET_BIN_RANK1=${STATIC_TARGET_BIN}-rank1.exe
STATIC_TARGET_BIN_RANK0_LDFLAGS=-static -Wl,-Ttext-segment -Wl,0xE000000
STATIC_TARGET_BIN_RANK1_LDFLAGS=-static -Wl,-Ttext-segment -Wl,0xF000000
STATIC_TARGET_OBJS=${TARGET_OBJS} ${TARGET_PRELOAD_LIB_OBJS}

run: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0} ${STATIC_TARGET_BIN_RANK1} disableASLR
	TARGET_EXE=${STATIC_TARGET_BIN} mpirun -n 2 ./$< arg1 arg2 arg3

run0: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0} disableASLR
	TARGET_EXE=${STATIC_TARGET_BIN} ./$< arg1 arg2 arg3

gdb: ${KERNEL_LOADER_BIN} ${STATIC_TARGET_BIN_RANK0}
	TARGET_EXE=${STATIC_TARGET_BIN} gdb --args ./$< arg1 arg2 arg3

restart0: ${KERNEL_LOADER_BIN} rank_0_ckpt.img
	TARGET_EXE=${STATIC_TARGET_BIN} ./$< --restore ./rank_0_ckpt.img

restart-simgrid0: ${SIMGRID_BIN} rank_0_ckpt.img
	TARGET_EXE=${STATIC_TARGET_BIN} smpirun -gdb -np 1 -platform cluster_backbone.xml kernel-loader-dyn.exe --restore ./rank_0_ckpt.img

restart-simgrid: ${SIMGRID_BIN} ./rank_0_ckpt.img ./rank_1_ckpt.img
	TARGET_EXE=${STATIC_TARGET_BIN} smpirun -np 2 -platform cluster_backbone.xml kernel-loader-dyn.exe --restore ./rank_0_ckpt.img ./rank_1_ckpt.img

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

lower-half-mpi.o: lower-half-mpi.c
	${CC} ${CFLAGS} ${KERNEL_LOADER_CFLAGS} $< -o $@

lower-half-mpi-simgrid.o: lower-half-mpi.c
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
				${FILE}-simgrid.o \
		  GTAGS GRTAGS GPATH

.PHONY: dist vi vim clean gdb tags tidy restart run run0 enableASLR disableASLR restart-simgrid
