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
yield → ecall → isa_raise_intr → csr[MTVEC] → __am_asm_trap(trap.S) → __am_irq_handle → user_handler
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

此时需要阅读 `riscv-privileged` 的 `3.1.15 Machine Cause Register (mcause)` 章节，通过 `Table 3.6: Machine cause register (mcause) values after trap` 可以解答上述问题。

> 谈一谈中断和异常
> - 中断(Interrupt)机制,即处理器核在顺序执行程序指令流的过程中突然被别的请求打断而中止执行当前的程序,转而去处理别的事情,待其处理完了别的事情,然后重新回到之前程序中断的点继续执行之前的程序指令流。中断处理是一种正常的机制,而非一种错误情形。中断源通常来自于外围硬件设备,是一种外因。
> - 异常(Exception)机制,即处理器核在顺序执行程序指令流的过程中突然遇到了异常的事情而中止执行当前的程序,转而去处理该异常。异常是由处理器内部事件或程序执行中的事件引起的, 譬如本身硬件故障、程序故障,或者执行特殊的系统服务指令而引起的,简而言之是一种内因。
> - 中断和异常最大的区别是起因内外有别。除此之外,从本质上来讲,中断和异常对于处理器而言基本上是一个概念。处理器广义上的异常,通常只分为同步异常(Synchronous Exception)和异步异常(Asynchronous Exception) 。

上述的 `yield` 指令属于"内因"，因此属于异常，而非中断（对应的 mcause 为 11，即 0xb，Environment call from M-mode）。

#### __am_asm_trap

`__am_asm_trap` 函数在 `trap.S` 中定义，是异常入口函数，主要作用是为 `__am_irq_handle` 准备 `Context` 参数。

Context 需要保存的上下文信息包括：

1. 通用寄存器。riscv32将包含的32个通用寄存器依次压栈。
2. 触发异常时的PC和处理器状态。riscv32将相关信息保存在mstatus寄存器中，我们还需要将它们从系统寄存器中读出并保存在堆栈上。
3. 异常号。riscv32的异常号已经由硬件保存在mcause寄存器中，我们还需要将其保存在堆栈上。
4. 地址空间。这是为PA4准备的，riscv32将地址空间信息与0号寄存器共用存储空间。

`__am_asm_trap` 通过将结构体的各成员属性依次压栈构造 `Context` 实参，注意压栈顺序与结构体成员的定义顺序相反，定义顺序靠前的成员分配在栈的低地址。据此重新组织abstract-machine/am/include/arch/$ISA-nemu.h 中定义的Context结构体的成员。

#### __am_irq_handle

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

