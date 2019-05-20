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
#include "lower-half-mpi.h"

#undef __MPI_Init
#undef __MPI_Comm_rank
#undef __MPI_Send
#undef __MPI_Recv

static MPI_Comm virtual_to_real_comm(MPI_Comm );
static MPI_Datatype virtual_to_real_type(MPI_Datatype );

int
__MPI_Init(int *argc, char ***argv)
{
  return MPI_SUCCESS;
}

// DEFINE_FNC(int, Comm_rank, (MPI_Comm) comm, (int *) world_rank)
int
__MPI_Comm_rank(MPI_Comm comm, int *world_rank)
{
  int retval;
  DLOG(NOISE, "lower half mpi\n");
  retval = MPI_Comm_rank(virtual_to_real_comm(comm), world_rank);
  return retval;
}

// DEFINE_FNC(int, Send, (const void *) buf, (int) count, (MPI_Datatype) datatype,
//            (int) dest, (int) tag, (MPI_Comm) comm)
int
__MPI_Send(const void *buf, int count, MPI_Datatype datatype,
           int dest, int tag, MPI_Comm comm)
{
  int retval;
  DLOG(NOISE, "lower half mpi\n");
  retval = MPI_Send(buf, count, virtual_to_real_type(datatype),
                    dest, tag, virtual_to_real_comm(comm));
  return retval;
}

// DEFINE_FNC(int, Recv, (void *) buf, (int) count, (MPI_Datatype) datatype,
//            (int) source, (int) tag, (MPI_Comm) comm, (MPI_Status *) status)
int
__MPI_Recv(void *buf, int count, MPI_Datatype datatype,
           int source, int tag, MPI_Comm comm, MPI_Status *status)
{
  int retval;
  DLOG(NOISE, "lower half mpi\n");
  retval = MPI_Recv(buf, count, virtual_to_real_type(datatype),
                    source, tag, virtual_to_real_comm(comm), status);
  return retval;
}


static MPI_Comm
virtual_to_real_comm(MPI_Comm comm)
{
  switch ((uintptr_t)comm) {
    case 0:  return MPI_COMM_NULL; break;
    case 1:  return MPI_COMM_WORLD; break;
    default: return (MPI_Comm)-1;
  }
}

static MPI_Datatype
virtual_to_real_type(MPI_Datatype type)
{
  switch ((uintptr_t)type) {
    case 1:  return MPI_INT; break;
    default: return (MPI_Datatype)-1;
  }
}
