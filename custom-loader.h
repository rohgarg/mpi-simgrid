#ifndef CUSTOM_LOADER_H
#define CUSTOM_LOADER_H

#define MAX_ELF_INTERP_SZ 256
#define MAX_ELF_DEBUG_SZ 256

typedef struct __DynObjInfo
{
  void *baseAddr;
  void *entryPoint;
  uint64_t phnum;
  void *phdr;
  void *mmapAddr;
  void *sbrkAddr;
} DynObjInfo_t;

DynObjInfo_t safeLoadLib(const char *);
void get_debug_symbol_file(int , char* , size_t );

#endif // ifndef CUSTOM_LOADER_H
