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
#include "mtcp_sys.h"
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
  int mtcp_sys_errno;
  int fd = mtcp_sys_open2(LH_FILE_NAME, O_RDONLY);
  if (fd < 0) {
    DLOG(ERROR, "Could not open addr.bin for reading. Error: %s",
         strerror(mtcp_sys_errno));
    exit(-1);
  }

  int rc = mtcp_sys_read(fd, &lhInfo, sizeof(lhInfo));
  if (rc < sizeof(lhInfo)) {
    DLOG(ERROR, "Read fewer bytes than expected from addr.bin. Error: %s",
         strerror(mtcp_sys_errno));
    exit(-1);
  }
  mtcp_sys_unlink(LH_FILE_NAME);
  mtcp_sys_close(fd);
}
