#include "loader.h"

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
    if (ehdr){
        free((void*)ehdr);
        ehdr=NULL;
    }
    if (phdr){
        phdr=NULL;
    }
    if (fd!= -1) {
        close(fd);
        fd =-1;
    }
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char** exe){
  fd = open(*exe, O_RDONLY);
  if (fd<0){
      printf("Error in opening the ELF file\n");
      exit(1);
  }

  // 1. Load entire binary content into the memory from the ELF file.
  off_t elfsize = lseek(fd, 0, SEEK_END);
  if (elfsize<=0){
      printf("File size is invalid.\n");
      exit(1);
  }
  lseek(fd, 0, SEEK_SET);

  char *heap = (char*)malloc(elfsize);
  if (!heap){
      printf("Memory allocation was not successful.\n");
      exit(1);
  }
  if (read(fd, heap, elfsize) != elfsize) {
      printf("Could not read.\n");
      free(heap);
      exit(1);
  }

  ehdr = (Elf32_Ehdr*)(heap);
  phdr = (Elf32_Phdr*)(heap+(*ehdr).e_phoff);

  // 2. Iterate through the PHDR table and find the section of PT_LOAD 
  //    type that contains the address of the entrypoint method in fib.c
  Elf32_Addr entry =(*ehdr).e_entry;
  Elf32_Phdr *chosen= NULL;
  for (int i= 0; i <(*ehdr).e_phnum; i++){
      Elf32_Phdr *p= &phdr[i];

      if ((*p).p_type == PT_LOAD && entry >= (*p).p_vaddr && entry < (*p).p_vaddr + (*p).p_memsz) {
          chosen = p;
          break;
      }
  }
  if (!chosen) {
      printf("Entry not found in PT_LOAD.\n");
      free(heap);
      exit(1);
  }

  // 3. Allocate memory of the size "p_memsz" using mmap function 
  //    and then copy the segment content
  void *seg = mmap(NULL, (*chosen).p_memsz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (seg == MAP_FAILED) {
      printf("Memory mapping failed.\n");
      free(heap);
      exit(1);
  }

  memcpy(seg, heap+(*chosen).p_offset, (*chosen).p_filesz);
  if ((*chosen).p_memsz > (*chosen).p_filesz){
      memset((char*)seg + (*chosen).p_filesz, 0, (*chosen).p_memsz - (*chosen).p_filesz);
  }

  // 4. Navigate to the entrypoint address into the segment loaded in the memory in above step
  void *start_addr = (char*)seg + (entry - (*chosen).p_vaddr);

  // 5. Typecast the address to that of function pointer matching "_start" method in fib.c.
  int (*_start)(void) = (int (*)(void))start_addr;

  // 6. Call the "_start" method and print the value returned from the "_start"
  int result = _start();
  printf("User _start return value = %d\n",result);
}

int main(int argc, char** argv) 
{
  if(argc != 2) {
    printf("Usage: %s <ELF Executable> \n",argv[0]);
    exit(1);
  }
  // 1. carry out necessary checks on the input ELF file
  FILE* ELFfile=fopen(argv[1],"rb");
  if (!ELFfile){
    printf("Error in opening the ELF file.");
    exit(1);}
  fclose (ELFfile);
  // 2. passing it to the loader for carrying out the loading/execution
  load_and_run_elf(&argv[1]);
  // 3. invoke the cleanup routine inside the loader  
  loader_cleanup();
  return 0;
}
