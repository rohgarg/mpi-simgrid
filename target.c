#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>

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
  struct timeval start, end;

  processArgs(argc, (const char**)argv);

  int rank = -1;
  int size = -1;
  int rc = MPI_Init(&argc, &argv);
  rc = MPI_Comm_size(_MPI_COMM_WORLD, &size);
  rc = MPI_Comm_rank(_MPI_COMM_WORLD, &rank);
  if (rc != MPI_SUCCESS) {
    printf("Could not initialize MPI... Exiting\n");
    return -1;
  }

  // Communicate along the ring
  while (i < 2) {
    int buf = 17;
    MPI_Status status;
    if (rank == 0) {
      gettimeofday(&start, NULL);
      printf("Rank %d: sending the message rank %d\n", rank, 1);
      fflush(stdout);
      MPI_Send(&buf, 1, _MPI_INT, 1, 1, _MPI_COMM_WORLD);
      MPI_Recv(&buf, 1, _MPI_INT, size - 1, 1, _MPI_COMM_WORLD, &status);
      printf("Rank %d: received the message from rank %d\n", rank, size - 1);
      fflush(stdout);
      gettimeofday(&end, NULL);
      printf("%f\n", (end.tv_sec * 1000000.0 + end.tv_usec - start.tv_sec * 1000000.0 - start.tv_usec) / 1000000.0);
    } else {
      MPI_Recv(&buf, 1, _MPI_INT, rank - 1, 1, _MPI_COMM_WORLD, &status);
      printf("Rank %d: receive the message and sending it to rank %d\n", rank,
             (rank + 1) % size);
      fflush(stdout);
      MPI_Send(&buf, 1, _MPI_INT, (rank + 1) % size, 1, _MPI_COMM_WORLD);
    }
    sleep(2);
    i++;
  }

  printf("\n");

  CkptOrRestore_t ret = doCheckpoint();
  if (ret == POST_RESUME) {
    printf("[Rank-%d]: Resuming after ckpt...\n", rank);
  } else if (ret == POST_RESTART) {
    printf("[Rank-%d]: Restarting from a ckpt...\n", rank);
  }

  while (i < 4) {
    int buf = 17;
    MPI_Status status;
    if (rank == 0) {
      gettimeofday(&start, NULL);
      printf("Rank %d: sending the message rank %d\n", rank, 1);
      fflush(stdout);
      MPI_Send(&buf, 1, _MPI_INT, 1, 1, _MPI_COMM_WORLD);
      MPI_Recv(&buf, 1, _MPI_INT, size - 1, 1, _MPI_COMM_WORLD, &status);
      printf("Rank %d: received the message from rank %d\n", rank, size - 1);
      gettimeofday(&end, NULL);
      fflush(stdout);
      printf("%f\n", (end.tv_sec * 1000000.0 + end.tv_usec - start.tv_sec * 1000000.0 - start.tv_usec) / 1000000.0);
    } else {
      MPI_Recv(&buf, 1, _MPI_INT, rank - 1, 1, _MPI_COMM_WORLD, &status);
      printf("Rank %d: receive the message and sending it to rank %d\n", rank,
             (rank + 1) % size);
      fflush(stdout);
      MPI_Send(&buf, 1, _MPI_INT, (rank + 1) % size, 1, _MPI_COMM_WORLD);
    }
    sleep(2);
    i++;
  }
  printf("\n");
  MPI_Finalize();

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
