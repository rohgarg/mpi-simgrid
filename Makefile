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

CFLAGS=-g3 -O0 -fPIC -I. -I${MPI_INCLUDE_PATH} -c -std=gnu11 -Wall -Werror
KERNEL_LOADER_CFLAGS=-DSTANDALONE
MPICC_FLAGS=-Wl,-Ttext-segment -Wl,0xF00000

KERNEL_LOADER_BIN=kernel-loader.exe
TARGET_BIN=t.exe
TARGET_PRELOAD_LIB=libuhwrappers.so

SIMGRID_OBJS=${FILE}.o procmapsutils.o custom-loader.o mmap-wrapper.o sbrk-wrapper.o mpi-lh-if.o mem-restore.o utils.o trampoline_setup.o lower-half-mpi.c
SIMGRID_BIN=simgrid.exe

ckpt.img: ${KERNEL_LOADER_BIN} ${TARGET_BIN} ${TARGET_PRELOAD_LIB} disableASLR
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=1 ./$< $$PWD/${TARGET_BIN} arg1 arg2 arg3

run: ${KERNEL_LOADER_BIN} ${TARGET_BIN} ${TARGET_PRELOAD_LIB} disableASLR
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=1 ./$< $$PWD/${TARGET_BIN} arg1 arg2 arg3

gdb: ${KERNEL_LOADER_BIN} ${TARGET_BIN} ${TARGET_PRELOAD_LIB}
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=1 gdb --args ./$< $$PWD/${TARGET_BIN} arg1 arg2 arg3

restart: ${KERNEL_LOADER_BIN} ckpt.img
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=1 ./$< --restore ./ckpt.img

restart-simgrid: ${SIMGRID_BIN} ckpt.img
	UH_PRELOAD=$$PWD/${TARGET_PRELOAD_LIB} TARGET_LD=${RTLD_PATH} RANK_ID=1 ./$< --restore ./ckpt.img

disableASLR:
	@- [ `cat /proc/sys/kernel/randomize_va_space` == 0 ] || sudo sh -c 'echo 0 > /proc/sys/kernel/randomize_va_space'

enableASLR:
	@- [ `cat /proc/sys/kernel/randomize_va_space` -ne 2 ] && sudo sh -c 'echo 2 > /proc/sys/kernel/randomize_va_space'

.c.o:
	${CC} ${CFLAGS} $< -o $@

${FILE}.o: ${FILE}.c
	${CC} ${CFLAGS} ${KERNEL_LOADER_CFLAGS} $< -o $@

${TARGET_BIN}: ${TARGET_OBJS}
	${LD} $< -o $@

${TARGET_PRELOAD_LIB}: ${TARGET_PRELOAD_LIB_OBJS}
	${LD} -shared $^ -o $@

${KERNEL_LOADER_BIN}: ${KERNEL_LOADER_OBJS}
	${MPICC} -static ${MPICC_FLAGS} $^ -o $@ -lpthread

${SIMGRID_BIN}: ${SIMGRID_OBJS}
	${LD} -static ${MPICC_FLAGS} $^ -o $@ -lpthread

vi vim:
	vim ${FILE}.c

tags:
	gtags .

dist: clean
	(dir=`basename $$PWD` && cd .. && tar zcvf $$dir.tgz $$dir)
	(dir=`basename $$PWD` && ls -l ../$$dir.tgz)

tidy:
	rm -f ./ckpt.img ./addr.bin

clean: tidy
	rm -f ${KERNEL_LOADER_OBJS} ${TARGET_OBJS} ${KERNEL_LOADER_BIN} \
	      ${TARGET_BIN} ${TARGET_PRELOAD_LIB_OBJS} ${TARGET_PRELOAD_LIB} \
		  GTAGS GRTAGS GPATH

.PHONY: dist vi vim clean gdb tags tidy restart run enableASLR disableASLR restart-simgrid
