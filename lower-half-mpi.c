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

#include "ckpt-restart.h"
#include "common.h"
#include "lower-half-mpi.h"

#undef __MPI_Init
#undef __MPI_Finalize
#undef __MPI_Comm_rank
#undef __MPI_Comm_size
#undef __MPI_Send
#undef __MPI_Recv
#undef __MPI_Exit

static MPI_Comm virtual_to_real_comm(MPI_Comm );
static MPI_Datatype virtual_to_real_type(MPI_Datatype );

int
__MPI_Init(int *argc, char ***argv)
{
  return MPI_SUCCESS;
}

int
__MPI_Finalize()
{
  int retval;
  DLOG(NOISE, "lower half mpi\n");
  retval = MPI_Finalize();
  return retval;
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

// DEFINE_FNC(int, Comm_size, (MPI_Comm) comm, (int *) world_size)
int
__MPI_Comm_size(MPI_Comm comm, int *world_size)
{
  int retval;
  DLOG(NOISE, "lower half mpi\n");
  retval = MPI_Comm_size(virtual_to_real_comm(comm), world_size);
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

int
__MPI_Exit(int status)
{
  if (g_postRestart) {
   // This never returns
   setcontext(&g_context);
  }
  return 0;
}


static MPI_Comm
virtual_to_real_comm(MPI_Comm comm)
{
  switch ((uintptr_t)comm) {
    case 0:  return MPI_COMM_NULL; break;
    case 1:  return MPI_COMM_WORLD; break;
    case 2:  return MPI_COMM_SELF; break;
    default: return (MPI_Comm)-1;
  }
}

static MPI_Datatype
virtual_to_real_type(MPI_Datatype type)
{
  switch ((uintptr_t)type) {
    case 0:  return MPI_BYTE; break;
    case 1:  return MPI_INT; break;
    case 2:  return MPI_LONG; break;
    case 3:  return MPI_FLOAT; break;
    case 4:  return MPI_DOUBLE; break;
    default: return (MPI_Datatype)-1;
  }
}
