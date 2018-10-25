#ifndef UPPER_HALF_MPI_WRAPPERS_H
#define UPPER_HALF_MPI_WRAPPERS_H

#define EAT(x)
#define REM(x) x
#define STRIP(x) EAT x
#define PAIR(x) REM x

/* This counts the number of args */
#define NARGS_SEQ(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, N, ...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

/* This will let macros expand before concating them */
#define PRIMITIVE_CAT(x, y) x ## y
#define CAT(x, y) PRIMITIVE_CAT(x, y)

/* This will call a macro on each argument passed in */
#define APPLY(macro, ...) CAT(APPLY_, NARGS(__VA_ARGS__))(macro, __VA_ARGS__)
#define APPLY_1(m, x1) m(x1)
#define APPLY_2(m, x1, x2) m(x1), m(x2)
#define APPLY_3(m, x1, x2, x3) m(x1), m(x2), m(x3)
#define APPLY_4(m, x1, x2, x3, x4) m(x1), m(x2), m(x3), m(x4)
#define APPLY_5(m, x1, x2, x3, x4, x5) m(x1), m(x2), m(x3), m(x4), m(x5)
#define APPLY_6(m, x1, x2, x3, x4, x5, x6) m(x1), m(x2), m(x3),                \
                                           m(x4), m(x5), m(x6)
#define APPLY_7(m, x1, x2, x3, x4, x5, x6, x7) m(x1), m(x2), m(x3),            \
                                               m(x4), m(x5), m(x6), m(x7)

#define NEXT_FUNC(func)                                                        \
  ({                                                                           \
    static __typeof__(&MPI_##func)_real_MPI_## func =                          \
                                                (__typeof__(&MPI_##func)) - 1; \
    if (_real_MPI_ ## func == (__typeof__(&MPI_##func)) - 1) {                 \
      LhDlsym_t dlsymFptr = (LhDlsym_t)lhInfo.lhDlsym;                         \
      _real_MPI_ ## func = (__typeof__(&MPI_##func))dlsymFptr(MPI_Fnc_##func); \
    }                                                                          \
    _real_MPI_ ## func;                                                        \
  })

// Convenience macro to define simple wrapper functions
#define DEFINE_FNC(rettype, name, args...)                                     \
  rettype MPI_##name(APPLY(PAIR, args))                                        \
  {                                                                            \
    DLOG(SUPERNOISE, "In wrapper\n");                                          \
    rettype retval;                                                            \
    if (!initialized) {                                                        \
      initialize_wrappers();                                                   \
    }                                                                          \
    JUMP_TO_LOWER_HALF(lhInfo.lhFsAddr);                                       \
    retval = NEXT_FUNC(name)(APPLY(STRIP, args));                              \
    RETURN_TO_UPPER_HALF();                                                    \
    return retval;                                                             \
  }

extern int MPI_Init(int *argc, char ***argv) __attribute__((weak));
#define MPI_Init(f, c) (MPI_Init ? MPI_Init(f, c) : 0)

extern int MPI_Comm_rank(MPI_Comm comm, int *rank) __attribute__((weak));
#define MPI_Comm_rank(f, g) (MPI_Comm_rank ? MPI_Comm_rank(f, g) : 0)

extern int MPI_Send(const void *buf, int count,
                    MPI_Datatype datatype,
                    int dest, int tag,
                    MPI_Comm comm) __attribute__((weak));
#define MPI_Send(a, b, c, d, e, f) (MPI_Send ? MPI_Send(a, b, c, d, e, f) : 0)

extern int MPI_Recv(void *buf, int count, MPI_Datatype datatype,
                    int source, int tag,
                    MPI_Comm comm, MPI_Status *status) __attribute__((weak));
#define MPI_Recv(a, b, c, d, e, f, g) (MPI_Recv ? MPI_Recv(a, b, c, d, e, f, g) : 0)

#endif // ifndef UPPER_HALF_MPI_WRAPPERS_H
