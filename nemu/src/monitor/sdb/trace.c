#include <monitor.h>
#include <elf.h>

#define NR_RB 10
#define SZ_BUF 128

static char iringbuf[NR_RB][SZ_BUF];
static int head = 0;

void invoke_itrace(char * buf) {
  strncpy(iringbuf[head], buf, SZ_BUF);
  head = (head + 1) % NR_RB;
}

void itrace_display() {
  printf("Instruction ring buffer (record size %d) display as follows:\n", NR_RB);
  for (int i = 0; i < NR_RB; i++) {
    if (iringbuf[i][0] != '\0') {
      if (i == (head + NR_RB - 1) % NR_RB) printf("-->");
      printf("\t%s\n", iringbuf[i]);
    }
  }
}

#ifdef CONFIG_FTRACE

typedef struct ftrace_entry {  // TODO: implemented by linkedlist
  char name[64];  // function name
  word_t start;   // start address
  uint32_t size;  // size
  /* struct ftrace_entry *next; */
} FT_Entry;       // [start, start+size)

/* static FT_Entry *ft_head = NULL, *ft_tail = NULL; */

#define FT_SIZE 1024
static FT_Entry ftrace_pool[FT_SIZE];
static uint32_t ftrace_size = 0;

typedef MUXDEF(CONFIG_ISA64, Elf64_Ehdr, Elf32_Ehdr) Elf_Ehdr;
typedef MUXDEF(CONFIG_ISA64, Elf64_Shdr, Elf32_Shdr) Elf_Shdr;
typedef MUXDEF(CONFIG_ISA64, Elf64_Sym, Elf32_Sym) Elf_Sym;

static void read_elf_head(Elf_Ehdr *elf_hdr, FILE *fp) {
  rewind(fp);
  size_t rt = fread(elf_hdr, sizeof(Elf_Ehdr), 1, fp);
  Assert(rt == 1, "ELF_SIZE_ERROR: read ELF file failed.");

  const unsigned char *e_ident = elf_hdr->e_ident;
  Assert(e_ident[0] == 0x7f && e_ident[1] == 0x45
         && e_ident[2] == 0x4c && e_ident[3] == 0x46,
         "ELF_ERROR: not a ELF file.");
  Assert(elf_hdr->e_machine == EM_RISCV, "ELF_ERROR: not a risc-v ELF file.");
}

static void read_elf_shdr(Elf_Shdr *elf_shdr_tb, Elf_Ehdr *elf_hdr, FILE *fp) {
  Assert(!fseek(fp, elf_hdr->e_shoff, SEEK_SET), "seek fault.");
  Assert(elf_hdr->e_shentsize == sizeof(Elf_Shdr), "section header entry size error");

  size_t rt = fread(elf_shdr_tb, sizeof(Elf_Shdr), elf_hdr->e_shnum, fp);

  Assert(rt == elf_hdr->e_shnum, "ELF_SIZE_ERROR: read ELF file failed.");
}

static void read_elf_sym(Elf_Shdr *elf_shdr, uint16_t shdr_nr, FILE *fp) {
  for (size_t i = 0; i < shdr_nr; i++) {
    Elf_Shdr shdr = elf_shdr[i];
    if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
      word_t offset = shdr.sh_offset;
      uint32_t size = shdr.sh_size, entsize = shdr.sh_entsize;
      Assert(entsize == sizeof(Elf_Sym), "entry size equal to 0.");
      Assert(size % entsize == 0, "section size %#x, entry size %#x.", size, entsize);

      uint32_t ent_nr = size / entsize;
      Elf_Sym elf_sym_tb[ent_nr];

      fseek(fp, offset, SEEK_SET);
      size_t rt = fread(elf_sym_tb, sizeof(Elf_Sym), ent_nr, fp);
      Assert(rt == ent_nr, "ELF_SIZE_ERROR: read ELF file failed.");

      uint32_t link = shdr.sh_link;
      uint32_t str_base = elf_shdr[link].sh_offset;  // string table base offset

      for (size_t k = 0; k < ent_nr; k++) {
        Elf_Sym sym = elf_sym_tb[k];

        if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) continue;

        FT_Entry* ft_ent = &ftrace_pool[ftrace_size++];
        Assert(ftrace_size < FT_SIZE, "ftrace_size larger than max value [%d] allowed.", FT_SIZE);

        if (sym.st_name == 0) {  // if the calue is zero, the symbol has no name
          strncpy(ft_ent->name, "NO_NAME", sizeof(ft_ent->name));
        } else {
          char buf[16];
          sprintf(buf, "%%%lus", sizeof(ft_ent->name) - 1);
          fseek(fp, str_base + sym.st_name, SEEK_SET);
          Assert(EOF != fscanf(fp, buf, ft_ent->name), "EOF occur");
        }
        ft_ent->size = sym.st_size;
        ft_ent->start = sym.st_value;
      }

      // print symbol table
      /* char sym_name[64]; */
      /* printf("Num:\t%-25s\t%12s\tsize\tinfo\tother\tshndx\n", "name", "value"); */
      /* for (size_t k = 0; k < ent_nr; k++) { */
      /*   Elf_Sym sym = elf_sym_tb[k]; */
      /*   if (sym.st_name == 0) {  // if the calue is zero, the symbol has no name */
      /*     sprintf(sym_name, "%-25s", "NO_NAME"); */
      /*   } else { */
      /*     fseek(fp, str_base + sym.st_name, SEEK_SET); */
      /*     Assert(EOF != fscanf(fp, "%63s", sym_name), "EOF occur"); */
      /*   } */
      /*   printf("%d\t%-25s\t%#12x\t%u\t%u\t%u\t%u\n", */
      /*          (int)k, sym_name, sym.st_value, sym.st_size, sym.st_info, sym.st_other, sym.st_shndx); */
      /* } */
    }
  }
}

void init_ftrace(char **elf_trace_file, uint32_t nr_elf) {
  if (elf_trace_file == NULL) {
    Warn("ftrace is enabled while no ELF file is provided!");
    return;
  }

  for (uint32_t i = 0; i < nr_elf; i++) {
    FILE *elfp = fopen(elf_trace_file[i], "rb");
    if (elfp == NULL) {
      Warn("fail to open ftrace elf file %s.", elf_trace_file[i]);
      return;
    }

    Log("start initializing ftrace with elf file: %s.", elf_trace_file[i]);

    // parse elf header
    Elf_Ehdr elf_hdr;
    read_elf_head(&elf_hdr, elfp);

    // parse elf section header table
    uint16_t shdr_nr = elf_hdr.e_shnum;
    Elf_Shdr elf_shdr_tb[shdr_nr];
    read_elf_shdr(elf_shdr_tb, &elf_hdr, elfp);

    // print section header
    /* int start = elf_shdr_tb[elf_hdr.e_shstrndx].sh_offset; */
    /* char name[64]; */
    /* printf("%-20s\ttype\toffset\tsize\tentry_size\tlink\n", "name"); */
    /* for (int i = 0; i < shdr_nr; i++) { */
    /*   Elf_Shdr e = elf_shdr_tb[i]; */
    /*   fseek(elfp, start+e.sh_name, SEEK_SET); */
    /*   Assert(EOF != fscanf(elfp, "%63s", name), "EOF occur"); */
    /*   printf("%-20s\t%u\t%#x\t%#x\t%#x\t%#x\n", name, e.sh_type, e.sh_offset, e.sh_size, e.sh_entsize, e.sh_link); */
    /* } */

    // parse elf symbol table
    read_elf_sym(elf_shdr_tb, shdr_nr, elfp);

    // print function symbol
    /* printf("%-20s\tstart\tsize\n", "func_name"); */
    /* for (int i = 0; i < ftrace_size; i++) { */
    /*   printf("%-20s\t%#x\t%#x\n", ftrace_pool[i].name, ftrace_pool[i].start, ftrace_pool[i].size); */
    /* } */

    fclose(elfp);
  }
}

static const char *ft_name(vaddr_t pc) {
  FT_Entry * ft_ent;
  for (size_t i = 0; i < ftrace_size; i++) {
    ft_ent = &ftrace_pool[i];
    if (pc >= ft_ent->start && pc < ft_ent->start + ft_ent->size) {
      return ft_ent->name;
    }
  }
  return NULL;
}

/*
 * |   rd  |  rs1  | rs1=rd | RAS action     |
 * |-------|-------|--------|----------------|
 * | !link | !link |    -   | none           |
 * | !link |  link |    -   | pop            |
 * |  link | !link |    -   | push           |
 * |  link |  link |    0   | pop, then push |
 * |  link |  link |    1   | push           |
*/
// In the above, link is true when the register is either x1 or x5.
#define LINK(reg) ((reg) == 1 || (reg) == 5)

void invoke_ftrace(Decode* s) {
  if (ftrace_size <= 0) return;

  word_t rd = BITS(s->isa.inst.val, 11, 7);
  int rs1 = BITS(s->isa.inst.val, 19, 15);
  vaddr_t pc = s->pc, dnpc = s->dnpc;

  if (!LINK(rd) && !LINK(rs1)) return;

  const char *from, *to;
  if (!LINK(rd) && LINK(rs1)) {  // pop
  /* if (rd == 0 && rs1 == 1) { */
    from = ft_name(pc);
    to = ft_name(dnpc);
    if (from == NULL || to == NULL) return;
    Log("[FTRACE-RET]   %s[" FMT_PADDR "] -> %s[" FMT_PADDR "]", from, pc, to, dnpc);
  } else if (LINK(rd) && (!LINK(rs1) || rd == rs1)) {  // push
  /* } else if (rd == 1) { */
    from = ft_name(pc);
    to = ft_name(dnpc);
    if (from == NULL || to == NULL) return;
    Log("[FTRACE-CALL]  %s[" FMT_PADDR "] -> %s[" FMT_PADDR "]", from, pc, to, dnpc);
  } else {  // pop, then push
    panic("Coroutines not implemented");
  }
}

#undef LINK

#else
void init_ftrace(char **elf_trace_file, uint32_t nr_elf) { }
#endif
