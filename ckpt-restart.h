#ifndef CKPT_RESTART_H
#define CKPT_RESTART_H

#include <signal.h>
#include <ucontext.h>

typedef char* VA;

#define CKPT_SIGNAL  SIGUSR2

typedef struct __CkptRestartState
{
  ucontext_t ctx;
  void *sp;
  void *fsAddr; // fs is perhaps not saved as part of getcontext
} CkptRestartState_t;

typedef enum __CkptOrRestore
{
  RUNNING,
  POST_RESUME,
  POST_RESTART,
} CkptOrRestore_t;

// Public API:
//   Returns POST_RESUME, if resuming from a checkpoint,
//   Returns POST_RESTORE, if restarting from a checkpoint
CkptOrRestore_t doCheckpoint() __attribute__((weak));
#define doCheckpoint() (doCheckpoint ? doCheckpoint() : 0)

int restoreCheckpoint(int , const char *);

// Returns true if needle is in the haystack
static inline int
regionContains(const void *haystackStart,
               const void *haystackEnd,
               const void *needleStart,
               const void *needleEnd)
{
  return needleStart >= haystackStart && needleEnd <= haystackEnd;
}

static inline int
doAreasOverlap(VA addr1, size_t size1, VA addr2, size_t size2)
{
  VA end1 = (VA)addr1 + size1;
  VA end2 = (VA)addr2 + size2;

  return (addr1 >= addr2 && addr1 < end2) || (addr2 >= addr1 && addr2 < end1);
}

#endif // ifndef CKPT_RESTART_H
