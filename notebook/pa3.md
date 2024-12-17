## 操作系统之缘起

要实现一个最简单的操作系统, 就要实现以下两点功能:

- 用户程序执行结束之后, 可以跳转到操作系统的代码继续执行
- 操作系统可以加载一个新的用户程序来执行

操作系统和其它用户程序还是不太一样的: 一个用户程序出错了, 操作系统可以运行下一个用户程序; 但如果操作系统崩溃了, 整个计算机系统都将无法工作. 所以, 人们还是希望能把操作系统保护起来, 尽量保证它可以正确工作. 在这个需求面前, 用call/jal指令来进行操作系统和用户进程之间的切换就显得太随意了.

为了阻止程序将执行流切换到操作系统的任意位置, 硬件中逐渐出现保护机制相关的功能, 比如

- i386中引入了保护模式(protected mode)和特权级(privilege level)的概念
- mips32处理器可以运行在内核模式和用户模式
- riscv32则有机器模式(M-mode), 监控者模式(S-mode)和用户模式(U-mode)

通常来说, 操作系统运行在S模式, 因此有权限访问所有的代码和数据; 而一般的程序运行在U模式, 这就决定了它只能访问U模式的代码和数据.

保护机制的本质: 在硬件中加入一些与特权级检查相关的门电路(例如比较器电路), 如果发现了非法操作, 就会抛出一个异常信号, 让CPU跳转到一个约定好的目标位置, 并进行后续处理.

两个史诗级漏洞 —— Meltdown和Spectre：https://meltdownattack.com/

## 中断/异常全流程分析

### CTE's API

除了通用寄存器gpr（general purpose registers）以外，不同架构的指令集往往还会使用一些特殊的寄存器来实现特定的功能，比如x86架构的EFLAGS寄存器，而在riscv架构下，使用csr (control and status registers) 控制状态寄存器。

在PA中, 除了用来存放异常入口地址的mtvec寄存器以外，我们只使用如下3个CSR寄存器:

- mepc - 存放触发异常的PC
- mstatus - 存放处理器的状态
- mcause - 存放触发异常的原因

riscv32 总共提供了 4096 个 CSR 寄存器，具体可查看 `riscv-privileged`-`2.2 CSR Listing`。上述 CSR 寄存器的索引号可以在 `Table 2.5: Currently allocated RISC-V machine-level CSR addresses` 中查到。

每个架构在实现各自的CTE API时, 都统一通过 `Event` 结构体来描述执行流切换的原因。

```c
typedef struct Event {
  enum { ... } event;  // 事件编号
  uintptr_t cause, ref;  // 补充信息
  const char *msg;  // 事件信息字符串
} Event;
```

描述上下文的结构体类型名统一为 `Context`, 由于不同架构之间上下文信息的差异过大，所以在 AM 中 Context 的具体成员是由不同的架构自己定义的。

另外两个统一的API:

- `bool cte_init(Context* (*handler)(Event ev, Context *ctx))` 用于进行CTE相关的初始化操作. 其中它还接受一个来自操作系统的事件处理回调函数的指针, 当发生事件时, CTE将会把事件和相关的上下文作为参数, 来调用这个回调函数, 交由操作系统进行后续处理.
- `void yield()` 用于进行自陷操作, 会触发一个编号为EVENT_YIELD事件. 不同的ISA会使用不同的自陷指令来触发自陷操作, 具体实现请RTFSC.

### CTE初始化及自陷操作

```c
// cte initialization
init_irq → cte_init → asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap)); → user_handler=do_event

// trap
yield → ecall → isa_raise_intr(NO, epc) → s->dnpc=csr[MTVEC] → __am_asm_trap/trap.S → __am_irq_handle(Context*) → user_handler/do_event(Event, Context*) → get back to __am_asm_trap/trap.S → mret → next instruction
```
```text
//  trap 流程
-> nanos-lite[main.c] main()

-----> abstact-machine[cte.c] yield()

---------> nemu[inst.c] ecall

-------------> nemu[intr.c] isa_raise_intr(NO, epc) // s->dnpc=csr[MTVEC]

-----------------> abstract-machine[trap.S] __am_asm_trap // 保存上下文+异常处理+恢复上下文

---------------------> abstract-machine[cte.c] __am_irq_handle(Contex*)

-------------------------> nanos-lite[irq.c] do_event(Event, Context*)

---------------------> abstract-machine[cte.c] __am_irq_handle(Context*)

-----------------> abstract-machine[trap.S] __am_asm_trap

// 经过mret之后pc值被还原为异常之前的pc值(或者由软件确认的是否+4的pc值)

---------> nemu[intr.c] mret

-----> abstact-machine[cte.c] yield()

-> nanos-lite[main.c] main()
```

#### cte initialization

`cte_init` 主要做了两件事情：

1. 将异常入口地址设置到 `mtvec` 寄存器
2. 将 `user_handler` 设置为 `do_event`

#### yield & ecall

为什么 `yield` 在执行 `ecall` 指令之前要先执行 `li a7, -1` ？

```
void yield() {
  asm volatile("li a7, -1; ecall");
}
```

在RISC-V架构中，ecall指令用于触发系统调用。在执行ecall之前，将a7寄存器的值设置为-1，是因为RISC-V使用a7寄存器来表示系统调用的编号。具体来说，当a7寄存器的值为-1时，表示要触发异常处理程序并进入操作系统的内核模式，而不是一个具体的系统调用。

#### isa_raise_intr

riscv32触发异常后硬件的响应过程如下:

1. 将当前PC值保存到mepc寄存器
2. 在mcause寄存器中设置异常号
3. 从mtvec寄存器中取出异常入口地址
4. 跳转到异常入口地址

这个过程在 `isa_raise_intr` 中完成。

`isa_raise_intr(word_t NO, vaddr_t epc)` 接收两个参数，其中 `NO` 用于设置 `mcause`。

```c
INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , I, s->dnpc = isa_raise_intr(?, s->pc));
```

那么，在 `ecall` 中调用 `isa_raise_intr` 时，`NO` 应该设置为多少呢？

通过 diff-test 可以发现在 `trap.S` 中执行 `csrr t0, mcause` 指令后 `t0` 寄存器出错，正确的 `t0` 值应该是 `0xb`（这个bug真的是查了我好久好久）。

```txt
Register information display as follows:
        $0      0           |   ra      0x80000190
        sp      0x80013f50  |   gp      0
        tp      0           |   t0      0
        t1      0           |   t2      0
        s0      0x80002000  |   s1      0x80002000
        a0      0x54        |   a1      0x1
        a2      0x10        |   a3      0
        a4      0x80001e3c  |   a5      0
        a6      0x80001d2c  |   a7      0xffffffff
        s2      0           |   s3      0
        s4      0           |   s5      0
        s6      0           |   s7      0
        s8      0           |   s9      0
        s10     0           |   s11     0
        t3      0           |   t4      0
        t5      0           |   t6      0
        pc      0x80000678
=============Reference CPU Status===============
        $0      0           |   ra      0x80000190
        sp      0x80013f50  |   gp      0
        tp      0           |   t0      0xb
        t1      0           |   t2      0
        s0      0x80002000  |   s1      0x80002000
        a0      0x54        |   a1      0x1
        a2      0x10        |   a3      0
        a4      0x80001e3c  |   a5      0
        a6      0x80001d2c  |   a7      0xffffffff
        s2      0           |   s3      0
        s4      0           |   s5      0
        s6      0           |   s7      0
        s8      0           |   s9      0
        s10     0           |   s11     0
        t3      0           |   t4      0
        t5      0           |   t6      0
        pc      0x80000678
[src/cpu/cpu-exec.c:128 cpu_exec] nemu: ABORT at pc = 0x80000674
```

到此，需要反问一下，为什么应该将 `mcause` 设置为 `0xb`？

> ecall的功能：The ECALL instruction is used to make a request to the supporting execution environment. When executed in U-mode, S-mode, or M-mode, it generates an environment-call-from-U-mode exception, environment-call-from-S-mode exception, or environment-call-from-M-mode exception, respectively, and performs no other operation.

此时需要阅读 `riscv-privileged` 的 `3.1.15 Machine Cause Register (mcause)` 章节，通过 `Table 3.6: Machine cause register (mcause) values after trap` 可以解答上述问题。

> 谈一谈中断和异常
>
> - 中断(Interrupt)机制,即处理器核在顺序执行程序指令流的过程中突然被别的请求打断而中止执行当前的程序,转而去处理别的事情,待其处理完了别的事情,然后重新回到之前程序中断的点继续执行之前的程序指令流。中断处理是一种正常的机制,而非一种错误情形。中断源通常来自于外围硬件设备,是一种外因。
> - 异常(Exception)机制,即处理器核在顺序执行程序指令流的过程中突然遇到了异常的事情而中止执行当前的程序,转而去处理该异常。异常是由处理器内部事件或程序执行中的事件引起的, 譬如本身硬件故障、程序故障,或者执行特殊的系统服务指令而引起的,简而言之是一种内因。
> - 中断和异常最大的区别是起因内外有别。除此之外,从本质上来讲,中断和异常对于处理器而言基本上是一个概念。处理器广义上的异常,通常只分为同步异常(Synchronous Exception)和异步异常(Asynchronous Exception) 。

上述的 `yield` 指令属于"内因"，因此属于异常，而非中断（对应的 mcause 为 11，即 0xb，Environment call from M-mode）。

#### \_\_am_asm_trap

`__am_asm_trap` 函数在 `trap.S` 中定义，是异常入口函数，主要作用是为 `__am_irq_handle` 准备 `Context` 参数。

Context 需要保存的上下文信息包括：

1. 通用寄存器。riscv32将包含的32个通用寄存器依次压栈。
2. 触发异常时的PC和处理器状态。riscv32将相关信息保存在mstatus寄存器中，我们还需要将它们从系统寄存器中读出并保存在堆栈上。
3. 异常号。riscv32的异常号已经由硬件保存在mcause寄存器中，我们还需要将其保存在堆栈上。
4. 地址空间。这是为PA4准备的，riscv32将地址空间信息与0号寄存器共用存储空间。

`__am_asm_trap` 通过将结构体的各成员属性依次压栈构造 `Context` 实参，注意压栈顺序与结构体成员的定义顺序相反，定义顺序靠前的成员分配在栈的低地址。据此重新组织abstract-machine/am/include/arch/$ISA-nemu.h 中定义的Context结构体的成员。

#### \_\_am_irq_handle

`__am_irq_handle` 的主要作用是事件分发，其本身很简单，值得讨论的是 `mret` 指令的实现。

> 事实上, 自陷只是异常类型中的一种. 有一种故障类异常, 它们返回的PC和触发异常的PC是同一个, 例如缺页异常, 在系统将故障排除后, 将会重新执行相同的指令进行重试, 因此异常返回的PC无需加4. 所以根据异常类型的不同, 有时候需要加4, 有时候则不需要加.
> 这里需要注意之前自陷指令保存的PC, 对于x86的int指令, 保存的是指向其下一条指令的PC, 这有点像函数调用; 而对于mips32的syscall和riscv32的ecall, 保存的是自陷指令的PC, 因此软件需要在**适当的地方**对保存的PC加上4, 使得将来返回到自陷指令的下一条指令.

risc-v 是精简指令集，通过软件来决定是否要对pc加4。在实现 `mret` 的过程中遇到一个坑排查了很久，最后是借助群友"小路 | ysyx"的提示解决的。

```c
// 实现一
INSTPAT("0011000 00010 00000 000 00000 11100 11", mret   , R, s->dnpc = csr(MEPC));
// 实现二
INSTPAT("0011000 00010 00000 000 00000 11100 11", mret   , R, s->dnpc = csr(MEPC)+4);
```

当采用"实现一"时，执行进入ecall调用的死循环，当采用"实现二"时，执行difftest显示pc寄存器出错。

在坑中时的想法：经多方查证，mret指令本身确实不需要将pc+4，"实现二"difftest不通过合情合理。但调用ecall的时候将ecall指令自身的pc放入了mepc，而执行mret指令会将mepc的值重新写入pc，这就导致即将被执行的下一条指令还是ecall指令，周而复始重复执行。那么到底怎样才能避免始终重复执行同一条指令呢？

最终反应过来，应该在执行mret之前，`__am_irq_handle` 中（由am定义的软件部分）控制 `mepc` 寄存器的值。

为什么是在 `__am_irq_handle` 而不是 `user_handler` 中控制 `mepc` 寄存器的值呢？

因为 `user_handler`/`do_event` 由 `nanos-lite` 框架提供，本身是isa无关的，`mepc` 寄存器仅存在于 risc-v 指令集中。

## 用户程序加载

> 加载用户程序是由loader(加载器)模块负责的, 加载的过程就是把可执行文件中的代码和数据放置在正确的内存位置, 然后跳转到程序入口, 程序就开始执行了.

> 在实现loader的过程中, 需要找出每一个需要加载的segment的Offset, VirtAddr, FileSiz和MemSiz这些参数.
> 其中相对文件偏移Offset指出相应segment的内容从ELF文件的第Offset字节开始, 在文件中的大小为FileSiz, 它需要被分配到以VirtAddr为首地址的虚拟内存位置, 在内存中它占用大小为MemSiz.
> 也就是说, 这个segment使用的内存就是 [VirtAddr, VirtAddr + MemSiz) 这一连续区间, 然后将segment的内容从ELF文件中读入到这一内存区间, 并将 [VirtAddr + FileSiz, VirtAddr + MemSiz) 对应的物理区间清零.

### api库的引入

在加载用户程序的过程中，必然要解析elf文件，其实在pa2中实现ftrace的时候已经做过这个功能了，但是这里没办法直接使用，因为nanos运行在nemu上，有些当初在nemu中能使用的api（宿主机系统提供的），nemu并没有提供。

又是一个折磨+融会贯通的过程：navy-apps中提供了newlib c库，正好可以用来解决nanos中没有对应api的问题！
那么怎么才能在nanos中启用newlib c呢？那就需要RTFSC查看Makefile，并回顾可重定位目标文件的链接过程。

顺便一说，abstract-machine 的 Makefile 中 `$(LIBS)` 和 `$(LINKAGE)` 两个变量具体是怎么产生的值得仔细研究。
于是，选择在nanos的Makefile中增加对应的规则，将libc和libos两个库链接到nanos的elf文件中。至此，终于可以开始着手实现解析elf文件和加载用户程序的功能了。

但是经过实测，还是不能在nanos中通过nemu直接读取ramdisk.img文件，反思发现是因为nemu只能够访问特定的内存区段，无法直接读取硬盘上任意位置的文件。

思考ramdisk、nanos、am、klib、nemu的关系：nemu是模拟的cpu等硬件，其上提供am和klib等接口（其实也是跑在nemu上的程序）供上层应用程序使用，nanos是一个运行在nemu硬件上的特殊程序（特殊在可以加载ramdisk这样的普通应用程序）。实际上从编译过程来看，编译对象只分为两部分，一部分是nemu，另一部分是被打包编译到一起的ramdisk、nanos、am、klib，经打包处理成bin文件后可以被nemu执行。其中在编译构建时，`ramdisk.img` 可执行目标文件是作为数据直接通过 `resources.S` 嵌入到了nanos可执行目标文件的.data节中了。

需要关注的几个文件：

- $(NEMU_HOME)/src/memory/Kconfig: 定义nemu中内存的起始地址为0x80000000（不是宿主机中的地址）
- $(AM_HOME)/scripts/linker.ld: 分配规划nemu中的内存使用总布局（声明并使用了_pmem_start）
- $(AM_HOME)/scripts/platform/nemu.mk: 定义了_pmem_start=0x80000000
- $(NANOS_HOME)/src/resources.S: 定义nanos中ramdisk.img被加载到.data节，其数据开始位置被定义为ramdisk_start
- $(NAVY_HOME)/scripts/riscv32.mk: 定义dummy/ramdisk.img程序中代码段的起始地址text-segment为0x83000000

经上述分析可以发现，其实ramdisk.img已经作为nanos的.data节数据被加载到nemu的内存中了，只是没有被当做可执行文件来执行，后续需要通过loader将其加载到nemu内存中恰当的位置来执行。

那么怎么在nanos中找到.data中的ramdisk.img呢？其实在 `resources.S` 中已经打好了标记——`ramdisk_start`，甚至 `ramdisk.c` 中已经贴心的准备好了api（同时也意味着前面费尽心思引入newlib的库函数是徒劳的）。

### 检测ELF文件

#### 魔数检测

其实在pa2实现ftrace的时候就已经实现过检查魔数（0x7f454c46）了，这次突然发现在 `elf.h` 中其实已经定义了一个魔数的宏 `ELFMAG`，可以直接使用。

#### ISA类型检测

AM中的ISA宏是在abstract-machine的Makefile中定义的。

```Makefile
CFLAGS   += -O2 -MMD -Wall -Werror $(INCFLAGS) \
            -D__ISA__=\"$(ISA)\" -D__ISA_$(shell echo $(ISA) | tr a-z A-Z)__ \
```

ELF支持的ISA，可以查看 `elf.h` 中对 `e_machine` 字段的定义。

### 正式加载

其实剩下的部分和实现ftrace已经差不多了，甚至还要更简单些，不用处理字符串表。

- 从ELF头中提取程序头表的数量和起始偏移位置
- 逐个读取程序头表信息，根据程序段的类型、偏移位置、大小将其加载到内存指定位置
- 注意内存尺寸大于文件尺寸的区域需要清零

参考阅读： [COMPILER, ASSEMBLER, LINKER AND LOADER:
A BRIEF STORY](https://www.tenouk.com/ModuleW.html)
```c
// 一句神奇的代码
((void(*)())entry) ();
```

实现两个功能:

1. 将entry强制转化成函数指针
2. 程序跳转到绝对地址entry去执行

## syscall 系统调用

### csr difftest

> 实现loader后, 在init_proc()中调用naive_uload(NULL, NULL), 它会调用你实现的loader来加载第一个用户程序, 然后跳转到用户程序中执行. 如果你的实现正确, 你会看到执行dummy程序时在Nanos-lite中触发了一个未处理的4号事件. 这说明loader已经成功加载dummy, 并且成功地跳转到dummy中执行了. 关于未处理的事件, 我们会在下文进行说明.

这里说的未处理的4号事件，其实是 `__am_irq_handle` 的默认分支 `default: ev.event = EVENT_ERROR; break;` 进入 `do_event` 后触发了  `default: panic("Unhandled event ID = %d", e.event);`。

> 踩坑情况实录1：实现完load之后启动nemu并加载dummy，程序执行到最后就卡住没反应了，进入了未知死循环，这时log的重要性就体现出来了。默认情况下nume的日志只记录10000行指令，为了彻底排查问题产生原因，可以在menuconfig中把日志记录的上限设为 `-1`，确保所有的执行指令都被记录下来，然后合理利用正则过滤掉无价值信息，辅助分析。

日志显示：
- 总共调用了两次ecall（和nanos main方法中的yield没有关系，还没有执行到他）
- 总共调用了两次mret，一次是syscall，一次是exit（这次出错，mret之后pc循环指向 0x830049d4: jal zero, 0）

> 踩坑情况实录2：ecall 传递 mcause 系统调用号的矛盾，作为difftest的spike总是使用11作为系统调用号，`__syscall__` 使用1，yield使用-1。

经历了无数折磨终于醒悟，没有完善difftest csr检测带来的麻烦。因为没有完善 `diff_set_regs` 和 `diff_get_regs` 方法，~~初始化时设置的 `mstatus = 0x1800` 没有被同步至spike~~（同步了），csr寄存器的差异总是要延迟至通用寄存器时才能体现。

所以，完善csr相关的difftest，并且优化csr的结构设计，不再使用4096大小的数组来存储csr，而是直接将csr放到cpu的结构中，方便和difftest做比对。

既然都到这一步了，就分析下设置 `mstatus = 0x1800` 的效果，实际上就是让MPP位值为11，其余位全为0，即机器模式的上一个特权级别为机器模式。

```bash
# mstatus
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| 31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 |
| 15 | 14 | 13 | 12 | 11 | 10 | 9  | 8  | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 1  |
| 1  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  |
```

目前没有搞定spike总是使用11作为系统调用号应该怎样调整（虽然这是符合手册要求的），目前暂时选择使用 `difftest_skip_ref` 来跳过这一步的difftest比对（mret处也先用同样的方式跳过）。

> RISC-V Linux为什么没有使用a0来传递系统调用号？

使用a7寄存器来传递系统调用号，a0一般用来传递函数调用的第一个参数。

**man syscall**:

```
The first table lists the instruction used to transition to kernel mode.

Arch/ABI    Instruction           System  Ret  Ret  Error    Notes
                                  call #  val  val2
───────────────────────────────────────────────────────────────────
arm64       svc #0                w8      x0   x1   -
i386        int $0x80             eax     eax  edx  -
ia64        break 0x100000        r15     r8   r9   r10      1, 6
mips        syscall               v0      v0   v1   a3       1, 6
riscv       ecall                 a7      a0   a1   -
x86-64      syscall               rax     rax  rdx  -        5


The second table shows the registers used to pass the system call arguments.

Arch/ABI      arg1  arg2  arg3  arg4  arg5  arg6  arg7  Notes
──────────────────────────────────────────────────────────────
alpha         a0    a1    a2    a3    a4    a5    -

arm64         x0    x1    x2    x3    x4    x5    -
i386          ebx   ecx   edx   esi   edi   ebp   -
ia64          out0  out1  out2  out3  out4  out5  -
mips/o32      a0    a1    a2    a3    -     -     -     1
mips/n32,64   a0    a1    a2    a3    a4    a5    -
riscv         a0    a1    a2    a3    a4    a5    -
x86-64        rdi   rsi   rdx   r10   r8    r9    -
```

### 异常 & 中断

– 异常（内部）trap：在CPU内部发生的意外事件或特殊事件，按发生原因分为硬故障中断和程序性中断两类
  - 硬故障中断：如电源掉电、硬件线路故障等
  - 程序性中断：执行某条指令时发生的“例外(Exception)”事件，如溢出、缺页、越界、越权、越级、非法指令、除数为0、堆/栈溢出、 访问超时、断点设置、单步、系统调用等

– 中断（外部）：在CPU外部发生的特殊事件，通过“中断请求”信号向CPU请求处理
  - 如实时钟、控制台、打印机缺纸、外设准备好、采样计时到、DMA传输结束等

### strace

在menuconfig中增加对应的配置即可，需要考虑的是如何在nanos中使用menuconfig中生成的宏定义。

观察am和klib，均未使用过menuconfig生成的宏定义，那么主要的问题是“应不应该在nanos中引入”，而不是“怎样引入”。

nemu, an, klib, nanos 总体来说应该是比较独立的项目，个人决定还是需要把 `autoconf.h` 引入到nanos中，这应该是相对较优的选择，但是直接将nemu中include下的全部头文件都引入吗？这可能不太合适，容易出现重复定义的问题。

在仅引入 `autoconf.h` 的情况下，`IFDEF` 宏是无法使用的（被定义在了 `macro.h` 中），于是就有了下面这样的定义：

```c
#ifndef CONFIG_STRACE
# define CONFIG_STRACE 0
#endif
#define INVOKE_STRACE(c) if (CONFIG_STRACE) Log("[STRACE] syscall " #c " with NO = %d", c)
```

### write

> 你需要在do_syscall()中识别出系统调用号是SYS_write之后, 检查fd的值, 如果fd是1或2(分别代表stdout和stderr), 则将buf为首地址的len字节输出到串口(使用putch()即可). 最后还要设置正确的返回值(参考man 2 write).

```bash
man 2 write
# ---------------------------------------------------
ssize_t write(int fd, const void *buf, size_t count);

write() writes up to count bytes from the buffer starting at buf to the file referred to by the file descriptor fd.
  - On success, the number of bytes written is returned.
  - On error, -1 is returned, and errno is set to indicate the cause of the error.
```

> Navy中提供了一个hello测试程序(navy-apps/tests/hello), 它首先通过write()来输出一句话, 然后通过printf()来不断输出. 你需要实现write()系统调用, 然后把Nanos-lite上运行的用户程序切换成hello程序来运行.

P.S. 在没实现下面的堆区管理前printf只会输出第一个字符，这是正常的。

### sbrk

> 在Navy的Newlib中, sbrk() -> _sbrk() -> SYS_brk -> sys_brk(), 工作逻辑如下:
>
> 1. program break一开始的位置位于_end
> 2. 被调用时, 根据记录的program break位置和参数increment, 计算出新program break
> 3. 通过SYS_brk系统调用来让操作系统设置新program break
> 4. 若SYS_brk系统调用成功, 该系统调用会返回0, 此时更新之前记录的program break的位置, 并将旧program break的位置作为_sbrk()的返回值返回
> 5. 若该系统调用失败, _sbrk()会返回-1

重点是搞清楚sbrk和brk在成功和失败情况下的返回值分别是什么，不要搞混了。

```bash
man 2 sbrk
# ---------------------------------------------------
int brk(void *addr);
void *sbrk(intptr_t increment);

brk() sets the end of the data segment to the value specified by addr.
  - On success, brk() returns zero.
  - On error, -1 is returned, and errno is set to ENOMEM.
sbrk() increments the program's data space by increment bytes.  Calling sbrk() with an increment of 0 can be used to find the current location of the program break.
  - On  success, sbrk() returns the previous program break. (If the break was increased, then this value is a pointer to the start of the newly allocated memory).
  - On error, (void *) -1 is returned, and errno is set to ENOMEM.

man 3 end
# ---------------------------------------------------
The addresses of these symbols indicate the end of various program segments:
  - etext  This is the first address past the end of the text segment (the program code).
  - edata  This is the first address past the end of the initialized data segment.
  - end    This is the first address past the end of the uninitialized data segment (also known as the BSS segment).
```

> 如果你的实现正确, 你可以借助strace看到printf()不再是逐个字符地通过write()进行输出, 而是将格式化完毕的字符串通过一次性进行输出.

再次遇到一个大坑，个人实现的sbrk总是只能逐个输出字符，debug过程中被两个问题困扰：

1. 在`_sbrk`函数（navy）中`&_end`的值是`0x830068dc`，在`sys_brk`函数（nanos）中`&_end`的值是`0x8001c000`
  - 第一反应是`_sbrk`中的值有问题，可能是类型转换造成的
  - 反复碰壁后开始思考会不会两个函数中的`&_end`值都没有问题，那么应该以哪一个值作为堆地址
2. 怎样修改`&_end`的值

结果完全走偏了方向：

1. `_sbrk`函数（navy）中`&_end`反应的是hello程序的堆地址，`sys_brk`函数（nanos）中`&_end`反应的是nemu/nanos系统的堆地址
2. `&_end`的值是根本没法改变的，需要自己建立一个变量来维护堆地址

看到网上有些帖子是用静态局部变量来维护堆地址的，值得借鉴。

```c
// 目前sys_brk函数里面只需要写个返回值就行了
// 设置新地址由_sbrk完成了
extern char _end;
void *_sbrk(intptr_t increment) {
  static char *prog_brk = &_end;
  int ret_brk = _syscall_(SYS_brk, increment, 0, 0);
  if (ret_brk == 0) {  // success
    void *old_brk = prog_brk;
    prog_brk += increment;
    return (void*)old_brk;
  } else if (ret_brk == -1) {  // failed
    return (void*)-1;
  }
  assert(0);  // error
}
```

## 文件系统

实现难度不大，主要是理清楚各组函数接口之间绕来绕去的调用关系。

```c
// 应用程序
FILE *fopen( const char *filename, const char *mode );
size_t fread( void *buffer, size_t size, size_t count, FILE *stream );
size_t fwrite( const void *buffer, size_t size, size_t count, FILE *stream );
int fclose( FILE *stream );
int fseek( FILE *stream, long offset, int origin );

// 系统调用：navy
int open(const char *pathname, int flags, int mode);  // _open
size_t read(int fd, void *buf, size_t len);           // _read
size_t write(int fd, const void *buf, size_t len);    // _write
int close(int fd);                                    // _close
size_t lseek(int fd, size_t offset, int whence);      // _lseek

// syscall.c：nanos
static void sys_open(Context *c);
static void sys_read(Context *c);
static void sys_write(Context *c);
static void sys_close(Context *c);
static void sys_lseek(Context *c);

// 文件系统：nanos
int fs_open(const char *pathname, int flags, int mode);
size_t fs_read(int fd, void *buf, size_t len);
size_t fs_write(int fd, const void *buf, size_t len);
size_t fs_lseek(int fd, size_t offset, int whence);
int fs_close(int fd);

// ramdisk.c：nanos
size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
```

> 最后你还需要在Nanos-lite和Navy的libos中添加相应的系统调用, 来调用相应的文件操作.

调用逻辑关系为从上到下依次调用，全靠讲义一句提示来推断。


### gettimeofday

> 实现gettimeofday系统调用, 这一系统调用的参数含义请RTFM. 实现后, 在navy-apps/tests/中新增一个timer-test测试, 在测试中通过gettimeofday()获取当前时间, 并每过0.5秒输出一句话.

注意是作为系统调用来实现，总体实现不算复杂，主要在于需要调用pa2中实现的 `io_read(AM_TIMER_UPTIME)` 接口，好好回顾一下pa2的IOE是怎么实现的吧。

> 在Navy中提供了一个叫NDL(NJU DirectMedia Layer)的多媒体库, 你需要用gettimeofday()实现NDL_GetTicks(), 然后修改timer-test测试, 让它通过调用NDL_GetTicks()来获取当前时间.

NDL_GetTicks 效果类似于 `SDL_GetTicks -- Get the number of milliseconds since the SDL library initialization.`

其实就是在NDL初始化时记录一个事件，在NDL_GetTicks中计算时间差，和nemu中计算程序运行时间同理。


### NDL_PollEvent

> - 实现events_read()(在nanos-lite/src/device.c中定义), 把事件写入到buf中, 最长写入len字节, 然后返回写入的实际长度. 其中按键名已经在字符串数组names中定义好了, 你需要借助IOE的API来获得设备的输入. 另外, 若当前没有有效按键, 则返回0即可.
> - 在VFS中添加对/dev/events的支持.
> - 在NDL中实现NDL_PollEvent(), 从/dev/events中读出事件并写入到buf中.

需要图形化界面支持！

借机回顾了一下pa2中IOE相关的内容。



