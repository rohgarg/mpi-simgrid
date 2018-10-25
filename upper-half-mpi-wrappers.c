#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mpi.h>

#include "common.h"
#include "upper-half-wrappers.h"
#include "upper-half-mpi-wrappers.h"

#undef MPI_Init
#undef MPI_Comm_rank
#undef MPI_Send
#undef MPI_Recv

// DEFINE_FNC(int, Init, (int *) argc, (char ***) argv)

DEFINE_FNC(int, Comm_rank, (int) group, (int *) world_rank)
// 
// DEFINE_FNC(int, Send, (const void *) buf, (int) count, (MPI_Datatype) datatype,
//            (int) dest, (int) tag, (MPI_Comm) comm)
// 
// DEFINE_FNC(int, Recv, (void *) buf, (int) count, (MPI_Datatype) datatype,
//            (int) source, (int) tag, (MPI_Comm) comm, (MPI_Status *) status)
