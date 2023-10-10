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
        -x, --hex-dump <number or name>
```

`-x` 用来查看特定 section 的具体内容，相当于 `objdump -s -j xxxx`。

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

### shift operators

在实现 `srl`, `sra` 指令的时候意识到一个问题，在c语言中做 `a >> b` 右移操作时，如果a是有符号数，b是无符号数，是默认进行整数提升将a变为无符号数（0拓展），还是会进行符号位拓展呢？

查了一下C语言的手册，

> First, integer promotions are performed, individually, on each operand (Note: this is unlike other binary arithmetic operators, which all perform usual arithmetic conversions). **The type of the result is the type of lhs after promotion**.

> **For unsigned lhs and for signed lhs with nonnegative values**, the value of LHS >> RHS is the integer part of LHS / 2^RHS
. **For negative LHS**, the value of LHS >> RHS is *implementation-defined* where in most implementations, this performs arithmetic right shift (so that the result remains negative). Thus in most implementations, right shifting a signed LHS fills the new higher-order bits with the original sign bit (i.e. with 0 if it was non-negative and 1 if it was negative).

因此，右移操作的结果类型仅取决于 lhs 操作数。


## abstract machine

什么是 AM (abstract machine) ?

一个可以支撑各种程序运行在各种架构上的库，即一组API（不同的架构具有不同的实现方式，但保持作用统一），这组统一抽象的API代表了程序运行对计算机的需求, 我们把这组API称为抽象计算机.

作为一个向程序提供运行时环境的库, AM根据程序的需求把库划分成以下模块

**AM = TRM + IOE + CTE + VME + MPE**

- TRM (Turing Machine) - 图灵机, 最简单的运行时环境, 为程序提供基本的计算能力
- IOE (I/O Extension) - 输入输出扩展, 为程序提供输出输入的能力
- CTE (Context Extension) - 上下文扩展, 为程序提供上下文管理的能力
- VME (Virtual Memory Extension) - 虚存扩展, 为程序提供虚存管理的能力
- MPE (Multi-Processor Extension) - 多处理器扩展, 为程序提供多处理器通信的能力 (PA中不涉及)

**PA构建计算机系统的全过程:**

![计算机是个抽象层](https://nju-projectn.github.io/ics-pa-gitbook/ics2022/images/pa-concept.png)

### AM 项目结构

如果说现阶段的 nemu 是模拟了一个 cpu，那么 abstract-machine 就是模拟了一个可以运行的系统级框架（还不能称作为操作系统）以及库函数。总体来看，abstract-machine包含了 am 和 klib 两个部分。

- am - 不同架构的AM API实现, 目前只需要关注NEMU相关内容即可（abstract-machine/am/include/am.h列出了AM中的所有API）
- klib - 一些架构无关的库函数, 方便应用程序的开发

```bash
abstract-machine
├── am                                  # AM相关
│   ├── include
│   │   ├── amdev.h
│   │   ├── am.h
│   │   └── arch                        # 架构相关的头文件定义
│   ├── Makefile
│   └── src
│       ├── mips
│       │   ├── mips32.h
│       │   └── nemu                    # mips32-nemu相关的实现
│       ├── native                      # 以宿主机为平台的AM实现
│       │   ├── cte.c
│       │   ├── ioe
│       │   │   ├── audio.c
│       │   │   ├── disk.c
│       │   │   ├── gpu.c
│       │   │   ├── input.c
│       │   │   └── timer.c
│       │   ├── ioe.c
│       │   ├── mpe.c
│       │   ├── platform.c
│       │   ├── platform.h
│       │   ├── trap.S
│       │   ├── trm.c
│       │   └── vme.c
│       ├── platform
│       │   └── nemu                    # 以NEMU为平台的AM实现
│       │       ├── include
│       │       │   └── nemu.h
│       │       ├── ioe                 # IOE
│       │       │   ├── audio.c
│       │       │   ├── disk.c
│       │       │   ├── gpu.c
│       │       │   ├── input.c
│       │       │   ├── ioe.c
│       │       │   └── timer.c
│       │       ├── mpe.c               # MPE, 当前为空
│       │       └── trm.c               # TRM
│       ├── riscv
│       │   ├── nemu                    # riscv32(64)相关的实现
│       │   │   ├── cte.c               # CTE
│       │   │   ├── start.S             # 程序入口
│       │   │   ├── trap.S
│       │   │   └── vme.c               # VME
│       │   └── riscv.h
│       └── x86
│           ├── nemu                    # x86-nemu相关的实现
│           └── x86.h
├── klib                                # 常用函数库
├── Makefile                            # 公用的Makefile规则
└── scripts                             # 构建/运行二进制文件/镜像的Makefile
    ├── isa
    │   ├── mips32.mk
    │   ├── riscv32.mk
    │   ├── riscv64.mk
    │   └── x86.mk
    ├── linker.ld                       # 链接脚本
    ├── mips32-nemu.mk
    ├── native.mk
    ├── platform
    │   └── nemu.mk
    ├── riscv32-nemu.mk
    ├── riscv64-nemu.mk
    └── x86-nemu.mk
```

am 屏蔽了不同 isa 的底层实现细节，用来提供一组统一的、供程序使用的接口（例如把nemu_trap封装成halt）；klib 利用 am 提供的统一接口，拓展出一套统一的公用库函数（比如printf等标准库函数），方便应用程序的开发和调用。

abstract-machine 和 am-kernels 的关系

| TRM      | 计算       | 内存申请         | 结束运行      | 打印信息            |
|----------|------------|------------------|---------------|---------------------|
| 运行环境 | -          | malloc()/free()  | -             | printf()            |
| AM API   | -          | heap             | halt()        | putch()             |
| ISA接口  | 指令       | 物理内存地址空间 | nemu_trap指令 | I/O方式             |
| 硬件模块 | 处理器     | 物理内存         | Monitor       | 串口                |
| 电路实现 | cpu_exec() | pmem[]           | nemu_state    | serial_io_handler() |

am-kernels子项目用于收录一些可以在AM上运行的测试集和简单程序。

```bash
am-kernels
├── benchmarks                  # 可用于衡量性能的基准测试程序
│   ├── coremark
│   ├── dhrystone
│   └── microbench
├── kernels                     # 可展示的应用程序
│   ├── hello
│   ├── litenes                 # 简单的NES模拟器
│   ├── nemu                    # NEMU
│   ├── slider                  # 简易图片浏览器
│   ├── thread-os               # 内核线程操作系统
│   └── typing-game             # 打字小游戏
└── tests                       # 一些具有针对性的测试集
    ├── am-tests                # 针对AM API实现的测试集
    └── cpu-tests               # 针对CPU指令实现的测试集
```

### inline assembly

[GCC-Inline-Assembly-HOWTO](http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)

### stdarg

C语言是支持变参数函数的，为了获得数目可变的参数，可以使用C库 `stdarg.h` 中提供的宏。

其实主要就是 `va_start`, `va_end`, `va_arg`, `va_copy` 这4个api，其中有一个比较坑的地方是 "If  ap  is passed to a function that uses va_arg(ap,type), then the value of ap is undefined after the return of that function."，导致在把各printf公用的参数解析功能抽取为函数时不小心触发 undefined behavior，极难排查（尤其是目前还没有实现printf打印），最后将函数转换为了宏定义才解决。

也意识到C语言的变参数功能确实相对简陋，如果想要将接收到的变参数传递给另一个变参函数，更简单的方法是为变参函数设计一个va_list类型的形参。

变参函数的底层原理其实是通过汇编到栈里面去逐个取被压栈的传入变量。


### trace config

在menuconfig中有几个关于trace的宏定义（其中CONFIG_WATCHPOINT是在pa1中添加的），RTFSC，弄清楚各配置项的作用：

```text
# Testing and Debugging
#
CONFIG_TRACE=y  # 是否启用日志记录
CONFIG_TRACE_START=0  # 日志记录从第0条指令开始
CONFIG_TRACE_END=10000  # 日志记录从第10000条指令结束
CONFIG_ITRACE=y  # 是否启用指令记录
CONFIG_ITRACE_COND="true"  # 如果启用指令记录，可以灵活定义记录条件
CONFIG_WATCHPOINT=y  # 是否启用监视点
# CONFIG_DIFFTEST is not set
CONFIG_DIFFTEST_REF_PATH="none"
CONFIG_DIFFTEST_REF_NAME="none"
# end of Testing and Debugging
```

注意区分 `Log` 和 `trace`，手动调用前者会同时在标准输出和日志中记录信息，后者负责自动记录某些特定场景的日志。


### symbols table

三类链接器符号：

Global symbols（模块内部定义的全局符号）
 – 由模块m定义并能被其他模块引用的符号。例如，非static C函数和非static的C全局变量(指不带static的全局变量)
External symbols（外部定义的全局符号）
 – 由其他模块定义并被模块m引用的全局符号
Local symbols（本模块的局部符号）
 – 仅由模块m定义和引用的本地符号。

强符号和弱符号：

– 函数名和已初始化的全局变量名是强符号
– 未初始化的全局变量名是弱符号

多重定义符号的处理规则：

Rule 1: 强符号不能多次定义
 – 强符号只能被定义一次，否则链接错误
Rule 2: 若一个符号被定义为一次强符号和多次弱符号，则按强定义为准
 – 对弱符号的引用被解析为其强定义符号
Rule 3: 若有多个弱符号定义，则任选其中一个
 – 使用命令 gcc –fno-common链接时，会告诉链接器在 遇到多个弱定义的全局符号时输出一条警告信息。

符号解析集合：

- E 将被合并以组成可执行文件的所有目标文件集合
- U 当前所有未解析的引用符号的集合
- D 当前所有定义符号的集合

更多相关知识，到袁春风老师《计算机系统基础》课程的第11章去回顾。

> 消失的符号

局部变量、形参不属于elf中关注的符号。

> 冗余的符号表

`.o` 文件需要使用符号表来进行重定位和链接，可执行文件已经完成了重定位和链接，不再需要使用符号表，因此使用 `strip -s` 丢弃符号表后依然可以执行。

> 寻找"Hello World!"

"Hello World!" 字符串在 `.rodata` 节中。

`.symtab` 中并没有直接存储符号信息，而是指向了 `strtab` 中对应的偏移地址。

#### hd

新认识到一个二进制处理工具 `hd`

```bash
❱❱❱ hd --help
Usage:
 hd [options] <file>...

Display file contents in hexadecimal, decimal, octal, or ascii.

Options:
 -b, --one-byte-octal      one-byte octal display
 -c, --one-byte-char       one-byte character display
 -C, --canonical           canonical hex+ASCII display
```

还有一个类似的工具 `hexdump`。


### ftrace

> 如何从jal和jalr指令中正确识别出函数调用指令和函数返回指令

|   rd  |  rs1  | rs1=rd | RAS action     |
|-------|-------|--------|----------------|
| !link | !link |    -   | none           |
| !link |  link |    -   | pop            |
|  link | !link |    -   | push           |
|  link |  link |    0   | pop, then push |
|  link |  link |    1   | push           |

In the above, link is true when the register is either x1 or x5.

手册里给了这样一张表，另外，"RISC-V Assembly Programmer's Handbook" 章节给了两个伪指令的基础格式：

| pseudoinstruction | Base Instruction                                                      | Meaning                  |
|:-----------------:|:----------------------------------------------------------------------|:-------------------------|
|        ret        | `jalr x0, 0(x1)`                                                        | Return from subroutine   |
|    call offset    | `auipc x1, offset[31 : 12] + offset[11]`</br> `jalr x1, offset[11:0](x1)` | Call far-away subroutine |

如果照汇编手册章节理解的话，只要 `rs1==x1 && rd==x0` 就是 ret，只要 `rs1==x1 && rd==x1` 就是call。

我是按照表定义的规则来实现的。


> 解析ELF文件

这个说来就话长了，还是 `man 5 elf` 吧。值得注意的一点是，一个elf文件中可能包含多个字符串表，当找到符号表之后，应该根据符号表对应的节头表项的link属性来确定相应的字符串表是哪一个。

还获得了一些乱七八糟的知识，
- `_start` 符号表项的 size 值为0，比较特别
- 符号表中有一些表项是 `STT_SECTION` 类型，这样的表项在对应的字符串表中是找不到信息的


> 不匹配的函数调用和返回

正巧，在验证ftrace实现效果的时候也发现了类似的问题： 对于普通案例来说，ftrace表现与预期相符。但是对于recursion.c递归调用这个案例，部分ret无法识别。

比如在f0()函数调用返回前，执行的指令为 `jalr ra, 0(a5)` ，的确不符合表中所述的pop情况，不应该被识别为ret指令，但实际上这里调用了f3()函数，并且在后续过程中由内层函数直接返回到f0()的外层函数。

此现象称为——"[尾递归](https://zhuanlan.zhihu.com/p/611472573)"。

> **常规递归和尾递归的区别在于函数的调用和返回顺序以及对栈空间的利用方式不同**
>
> 具体来说，常规递归的过程是：每次函数调用时需要记录当前函数上下文的信息，包括传入参数、局部变量值等，并将这些信息保存在栈空间（也称调用栈或运行时栈）中。在递归过程中，每一次递归调用都会新建一个对应的栈帧（栈的一层）并保存上述信息。
>
> 而尾递归则是指递归过程中，只有一个栈帧被不断的更新。即，函数执行完成后直接返回结果，不再进行任何其他操作。这样一来，由于只占用一个栈帧，程序在空间上会得到很大优化，极大地降低了内存负担。同时，尾递归也可以被优化为迭代的形式，进一步减少时间负担。
>
> 总的来说，尾递归在递归过程中最后一步执行递归函数的步骤，并且在函数结束前不再需要执行任何其他计算或操作；而常规递归则在递归嵌套过程中创建多个栈帧以保存函数调用的相关信息。

做了点简单的测试，普通的"[尾调用](https://en.wikipedia.org/wiki/Tail_call)"经gcc优化有时也可以具备和"尾递归"相同的效果。


## AM作为基础设施

### 如何生成native的可执行文件

> 阅读相关Makefile, 尝试理解abstract-machine是如何生成native的可执行文件的.

```makefile
image:
	@echo + LD "->" $(IMAGE_REL)
	@g++ -pie -o $(IMAGE) -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX) -lSDL2 -ldl

run: image
	$(IMAGE)

gdb: image
	gdb -ex "handle SIGUSR1 SIGUSR2 SIGSEGV noprint nostop" $(IMAGE)
```

查看 makefile 执行过程，生成native可执行文件主要包括4个步骤（以string.c为例）：
1. 编译得到 `string.o`
2. 编译并打包得到 `am-native.a`
3. 编译并打包得到 `klib-native.a`
4. `g++ -pie -o string-native -Wl,--whole-archive string.o am-native.a klib-native.a -Wl,-no-whole-archive -Wl,-z -Wl,noexecstack -lSDL2 -ldl` 得到可执行文件（命令中的路径被我简化了）

### gdb调试native

> 如何在选择 native 架构时使用 gdb 调试程序？

在 abstract-machine 的 Makefile 和 native.mk 中分别添加以下内容：

```makefile
# Makefile
ifeq ($(ISA), native)
  CFLAGS += -Og -ggdb3
endif
```

```makefile
# native.mk
image:
	@echo + LD "->" $(IMAGE_REL)
	@g++ -Og -ggdb3 -pie -o $(IMAGE) -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX) -lSDL2 -ldl
```

如此之后，便可支持在 native 模式下使用 gdb 调试。

### 奇怪的错误码

> 为什么错误码是1呢? 你知道make程序是如何得到这个错误码的吗?

**`make ALL=string ARCH=native run` 报错原理：**

```bash
Exit code = 01h
make[1]: *** [/home/vbox/ics2022/abstract-machine/scripts/native.mk:24: run] Error 1
 fail
[          fail] FAIL!
```

当 `check` 失败时，`check` 调用了 `halt(1)`，最终是调用了 `exit(1)`。

```c
void halt(int code) {
  const char *fmt = "Exit code = 40h\n";
  for (const char *p = fmt; *p; p++) {
    char ch = *p;
    if (ch == '0' || ch == '4') {
      ch = "0123456789abcdef"[(code >> (ch - '0')) & 0xf];
    }
    putch(ch);
  }
  __am_exit_platform(code);
  putstr("Should not reach here!\n");
  while (1);
}

void __am_exit_platform(int code) {
  // let Linux clean up other resource
  extern int __am_mpe_init;
  if (__am_mpe_init && cpu_count() > 1) kill(0, SIGKILL);
  exit(code);
}
```

```markdown
5.5 Errors in Recipes

After each shell invocation returns, make looks at its exit status. If the shell completed successfully (the exit status is zero), the next line in the recipe is executed in a new shell; after the last line is finished, the rule is finished.

If there is an error (the exit status is nonzero), make gives up on the current rule, and perhaps on all rules.

Sometimes the failure of a certain recipe line does not indicate a problem. For example, you may use the mkdir command to ensure that a directory exists. If the directory already exists, mkdir will report an error, but you probably want make to continue regardless.

To ignore errors in a recipe line, write a ‘-’ at the beginning of the line’s text (after the initial tab). The ‘-’ is discarded before the line is passed to the shell for execution.

The exit status of make is always one of three values:

- 0 - The exit status is zero if make is successful.
- 2 - The exit status is two if make encounters any errors. It will print messages describing the particular errors.
- 1 - The exit status is one if you use the ‘-q’ flag and make determines that some target is not already up to date. See Section 9.3 [Instead of Executing Recipes], page 111.
```
因此，当 exit 返回码为 1 时，make认为执行失败，显示报错。

**`make ALL=string ARCH=riscv32-nemu run` 报错原理：**

当 `check` 失败时，`check` 调用了 `halt(1)` -> `nemu_trap(code)` -> `asm volatile("mv a0, %0; ebreak" : :"r"(code))` 即调用内联汇编将 "1" 放入 `$a0` 10号寄存器中，并调用 `ebreak`。`ebreak` -> `NEMUTRAP(s->pc, R(10))` -> `set_nemu_state(NEMU_END, thispc, code)`，其中 code = R(10) = 1，当执行下一条指令时，监测到 `NEMU_END` 状态，程序结束。

### `__NATIVE_USE_KLIB__` 原理

> 为什么定义宏__NATIVE_USE_KLIB__之后就可以把native上的这些库函数链接到klib? 这具体是如何发生的? 尝试根据你在课堂上学习的链接相关的知识解释这一现象.

klib 的 `stdio.c`, `stdlib.c`, `string.c` 中均定义了 `#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)`。

当将 ARCH 指定为 `native` 时，abstract-machine 的 Makefile 会在 CFLAGS 中定义 `-D__ISA_$(shell echo $(ISA) | tr a-z A-Z)__`，即 `-D__ISA_NATIVE__` （tr实现小写转大写）。若不定义 `__NATIVE_USE_KLIB__` 则klib中的标准库函数不会被定义，在链接的过程中会自动链接 `libc.o` 标准库。同理，当不使用native架构或者定义了 `__NATIVE_USE_KLIB__` 时，klib中将会定义和标准库中同名的函数，链接优先级高于标准库，故最终使用klib定义的库函数。

⭐️可以先在native上用glibc的库函数来测试你编写的测试代码, 然后在native上用这些测试代码来测试你的klib实现, 最后再在NEMU上运行这些测试代码来测试你的NEMU实现.

### 可移植性的指针

> 具有移植性的指针类型

标准的整数类型 uintptr_t，它可以在任何平台上表示指针类型的大小。intptr_t。

### mainargs参数传递

> `make ARCH=native mainargs=h run` 指令中的mainargs是怎么传递到main方法中的？

在 `platform.c` 中定义了init_platform(constructor)，方法中通过getenv获取了"mainargs"，然后直接调用main方法。`native/trm.c` 中调用了 `__am_platform_dummy`，注释说明了 `__am_platform_dummy` 的作用（值得一看）。由于是直接在native环境中运行，`_start` 方法执行后会自动调用constructor，进而导致main方法的执行。


> `make ARCH=riscv32-nemu mainargs=h run` 指令中的mainargs是怎么传递到main方法中的？

`trm.c` 中直接执行了 `main(mainargs)`，其中 `static const char mainargs[] = MAINARGS;`，`MAINARGS` 又是在 `paltform/nemu.mk` 中通过 `CFLAGS += -DMAINARGS=\"$(mainargs)\"` 传入的。

### spike寄存器定义

> RTFSC, 找出spike中寄存器定义的顺序.

- spike-diff/difftest.cc中定义了DUT和REF之间约定的API。
- spike 中的寄存器类名叫 `regfile_t`，定义在 `decode.h` 中。
- spike 中的 RISC-V hart 类名叫 `state_t`，定义在 `preocessor.h` 中。
- 寄存器名字的定义在 `disasm/regnames.cc` 里面（找的我好苦啊，直接全局搜 `zero` 就出来了）

确认寄存器定义顺序一致后，直接调用 `memcmp` 比较内存是否一致即可完成 `isa_difftest_checkregs`。


### 捕捉死循环(有点难度)

TODO

### 拓展阅读

- [计算的极限](https://zhuanlan.zhihu.com/p/270155475)
- [KVM](https://www.linux-kvm.org/page/Main_Page) & [QEMU](http://www.qemu.org/) & [Spike](https://github.com/riscv-software-src/riscv-isa-sim)











