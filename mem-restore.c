#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "ckpt-restart.h"
#include "kernel-loader.h"
#include "procmapsutils.h"
#include "utils.h"

static int restoreFs(void *fs);
static int restoreMemory(int );
static int restoreMemoryRegion(int , const Area* );
static void* startRank(void* );
static int restoreMemoryForImage(const char* , CkptRestartState_t* );
static int anyOverlappingRegion(const Area* );

int
restoreCheckpoint(int rank, const char *ckptImg)
{
  int rc = 0;

  CkptRestartState_t *ctx = calloc(1, sizeof(CkptRestartState_t));
  rc = restoreMemoryForImage(ckptImg, ctx);
  if (rc < 0) {
    DLOG(ERROR, "Failed to restore img: %s. Exiting...\n", ckptImg);
    return rc;
  }
  char *ldname  = getenv("TARGET_EXE");
  char exeName[100] = {0};
  snprintf(exeName, 100, "%s-rank%d.exe", ldname, rank);
  patchLowerHalfInfo(exeName);
  startRank((void*)ctx);
  return rc;
}

static int
restoreMemoryForImage(const char *ckptImg, CkptRestartState_t *st)
{
  assert(st);
  int rc = 0;
  int ckptfd = open(ckptImg, O_RDONLY);
  if (ckptfd == -1) {
    DLOG(ERROR, "Unable to open '%s'. Error %s\n",
         ckptImg, strerror(errno));
    return -1;
  }
  rc = readAll(ckptfd, st, sizeof *st);
  assert (rc == sizeof *st);
  rc = restoreMemory(ckptfd);
  close(ckptfd);
  if (rc < 0) {
    DLOG(ERROR, "Failed to restore img: %s\n", ckptImg);
    return -1;
  }
  return 0;
}

static void*
startRank(void *arg)
{
  if (!arg) return NULL;
  CkptRestartState_t *st = (CkptRestartState_t *)arg;
  restoreFs(st->fsAddr);
  // This never returns
  setcontext(&st->ctx);
  return NULL;
}

static int
restoreFs(void *fs)
{
  int rc = syscall(SYS_arch_prctl, ARCH_SET_FS, (uintptr_t)fs);
  if (rc < 0) {
    DLOG(ERROR, "Failed to restore fs(%p) for restart. Error: %s\n",
         fs, strerror(errno));
    return -1;
  }
  return rc;
}

static int
restoreMemory(int ckptfd)
{
  int rc = 0;
  Area area = {0};
  while (!rc && readAll(ckptfd, &area, sizeof area)) {
    rc = restoreMemoryRegion(ckptfd, &area);
  }
  return rc;
}

// Returns 1 in case of area conflicts
static int
anyOverlappingRegion(const Area *area)
{
  const char *PROC_SELF_MAPS = "/proc/self/maps";
  int mapsfd = open(PROC_SELF_MAPS, O_RDONLY);
  if(mapsfd == -1) {
    DLOG(ERROR, "Unable to open '%s'. Error %d.", PROC_SELF_MAPS, errno);
    return 1;
  }
  Area pArea;
  while (readMapsLine(mapsfd, &pArea)) {
    if (doAreasOverlap(pArea.addr, pArea.size, area->addr, area->size)) {
      return 1;
    }
  }
  close(mapsfd);
  return 0;
}

// Returns 0 on success, -1 otherwise
static int
restoreMemoryRegion(int ckptfd, const Area* area)
{
  assert(area != NULL);

  void *addr;
  ssize_t bytes = 0;

  if (anyOverlappingRegion(area)) {
    return -1;
  }

  // Temporarily map with write permissions
  // 
  // NOTE: We mmap using our wrapper to track the upper half regions. This
  // enables the upper half to request for another checkpoint post restart.
  addr = mmapWrapper(area->addr, area->size, area->prot | PROT_WRITE,
                     MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    DLOG(ERROR, "Mapping failed for memory region (%s) at: %p of: %zu bytes. "
         "Error: %s\n", area->name, area->addr, area->size, strerror(errno));
    return -1;
  }
  // Read in the data
  bytes = readAll(ckptfd, area->addr, area->size);
  if (bytes < area->size) {
    DLOG(ERROR, "Read failed for memory region (%s) at: %p of: %zu bytes. "
         "Error: %s\n", area->name, area->addr, area->size, strerror(errno));
    return -1;
  }
  // Restore region permissions
  int rc = mprotect(area->addr, area->size, area->prot);
  if (rc < 0) {
    DLOG(ERROR, "Failed to restore perms for memory region (%s) at: %p "
         "of: %zu bytes. Error: %s\n",
         area->name, area->addr, area->size, strerror(errno));
    return -1;
  }
  return 0;
}
