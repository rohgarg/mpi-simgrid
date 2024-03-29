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
static int getNumRanks(const char **ckptImgs);
static int restoreMemoryForImages(const char** , CkptRestartState_t** , int );

int
restoreCheckpoint(const char **ckptImgs)
{
  int rc = 0;
  int noRanks = getNumRanks(ckptImgs);
  assert(noRanks > 0);

  CkptRestartState_t **ctxs = calloc(noRanks, sizeof(CkptRestartState_t*));
  rc = restoreMemoryForImages(ckptImgs, ctxs, noRanks);
  if (rc < 0) {
    DLOG(ERROR, "Failed to restore MPI ranks. Exiting...\n");
    return rc;
  }
  pthread_t ths[noRanks];
  for (int i = 0; i < noRanks; i++) {
    pthread_create(&ths[i], NULL, startRank, (void*)ctxs[i]);
  }
  for (int i = 0; i < noRanks; i++) {
    pthread_join(ths[i], NULL);
  }
  return rc;
}

static int
getNumRanks(const char **ckptImgs)
{
  int noRanks = 0;
  const char **c = ckptImgs;
  while (*c++) {
    noRanks++;
  }
  return noRanks;
}

static int
restoreMemoryForImages(const char **ckptImgs, CkptRestartState_t **ctxs, int noRanks)
{
  int rc;
  for (int i = 0; i < noRanks; i++) {
    ctxs[i] = calloc(1, sizeof **ctxs);
    rc = restoreMemoryForImage(ckptImgs[i], ctxs[i]);
    if (rc < 0) {
      DLOG(ERROR, "Failed to restore img: %s\n", ckptImgs[i]);
      return -1;
    }
  }
  return 0;
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
