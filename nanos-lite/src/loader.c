#include <proc.h>
#include <elf.h>
#include <fs.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#if defined(__ISA_AM_NATIVE__) || defined(__ISA_X86__)
# define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_MIPS32__)
# define EXPECT_TYPE EM_MIPS
#elif defined(__ISA_RISCV32__) || defined(__ISA_RISCV64__)
# define EXPECT_TYPE EM_RISCV
#elif defined(__ISA_LOONGARCH32R__)
# define EXPECT_TYPE EM_LOONGARCH
#else
# define EXPECT_TYPE EM_NONE // Unsupported ISA
#endif

static uintptr_t loader(PCB *pcb, const char *filename) {
  // 读取存储在.data节的ramdisk
  Elf_Ehdr elf_hdr;
  // Log("ELF header address: %x, size: %d\n", (uintptr_t)&elf_hdr, sizeof(Elf_Ehdr));
  int fd = fs_open(filename, 0, 0);
  if (fd == -1) panic("file %s not found", filename);

  fs_read(fd, (void*)&elf_hdr, sizeof(Elf_Ehdr));

  // 检测magic number
  const unsigned char *e_ident = elf_hdr.e_ident;
  assert(*(uint32_t *)e_ident == *(uint32_t *)ELFMAG);

  // 检测ELF ISA类型
  assert(elf_hdr.e_machine == EXPECT_TYPE);

  // 构建程序头表
  uint16_t phnum = elf_hdr.e_phnum, phoff = elf_hdr.e_phoff;
  Elf_Phdr* phdr_tb = (Elf_Phdr*)malloc(phnum*sizeof(Elf_Phdr));
  assert(phdr_tb);
  fs_lseek(fd, phoff, SEEK_SET);
  fs_read(fd, (void*)phdr_tb, phnum*sizeof(Elf_Phdr));

  // 加载
  Elf_Phdr phdr;
  for (uint16_t i=0; i<phnum; i++) {
    phdr = phdr_tb[i];
    // Log("deal with the %d/%d of program header, p_type: %d", i+1, phnum, phdr.p_type);
    if (phdr.p_type != PT_LOAD) continue;
    char* buf = (char*)malloc(phdr.p_memsz);
    assert(buf);
    // 提取ramdisk中待加载的节
    fs_lseek(fd, phdr.p_offset, SEEK_SET);
    fs_read(fd, (void*)buf, phdr.p_filesz);
    // 超出部分地址清零
    if (phdr.p_memsz > phdr.p_filesz) {
      memset((void*)buf+phdr.p_filesz, 0, phdr.p_memsz-phdr.p_filesz);
    }
    // 将待加载的节写入nemu内存对应地址
    // Log("load ramdisk image with size of %x into vaddr: %x\n", phdr.p_memsz, (uint32_t)phdr.p_vaddr);
    memcpy((void*)phdr.p_vaddr, buf, phdr.p_memsz);
    free(buf);
  }
  free(phdr_tb);
  Log("ramdisk program entry: %#x\n", elf_hdr.e_entry);
  return elf_hdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, "/bin/timer-test");
  Log("Jump to entry = %x", entry);
  ((void(*)())entry)();
}
