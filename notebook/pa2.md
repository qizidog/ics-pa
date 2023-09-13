## RTFSC-函数调用层级

```txt
cpu_exec
  execute
    exec_once
      isa_exec_once
        inst_fetch
        decode_exec
          INSTPAT -> pattern_decode
          INST_MATCH -> decode_operand
          __VA_ARGS__
    trace_and_difftest
```

`SEXT` 和 `BITS` 宏实在巧妙，建议多加理解。


## riscv32 指令实现

2022和2021版的pa在这部分差异比较大，2022版需要借助大量宏定义来实现译码操作。

很多人反应中文版的RISC-V手册有不少错误，建议用中文版辅助理解，具体细节还是直接看官方手册吧。

提一下RISC-V官方手册，指令的具体操作码等细节，可以在 `Chapter 24 RV32/64G Instruction Set Listings` 章节找到，指令的功效则需要在对应的各个章节去查找。另外，RISC-V有一些伪指令，可以在 `Chapter 25 RISC-V Assembly Programmer's Handbook` 章节查询。

伪指令在尝试执行 `dummy` 程序的时候会涉及到，反编译时看到的指令默认是伪指令。

### object file

在折腾 ELF 之前，先明确一下什么是 目标文件 [object file](https://en.wikipedia.org/wiki/Object_file).

> An object file is *a computer file containing object code, that is, machine code output of an assembler or compiler*. The object code is usually relocatable, and **not usually directly executable**. There are various formats for object files, and the same machine code can be packaged in different object file formats. An object file may also work like a shared library.

注意区别 "目标文件" 和 "可执行文件"。

> In addition to the object code itself, object files may contain metadata used for linking or debugging, including: information to resolve symbolic cross-references between different modules, relocation information, stack unwinding information, comments, program symbols, debugging or profiling information. Other metadata may include the date and time of compilation, the compiler name and version, and other identifying information.

> A computer programmer generates object code with a compiler or assembler. For example, 
> **under Linux**, the GNU Compiler Collection compiler will generate files with a `.o` extension which use the [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) format. 
> Compilation **on Windows** generates files with a `.obj` extension which use the [COFF](https://en.wikipedia.org/wiki/COFF) format. 
> **under macOS**, object files come with a `.o` extension which use the [Mach-O](https://en.wikipedia.org/wiki/Mach-O) format.
> A linker is then used to combine the object code into one executable program or library pulling in precompiled system libraries as needed.

并不是说 ELF 格式的文件都是用 `.o` 作为后缀，只是说 ELF 格式的 object file 一般以 `.o` 作为后缀，其他os平台同理。

### objdump

objdump - display information from object files.

```bash
objdump -d : disassemble sections which are expected to contain instructions.
        -s : display the full contents of any sections requested.
        -j name : display information only for section name.

        -D : like -d, but disassemble the contents of all sections.
        -S : display source code intermixed with disassembly, if possible.
        -l : label the display with the filename and source line numbers.

        -i : display a list showing all architectures and object formats available for specification with -b or -m.
        -b : specify that the object-code format manually (not necessary).
        -m : specify the architecture to use when disassembling object files.

        -f : display summary information of the files.
        -h : display summary information from the section headers.
        -x : equals to -a -f -h -p -r -t.
```

### readelf

readelf - display information about ELF files.

```bash
readelf -a : Equivalent to specifying -h, -l, -S, -s, -r, -d, -n, -V, -A, -u, -g and -I.
        -h, --file-header
        -l, --program-headers
        -S, --sections
        -s, --symbols
        -r, --relocs
        -d, --dynamic
        -n, --notes
        -V, --version-info
        -A, --arch-specific
        -u, --unwind
        -g, --section-groups
        -I, --histogram
```

### objcopy

objcopy - copy and translate object files

> The GNU objcopy utility copies the contents of an object file to another. objcopy uses the GNU BFD Library to read and write the object files. It can write the destination object file in a format different from that of the source object file. The exact behavior of objcopy is controlled by command-line options. Note that objcopy should be able to copy a fully linked file between any two formats. However, copying a relocatable object file between any two formats may not work as expected.

具体就不展开了，RTFM。

### disassemble dummy elf

在执行 `make ARCH=riscv32-nemu ALL=dummy run` 之后，build 目录下生成了 `dummy-riscv32-nemu.[elf|bin|txt]` 3个文件，其中 `.txt` 是 `dummy.c` 反汇编的结果，尝试弄清楚反汇编文件是怎么生成的。

```txt
/home/vbox/ics2022/am-kernels/tests/cpu-tests/build/dummy-riscv32-nemu.elf:     file format elf32-littleriscv


Disassembly of section .text:

80000000 <_start>:
80000000:       00000413                li      s0,0
80000004:       00009117                auipc   sp,0x9
80000008:       ffc10113                addi    sp,sp,-4 # 80009000 <_end>
8000000c:       00c000ef                jal     ra,80000018 <_trm_init>

80000010 <main>:
80000010:       00000513                li      a0,0
80000014:       00008067                ret

80000018 <_trm_init>:
80000018:       80000537                lui     a0,0x80000
8000001c:       ff010113                addi    sp,sp,-16
80000020:       03850513                addi    a0,a0,56 # 80000038 <_end+0xffff7038>
80000024:       00112623                sw      ra,12(sp)
80000028:       fe9ff0ef                jal     ra,80000010 <main>
8000002c:       00050513                mv      a0,a0
80000030:       00100073                ebreak
80000034:       0000006f                j       80000034 <_trm_init+0x1c>
```

> 如果你选择的ISA不是x86, 在查看客户程序的二进制信息(如objdump, readelf等)时, 需要使用相应的交叉编译版本, 如mips-linux-gnu-objdump, riscv64-linux-gnu-readelf等. 特别地, 如果你选择的ISA是riscv32, 也可以使用riscv64为前缀的交叉编译工具链.

```bash
❱❱❱ objdump -d build/dummy-riscv32-nemu.elf

build/dummy-riscv32-nemu.elf:     file format elf32-little

objdump: can't disassemble for architecture UNKNOWN!
```

执行 `riscv64-linux-gnu-objdump -s build/dummy-riscv32-nemu.elf` 命令，但是得到的反汇编结果似乎不太对劲，尤其是 .text 段，好像字节序是反的（尝试 `-BL` 和 `-BE` 均无效）：

```bash
❱❱❱ riscv64-linux-gnu-objdump -s build/dummy-riscv32-nemu.elf

build/dummy-riscv32-nemu.elf:     file format elf32-littleriscv

Contents of section .text:
 80000000 13040000 17910000 1301c1ff ef00c000  ................
 80000010 13050000 67800000 37050080 130101ff  ....g...7.......
 80000020 13058503 23261100 eff09ffe 13050500  ....#&..........
 80000030 73001000 6f000000                    s...o...
Contents of section .srodata.mainargs:
 80000038 00                                   .
Contents of section .comment:
 0000 4743433a 20285562 756e7475 2031312e  GCC: (Ubuntu 11.
 0010 342e302d 31756275 6e747531 7e32322e  4.0-1ubuntu1~22.
 0020 30342920 31312e34 2e3000             04) 11.4.0.
Contents of section .riscv.attributes:
 0000 412d0000 00726973 63760001 23000000  A-...riscv..#...
 0010 05727633 32693270 305f6d32 70305f61  .rv32i2p0_m2p0_a
 0020 3270305f 66327030 5f643270 3000      2p0_f2p0_d2p0.
```

跟随 Makefile 一探究竟，命令搞错了，上面的结果是正常现象，应该用 `riscv64-linux-gnu-objdump -d build/dummy-riscv32-nemu.elf`。

`.elf`, `.bin` 和 `.txt` 文件的出处：

```makefile
# ~/ics2022/abstract-machine/Makefile
$(IMAGE).elf: $(OBJS) am $(LIBS)
	@echo + LD "->" $(IMAGE_REL).elf
	@$(LD) $(LDFLAGS) -o $(IMAGE).elf --start-group $(LINKAGE) --end-group

# ~/ics2022/abstract-machine/scripts/platform/nemu.mk
image: $(IMAGE).elf
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin
```

反汇编结果出现大量伪指令，干扰了riscv32指令的实现过程，可以使用 `-M no-aliases` 参数来禁止反汇编生成伪指令。

```bash
❱❱❱ riscv64-linux-gnu-objdump -d -M no-aliases build/dummy-riscv32-nemu.elf

build/dummy-riscv32-nemu.elf:     file format elf32-littleriscv


Disassembly of section .text:

80000000 <_start>:
80000000:       00000413                addi    s0,zero,0
80000004:       00009117                auipc   sp,0x9
80000008:       ffc10113                addi    sp,sp,-4 # 80009000 <_end>
8000000c:       00c000ef                jal     ra,80000018 <_trm_init>

80000010 <main>:
80000010:       00000513                addi    a0,zero,0
80000014:       00008067                jalr    zero,0(ra)

80000018 <_trm_init>:
80000018:       80000537                lui     a0,0x80000
8000001c:       ff010113                addi    sp,sp,-16
80000020:       03850513                addi    a0,a0,56 # 80000038 <_end+0xffff7038>
80000024:       00112623                sw      ra,12(sp)
80000028:       fe9ff0ef                jal     ra,80000010 <main>
8000002c:       00050513                addi    a0,a0,0
80000030:       00100073                ebreak
80000034:       0000006f                jal     zero,80000034 <_trm_init+0x1c>
```


