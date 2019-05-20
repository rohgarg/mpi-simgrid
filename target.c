#include <stdio.h>
#include <unistd.h>

#include <mpi.h>

#include "ckpt-restart.h"
#include "upper-half-mpi-wrappers.h"

#define _MPI_COMM_WORLD   1
#define _MPI_INT   1

static void processArgs(int, const char** );

int
main(int argc, char **argv)
{
  int i = 0;

  processArgs(argc, (const char**)argv);

  int rank = -1;
  int rc = MPI_Init(&argc, &argv);
  printf("App: MPI_Init returned: %d\n", rc);
  MPI_Comm_rank(_MPI_COMM_WORLD, &rank);

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
    int buf = 17;
    if (rank == 0) {
      MPI_Send(&buf, 1, _MPI_INT, 1, 0, _MPI_COMM_WORLD);
    } else if (rank == 1) {
      MPI_Status status;
      MPI_Recv(&buf, 1, _MPI_INT, 0, 0, _MPI_COMM_WORLD, &status);
    }
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
