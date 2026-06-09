#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>

#define PAGE_SIZE 4096
#define MAX_PAGES 256

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

int page_faults = 0;
int num_pages = 0;             // no. of pages allocated
size_t FRAGMENTATION = 0;      // total internal fragmentation (in bytes)

// Heap buffer for ELF file
char *heap = NULL;

// mapped pages struct
typedef struct {
    void *page_addr;
    size_t size;        //size of mapped page (PAGE_SIZE)
} PageInfo;

PageInfo pages[MAX_PAGES];  //array of all mapped pages

void loader_cleanup() {
    if (heap) {
        free(heap);
        heap = NULL;
    }

    //upmap all the pages
    for (int i = 0; i < num_pages; i++) {
        if (pages[i].page_addr) {
            munmap(pages[i].page_addr, pages[i].size);
            pages[i].page_addr = NULL;
        }
    }

    if (fd != -1) {
        if (close(fd) == -1) {
            perror("Error closing ELF file");
        }
        fd = -1;
    }
}

// Set permissions based on segment flags
int get_prot_flags(Elf32_Word flags) {
    int prot = 0;
    if (flags & PF_R) prot |= PROT_READ;    // Segment readable
    if (flags & PF_W) prot |= PROT_WRITE;   // Segment writable
    if (flags & PF_X) prot |= PROT_EXEC;    // Segment executable
    return prot;
}


void handle_page_fault(void *fault_addr) {
    size_t page_start = ((size_t)fault_addr / PAGE_SIZE) * PAGE_SIZE;

    // Find which segment this fault address belongs to
    int seg_index = -1;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD &&
            (uintptr_t)phdr[i].p_vaddr <= (uintptr_t)fault_addr &&
            (uintptr_t)fault_addr < (uintptr_t)(phdr[i].p_vaddr + phdr[i].p_memsz)) {
            seg_index = i;
            break;
        }
    }

    if (seg_index == -1) {
        perror("Segmentation fault: Outside loadable segments");
        exit(1);
    }

    Elf32_Phdr *seg = &phdr[seg_index];
    int prot = get_prot_flags(seg->p_flags);

    off_t offset = seg->p_offset + (page_start - seg->p_vaddr);
    void *mapped;

    if ((size_t)(page_start - seg->p_vaddr) >= seg->p_filesz) {
        // BSS 
        mapped = mmap((void*)page_start, PAGE_SIZE, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    } else {
        mapped = mmap((void*)page_start, PAGE_SIZE, prot, MAP_PRIVATE | MAP_FIXED, fd, offset);
    }

    if (mapped == MAP_FAILED) {
        perror("mmap failed in handle_page_fault");
        exit(1);
    }

    pages[num_pages].page_addr = mapped;
    pages[num_pages].size = PAGE_SIZE;
    num_pages++;

    FRAGMENTATION += page_start + PAGE_SIZE - seg->p_vaddr - seg->p_memsz;
}

void sigsegv_handler(int signo, siginfo_t *info, void *context) {
    if (signo != SIGSEGV)
        return;

    page_faults++;
    handle_page_fault(info->si_addr);
}


void setup_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction setup failed");
        exit(1);
    }
}


void print_info(int result) {
    printf("User _start return value = %d\n", result);
    printf("Total page faults: %d\n", page_faults);
    printf("Total pages allocated: %d\n", num_pages);
    printf("Total fragmentation: %.4f KB\n", (double)FRAGMENTATION / 1024.0);
}

void load_and_run_elf(char **exe) {
    fd = open(*exe, O_RDONLY);
    if (fd < 0) {
        perror("Error opening ELF file");
        exit(1);
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        perror("Invalid file size");
        exit(1);
    }
    lseek(fd, 0, SEEK_SET);

    heap = (char *)malloc(size);
    if (!heap) {
        perror("Memory allocation failed");
        exit(1);
    }

    if (read(fd, heap, size) != size) {
        perror("File read failed");
        free(heap);
        exit(1);
    }

    ehdr = (Elf32_Ehdr *)heap;
    phdr = (Elf32_Phdr *)(heap + ehdr->e_phoff);

    // Check ELF validity
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        perror("Invalid ELF file");
        loader_cleanup();
        exit(1);
    }

    // Entry point
    int (*_start)(void) = (int (*)(void))(uintptr_t)(ehdr->e_entry);
    int result = _start();

    print_info(result);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }

    setup_signal_handler();
    load_and_run_elf(&argv[1]);
    loader_cleanup();

    return 0;
}
