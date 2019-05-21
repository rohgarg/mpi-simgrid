#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>

#include <mpi.h>

#include "ckpt-restart.h"
#include "upper-half-mpi-wrappers.h"

static void communicate(int rank, int size);

int
main(int argc, char **argv)
{
  int i = 0;
  int rank = -1;
  int size = -1;
  int rc = MPI_Init(&argc, &argv);
  rc = MPI_Comm_size(_MPI_COMM_WORLD, &size);
  rc = MPI_Comm_rank(_MPI_COMM_WORLD, &rank);
  if (rc != MPI_SUCCESS) {
    printf("Could not initialize MPI... Exiting\n");
    return -1;
  }

  while (i < 2) {
    communicate(rank, size);
    sleep(2);
    i++;
  }
  printf("\n");

  CkptOrRestore_t ret = doCheckpoint();
  if (ret == POST_RESUME) {
    printf("Rank %d: Resuming after ckpt...\n", rank);
  } else if (ret == POST_RESTART) {
    printf("Rank %d: Restarting from a ckpt...\n", rank);
  }

  while (i < 4) {
    communicate(rank, size);
    sleep(2);
    i++;
  }
  printf("\n");
  MPI_Finalize();

  return 0;
}

static void
communicate(int rank, int size)
{
  if (size < 2) {
    printf("%d ", rank);
    fflush(stdout);
    return;
  }

  // Communicate along the ring
  int token;
  MPI_Status status;
  int tag = 1;
  if (rank != 0) {
      MPI_Recv(&token, 1, _MPI_INT, rank - 1, tag,
               _MPI_COMM_WORLD, &status);
      printf("Process %d received token %d from process %d\n",
             rank, token, rank - 1);
      fflush(stdout);
  } else {
      // Set the token's value if you are process 0
      token = -1;
  }
  MPI_Send(&token, 1, _MPI_INT, (rank + 1) % size,
           tag, _MPI_COMM_WORLD);

  // Now process 0 can receive from the last process.
  if (rank == 0) {
      MPI_Recv(&token, 1, _MPI_INT, size - 1, tag,
               _MPI_COMM_WORLD, &status);
      printf("Process %d received token %d from process %d\n",
             rank, token, size - 1);
      fflush(stdout);
  }
}
