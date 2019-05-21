#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"
#include "upper-half-wrappers.h"

int initialized = 0;

static void readLhInfoAddr();

LowerHalfInfo_t lhInfo = {0};

void*
__sbrk(intptr_t increment)
{
  return sbrk(increment);
}

void*
sbrk(intptr_t increment)
{
  static __typeof__(&sbrk) lowerHalfSbrkWrapper = (__typeof__(&sbrk)) - 1;
  if (!initialized) {
    initialize_wrappers();
  }
  if (lowerHalfSbrkWrapper == (__typeof__(&sbrk)) - 1) {
    lowerHalfSbrkWrapper = (__typeof__(&sbrk))lhInfo.lhSbrk;
  }
  // TODO: Switch fs context
  return lowerHalfSbrkWrapper(increment);
}

void*
mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  static __typeof__(&mmap) lowerHalfMmapWrapper = (__typeof__(&mmap)) - 1;
  if (!initialized) {
    initialize_wrappers();
  }
  if (lowerHalfMmapWrapper == (__typeof__(&mmap)) - 1) {
    lowerHalfMmapWrapper = (__typeof__(&mmap))lhInfo.lhMmap;
  }
  // TODO: Switch fs context
  return lowerHalfMmapWrapper(addr, length, prot, flags, fd, offset);
}

int exitWrapper(int status) __attribute__((destructor));

int
exitWrapper(int status)
{
  DLOG(SUPERNOISE, "In wrapper\n");
  int retval;
  if (!initialized) {
    initialize_wrappers();
  }
  JUMP_TO_LOWER_HALF(lhInfo.lhFsAddr);
  retval = ({__typeof__(&exitWrapper)_lh_exit = (__typeof__(&exitWrapper)) -1;
  if (_lh_exit == (__typeof__(&exitWrapper)) -1) {
    LhDlsym_t dlsymFptr = (LhDlsym_t)lhInfo.lhDlsym;
    _lh_exit = (__typeof__(&exitWrapper))dlsymFptr(MPI_Fnc_Exit);
  } _lh_exit; })(status);
  RETURN_TO_UPPER_HALF();
  return retval;
}

void
initialize_wrappers()
{
  if (!initialized) {
    readLhInfoAddr();
    initialized = 1;
  }
}

void
reset_wrappers()
{
  initialized = 0;
}

static void
readLhInfoAddr()
{
  DLOG(INFO, "Upper half initialized");
}
