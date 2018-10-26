#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "custom-loader.h"
#include "kernel-loader.h"
#include "trampoline_setup.h"

// Uses ELF Format.  For background, read both of:
//  * http://www.skyfree.org/linux/references/ELF_Format.pdf
//  * /usr/include/elf.h
// The kernel code is here (not recommended for a first reading):
//    https://github.com/torvalds/linux/blob/master/fs/binfmt_elf.c

// FIXME: can we find this path at run time?
const char DEBUG_FILES_PATH[] = "/usr/lib/debug/lib/x86_64-linux-gnu/";

static void get_elf_interpreter(int , Elf64_Addr *, char* , void* );
static void* load_elf_interpreter(int , char* , Elf64_Addr *,
                                  void * , DynObjInfo_t* );
static void* map_elf_interpreter_load_segment(int , Elf64_Phdr , void* );

static void get_elf_debug_symbol_file(int fd, char* debug_symbol_file);

// Global functions
DynObjInfo_t
safeLoadLib(const char *name)
{
  void *ld_so_addr = NULL;
  DynObjInfo_t info = {0};
  off_t mmapOffset = 0;
  off_t sbrkOffset = 0;

  int ld_so_fd;
  Elf64_Addr cmd_entry, ld_so_entry;
  char elf_interpreter[MAX_ELF_INTERP_SZ];

  // FIXME: Do we need to make it dynamic? Is setting this required?
  // ld_so_addr = (void*)0x7ffff81d5000;
  // ld_so_addr = (void*)0x7ffff7dd7000;
  int cmd_fd = open(name, O_RDONLY);
  get_elf_interpreter(cmd_fd, &cmd_entry, elf_interpreter, ld_so_addr);
  // FIXME: The ELF Format manual says that we could pass the cmd_fd to ld.so,
  //   and it would use that to load it.
  close(cmd_fd);
  strncpy(elf_interpreter, name, sizeof elf_interpreter);

  ld_so_fd = open(elf_interpreter, O_RDONLY);
  info.baseAddr = load_elf_interpreter(ld_so_fd, elf_interpreter,
                                       &ld_so_entry, ld_so_addr, &info);

#ifdef UBUNTU
  char ldDebug[MAX_ELF_DEBUG_SZ];
  get_elf_debug_symbol_file(ld_so_fd, ldDebug);

  // const char *ldDebug = "/usr/lib/debug/lib/x86_64-linux-gnu/ld-2.27.so";
  int newFd = open(ldDebug, O_RDONLY);
#else
  const char *ldDebug = name;
  int newFd = ld_so_fd;
#endif // ifdef UBUNTU

  mmapOffset = get_symbol_offset(newFd, ldDebug, "mmap");
  sbrkOffset = get_symbol_offset(newFd, ldDebug, "sbrk");

  info.mmapAddr = (VA)info.baseAddr + mmapOffset;
  info.sbrkAddr = (VA)info.baseAddr + sbrkOffset;
  // FIXME: The ELF Format manual says that we could pass the ld_so_fd to ld.so,
  //   and it would use that to load it.
  close(ld_so_fd);

#ifdef UBUNTU
  close(newFd);
#endif // ifdef UBUNTU

  info.entryPoint = (void*)((unsigned long)info.baseAddr +
                            (unsigned long)cmd_entry);
  return info;
}


// Local functions
static void
get_elf_interpreter(int fd, Elf64_Addr *cmd_entry,
                    char* elf_interpreter, void *ld_so_addr)
{
  int rc;
  char e_ident[EI_NIDENT];

  rc = read(fd, e_ident, sizeof(e_ident));
  assert(rc == sizeof(e_ident));
  assert(strncmp(e_ident, ELFMAG, strlen(ELFMAG)) == 0);
  assert(e_ident[EI_CLASS] == ELFCLASS64); // FIXME:  Add support for 32-bit ELF

  // Reset fd to beginning and parse file header
  lseek(fd, 0, SEEK_SET);
  Elf64_Ehdr elf_hdr;
  rc = read(fd, &elf_hdr, sizeof(elf_hdr));
  assert(rc == sizeof(elf_hdr));
  *cmd_entry = elf_hdr.e_entry;

  // Find ELF interpreter
  int i;
  Elf64_Phdr phdr;
  int phoff = elf_hdr.e_phoff;

  lseek(fd, phoff, SEEK_SET);
  for (i = 0; i < elf_hdr.e_phnum; i++) {
    assert(i < elf_hdr.e_phnum);
    rc = read(fd, &phdr, sizeof(phdr)); // Read consecutive program headers
    assert(rc == sizeof(phdr));
  }
}

static void*
load_elf_interpreter(int fd, char *elf_interpreter,
                     Elf64_Addr *ld_so_entry, void *ld_so_addr,
                     DynObjInfo_t *info)
{
  char e_ident[EI_NIDENT];
  int rc;
  int firstTime = 1;
  void *baseAddr = NULL;

  rc = read(fd, e_ident, sizeof(e_ident));
  assert(rc == sizeof(e_ident));
  assert(strncmp(e_ident, ELFMAG, sizeof(ELFMAG)-1) == 0);
  // FIXME:  Add support for 32-bit ELF later
  assert(e_ident[EI_CLASS] == ELFCLASS64);

  // Reset fd to beginning and parse file header
  lseek(fd, 0, SEEK_SET);
  Elf64_Ehdr elf_hdr;
  rc = read(fd, &elf_hdr, sizeof(elf_hdr));
  assert(rc == sizeof(elf_hdr));

  // Find ELF interpreter
  int phoff = elf_hdr.e_phoff;
  Elf64_Phdr phdr;
  int i;
  lseek(fd, phoff, SEEK_SET);
  for (i = 0; i < elf_hdr.e_phnum; i++ ) {
    rc = read(fd, &phdr, sizeof(phdr)); // Read consecutive program headers
    assert(rc == sizeof(phdr));
    if (phdr.p_type == PT_LOAD) {
      // PT_LOAD is the only type of loadable segment for ld.so
      if (firstTime) {
        baseAddr = map_elf_interpreter_load_segment(fd, phdr, ld_so_addr);
        firstTime = 0;
      } else {
        map_elf_interpreter_load_segment(fd, phdr, ld_so_addr);
      }
    }
  }
  info->phnum = elf_hdr.e_phnum;
  info->phdr = (VA)baseAddr + elf_hdr.e_phoff;
  return baseAddr;
}

static void*
map_elf_interpreter_load_segment(int fd, Elf64_Phdr phdr, void *ld_so_addr)
{
  static char *base_address = NULL; // is NULL on call to first LOAD segment
  static int first_time = 1;
  int prot = PROT_NONE;
  if (phdr.p_flags & PF_R)
    prot |= PROT_READ;
  if (phdr.p_flags & PF_W)
    prot |= PROT_WRITE;
  if (phdr.p_flags & PF_X)
    prot |= PROT_EXEC;
  assert(phdr.p_memsz >= phdr.p_filesz);
  // NOTE:  man mmap says:
  // For a file that is not a  multiple  of  the  page  size,  the
  // remaining memory is zeroed when mapped, and writes to that region
  // are not written out to the file.
  void *rc2;
  // Check ELF Format constraint:
  if (phdr.p_align > 1) {
    assert(phdr.p_vaddr % phdr.p_align == phdr.p_offset % phdr.p_align);
  }
  int vaddr = phdr.p_vaddr;

  int flags = MAP_PRIVATE;
  unsigned long addr = ROUND_DOWN(base_address + vaddr);
  size_t size = ROUND_UP(phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr));
  off_t offset = phdr.p_offset - PAGE_OFFSET(phdr.p_vaddr);

  // phdr.p_vaddr = ROUND_DOWN(phdr.p_vaddr);
  // phdr.p_offset = ROUND_DOWN(phdr.p_offset);
  // phdr.p_memsz = phdr.p_memsz + (vaddr - phdr.p_vaddr);
  // NOTE:  base_address is 0 for first load segment
  if (first_time) {
    phdr.p_vaddr += (unsigned long long)ld_so_addr;
  } else {
    flags |= MAP_FIXED;
  }
  if (ld_so_addr) {
    flags |= MAP_FIXED;
  }
  // FIXME:  On first load segment, we should map 0x400000 (2*phdr.p_align),
  //         and then unmap the unused portions later after all the
  //         LOAD segments are mapped.  This is what ld.so would do.
  rc2 = mmapWrapper((void *)addr, size, prot, MAP_PRIVATE, fd, offset);
  if (rc2 == MAP_FAILED) {
    DLOG(ERROR, "Failed to map memory region at %p. Error:%s\n",
         (void*)addr, strerror(errno));
    return NULL;
  }
  unsigned long startBss = (uintptr_t)base_address +
                          phdr.p_vaddr + phdr.p_filesz;
  unsigned long endBss = (uintptr_t)base_address + phdr.p_vaddr + phdr.p_memsz;
  // Required by ELF Format:
  if (phdr.p_memsz > phdr.p_filesz) {
    // This condition is true for the RW (data) segment of ld.so
    // We need to clear out the rest of memory contents, similarly to
    // what the kernel would do. See here:
    //   https://elixir.bootlin.com/linux/v4.18.11/source/fs/binfmt_elf.c#L905
    // Note that p_memsz indicates end of data (&_end)

    // First, get to the page boundary
    uintptr_t endByte = ROUND_UP(startBss);
    // Next, figure out the number of bytes we need to clear out.
    // From Bss to the end of page.
    size_t bytes = endByte - startBss;
    memset((void*)startBss, 0, bytes);
  }
  // If there's more bss that overflows to another page, map it in and
  // zero it out
  startBss  = ROUND_UP(startBss);
  endBss    = ROUND_UP(endBss);
  if (endBss > startBss) {
    void *base = (void*)startBss;
    size_t len = endBss - startBss;
    flags |= MAP_ANONYMOUS; // This should give us 0-ed out pages
    rc2 = mmapWrapper(base, len, prot, flags, -1, 0);
    if (rc2 == MAP_FAILED) {
      DLOG(ERROR, "Failed to map memory region at %p. Error:%s\n",
           (void*)startBss, strerror(errno));
      return NULL;
    }
  }
  if (first_time) {
    first_time = 0;
    base_address = rc2;
  }
  return base_address;
}

static void
get_elf_debug_symbol_file(int fd, char* debug_symbol_file) {
  off_t filesz = lseek(fd, 0, SEEK_END);
  void* addr;
  addr = mmap(NULL, filesz, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert(addr != NULL);

  Elf64_Ehdr* elf_header = (Elf64_Ehdr *)addr;
  Elf64_Shdr* shdr = (Elf64_Shdr *) ((char*)addr + elf_header->e_shoff);
  char* strtab;
  strtab = (char*)((char*)addr + shdr[elf_header->e_shstrndx].sh_offset);

  int i;
  for(i=0; i<elf_header->e_shnum; ++i) {
    if (!strcmp(".gnu_debuglink", &strtab[shdr[i].sh_name])) {
      strncpy(debug_symbol_file, DEBUG_FILES_PATH, sizeof(DEBUG_FILES_PATH)-1);
      strcpy(debug_symbol_file + (sizeof(DEBUG_FILES_PATH)-1), (char*)addr + shdr[i].sh_offset);
      // strncpy(debug_symbol_file, &strtab[shdr[i].sh_name], MAX_ELF_DEBUG_SZ);
      break;
    }
  }

  // free the region the file occupied.
  assert(munmap(addr, filesz) != -1);

  fprintf(stderr, "debug_symbol_file (ld.so): %s\n", debug_symbol_file);
}
