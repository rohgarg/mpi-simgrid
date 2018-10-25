#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>

#include <mpi.h>

#include "common.h"

static void* MPI_Fnc_Ptrs[] = {
  NULL,
  FOREACH_FNC(GENERATE_FNC_PTR)
  NULL,
};

void*
lhDlsym(MPI_Fncs_t fnc)
{
  DLOG(INFO, "LH: Dlsym called with: %d\n", fnc);
  if (fnc < MPI_Fnc_NULL || fnc > MPI_Fnc_Invalid) {
    return NULL;
  }
  void *addr = MPI_Fnc_Ptrs[fnc];
  return addr;
}
