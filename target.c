#include <stdio.h>
#include <unistd.h>

#include <mpi.h>

#include "ckpt-restart.h"
#include "upper-half-mpi-wrappers.h"

static void processArgs(int, const char** );

int
main(int argc, char **argv)
{
  int i = 0;

  processArgs(argc, (const char**)argv);

  int rc = MPI_Init(&argc, &argv);
  printf("App: MPI_Init returned: %d\n", rc);

  while (i < 2) {
    printf("%d ", i);
    fflush(stdout);
    sleep(2);
    i++;
  }
  printf("\n");

  CkptOrRestore_t ret = doCheckpoint();
  if (ret == POST_RESUME) {
    printf("App: Resuming after ckpt...\n");
  } else if (ret == POST_RESTART) {
    printf("App: Restarting from a ckpt...\n");
    printf("App: Calling MPI_Init after restart...\n");
    rc = MPI_Init(&argc, &argv);
    printf("App: MPI_Init returned: %d\n", rc);
  }

  while (i < 4) {
    printf("%d ", i);
    fflush(stdout);
    sleep(2);
    i++;
  }
  printf("\n");

  return 0;
}

static void
processArgs(int argc, const char** argv)
{
  if (argc > 1) {
    printf("App: called with the following args: ");
    for (int j = 1; j < argc; j++) {
      printf("%s ", argv[j]);
    }
    printf("\n");
  }
}
