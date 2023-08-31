## 红白机运行报错解决

原理：

- https://stackoverflow.com/questions/56912619/variably-modified-stack-at-file-scope
- https://stackoverflow.com/questions/69719688/buildroot-error-when-building-with-ubuntu-21-10/69722830#69722830

怎么才能找到 SIGSTKSZ 这个宏变量定义的位置（ack试过了，glibc源码在哪）
STFW有人踩过这个坑了，这种排坑方式不理想，没有使用从源头排查问题的方法：https://zhuanlan.zhihu.com/p/463145529

From the C 2011 standard, 6.6, Constant Expressions: “An integer constant expression shall have integer type and shall only have operands that are integer constants, enumeration constants, character constants, sizeof expressions whose results are integer constants, _Alignof expressions, and floating constants that are the immediate operands of casts. Cast operators in an integer constant expression shall only convert arithmetic types to integer types, except as part of an operand to the sizeof or _Alignof operator.”

什么是 static arrays，integer constant expression的定义在哪里？


### libc、glibc、glib

下面的知识需要补充

查看glibc的版本：

```bash
getconf GNU_LIBC_VERSION
ldd --veresion
```

libc、glib、glibc简介：https://www.cnblogs.com/arci/p/14591030.html

glibc安装位置在哪里，系统自带的？这三个库的区别搞清楚

libc和glibc是同一类东西，他们之间的关系可以在 `man glibc` 中查看，里面讲的非常清楚，目前主流linux发行版用的c标准库都是glibc，通过 `whereis libc.so.6` 命令查看glibc库的位置。glibc是c语言第三方的轮子库，提供动态列表、链表、n叉数等工具。


## git实用命令拓展

重新建了个github库，但是没有完全实现预期效果，github上空的、无法打开的文件夹到底是怎么回事

由于子目录中有.git文件夹，导致子目录被作为了submodule，恢复方法：

```bash
git rm -rf --cached xxx/
git add xxx/
```

不需要提交到github才知道哪些文件被追踪了，哪些没有被git管理

```bash
git show xxx  # 查看某个版本的改动
git show HEAD^  # 查看上个版本提交的改动
git check-ignore -v xxx  # 查看指定文件是被哪一个配置给忽略了
git check-ignore xxx  # 查看当前目录被git忽略的文档
git status —ignored  # 查看当前目录被git忽略的文档
git ls-tree —name-only -r HEAD  # 查看git正在追踪的文件列表
```

额外学习一下git submodule，就是一个git项目可能依赖另一个git项目，但又有希望两个项目独立开发管理。主项目的子项目信息记录在 `.gitmodules` 文件中。

```bash
git submodule --help
git submodule --add git@github.com:xxx/yyy.git  # 在主项目下增加一个子项目
```

从远程拉取主项目后，获取到的只有主项目的最新代码，而子项目中是空的。需要我们在最初获取项目的时候使用两个指令来完成子项目的更新。

```bash
cd 子项目路径
git submodule init  # 初始化子项目
git submodule update  # 获取子项目远程最新的状态
```

注意：不管主项目中的是否有更新，只要子项目中修改过代码并提交过，主项目就需要再提交一次，因为子项目的修改是会同步到主项目中的，这样才能保证开发的一致性。

git是支持多个.gitignore配置的，子目录中的.gitignore配置负责该子目录中的忽略规则。


git 创建新的空分支

```bash
git checkout --orphan newbranchname
git rm -rf .
git commit --allow-empty
```


## shell

### 认识[shebang](https://en.wikipedia.org/wiki/Shebang_(Unix))

推荐使用 `env` 来增加可移植性：`#!/usr/bin/env python3`

不过，在zsh的实际使用中，shebang并不能完全确保生效


[The difference between test, \[ and \[\[](http://mywiki.wooledge.org/BashFAQ/031)

大致上来说，`[` 向前兼容，`[[` 是更为现代化的用法，支持更多test项，但是兼容性就会差一点，根据项目需求来选择（一般的项目用 `[[` 问题不大）

### tools

使用 [`fd`](https://github.com/sharkdp/fd#installation) 代替 `find` 命令，更简单方便

```bash
sudo apt install fd-find
# alias fd="/usr/bin/fdfind"
```


终于用上了tldr，推荐通过python的pip来安装（因为我不用npm）。

安装完成后可以执行 `tldr -u` 命令将常用的命令帮助缓存在本机。

由于网络问题，可能需要反复多执行几次。


### script 调用

在一个脚本中调用另一个脚本的[三种方式](https://www.yisu.com/zixun/407807.html)：

- fork 创建一个子进程来执行任务，任务执行完成后返回父进程继续执行
- exec 执行指定的任务，任务执行完成后直接退出，不再执行调用者后续的任务
- source 相当于合并任务到同一个进程中运行


## toml格式

- [TOML 官方文档（汉化）](https://github.com/LongTengDao/TOML/blob/%E9%BE%99%E8%85%BE%E9%81%93-%E8%AF%91/toml-v1.0.0.md)
- [TOML 中文教程](https://github.com/LongTengDao/TOML/wiki)


## RTFSC

### getopt

用来解析命令行参数的方法，

- getopt: 接受短参数项（`-`）
- getopt_long: 增加长参数项支持（`--`）
- getopt_long_only: 优先支持长参数项，只有当长参数项无法匹配时才会尝试匹配短参数项

主要是记录一下getopt中冒号（`:`）的含义（详细文档见 `man getopt_long` 的 "colon" 关键字）：

- 如果optstring中第一个字符不是 `:`，那么当出现未知参数项或缺少参数时将输出错误提示，getopt返回 `?`
- 如果optstring中第一个字符是 `:`，那么当出现未知参数项或缺少参数时将不输出错误提示，getopt返回 `:`
- 如果optstring中的参数项后不跟冒号，则表示该参数项不支持参数
- 如果optstring中的参数项后跟一个冒号，表示该参数项必须接受参数（此时参数项和参数之间有没有空格都可以）
- 如果optstring中的参数项后跟两个冒号，表示该参数项接受可选参数（此时参数项和参数之间不能有空格）


### constructor & destructor

首先需要区分一下 `ANSI C` 和 `GNU C`，`ANSI C` 是C语言的一个标准，定义了C语言的语法规则，
`GNU C` 在 `ANSI C` 的基础上增加了一些额外的拓展特性，`GNU C` 一般只在Linux环境下使用。

- [What is the difference between C, C99, ANSI C and GNU C?
Ask Question](https://stackoverflow.com/questions/17206568/what-is-the-difference-between-c-c99-ansi-c-and-gnu-c)
- [ANSI C标准 vs GNU C标准](https://blog.51cto.com/u_13933750/3229763)

[Attributes of Functions](https://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html#Function-Attributes) 就是 `GNU C` 支持的拓展语法之一。
因此，想要弄清楚 constructor 和 destructor 还得去看GNU C的[手册](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes)。

> GCC also supports attributes on variable declarations (see Variable Attributes), 
labels (see Label Attributes), enumerators (see Enumerator Attributes), 
statements (see Statement Attributes), types (see Type Attributes), 
and on field declarations (for tainted_args).

```c
#include <stdio.h>

int main() {
    printf("main function\n");
    return 0;
}

void __attribute__((constructor)) first() {
    printf("start\n");
}

void __attribute__((destructor)) last() {
    printf("last\n");
}
# $ ./a.out
# start
# main function
# last
```
如果有多个constructor或destructor，可以给每个constructor或destructor赋予优先级，
对于constructor，优先级数值越小，运行越早（优先级 0 到 100 为实现所保留，所以最小为101）。
destructor则相反，优先级数值越小，运行越晚（优先级 0 到 100 为实现所保留，所以最小为101）。

因此 `main` 方法并不是程序执行的开始/结束，程序启动时调用的第一个方法是 
[`_start` 方法](http://wen00072.github.io/blog/2015/02/14/main-linux-whos-going-to-call-in-c-language/)（可以通过断点调试查看函数调用栈验证）。


### exit

[C语言中这么骚的退出程序的方式你知道几个？](https://blog.csdn.net/weixin_47367099/article/details/127401312)

`on_exit`, `atexit`

两者都可以用来注册程序正常退出时（return或exit）的hook函数。
但是官方推荐尽可能使用 `atexit`，因为 `on_exit` 不是标准规定的方法。

一个程序中最多可以用 `atexit` 注册32个处理函数，这些处理函数的调用顺序与其注册的顺序相反。
`atexit` 和 `on_exit` 的调用时机在 destructor 之前。

`exit`, `_exit`, `return`, `abort`

- `exit`: 此函数由 `ANSI C` 定义，调用时会做大部分清理工作，但不会销毁局部对象，因为没有stack unwinding。
会进行的清理工作包括：销毁所有static和global对象，清空所有缓冲区，关闭所有I／O通道。
- `_exit`: 是一个系统调用，其效果等同于 `syscall(SYS_exit, 1)`，不会执行注册的hook退出函数
- `return`: 调用时进行stack unwinding，调用局部对象析构函数清理局部对象。如果在main中，则之后再交由系统调用exit()
- `abort`: 调用时不进行任何清理工作，直接终止程序，并产生 `SIGABRT` 信号。


### pmem映射

nemu中用pmem来代表内存，访问内存时涉及到 paddr_read/write 和 vaddr_read/write 两组函数，两者一个代表物理地址，一个代表虚拟地址，其中vaddr仅仅是paddr多一层的封装。

除了逻辑层面的区别以外，暂时看不出单独封装一层vaddr有什么特别的意义，也许以后会找到答案。


### 宏工具




## 基础设施

### 字符串转数字

```c
#include <stdlib.h>
strtol、strtoll、strtoul、strtoull
atoi, atol, atoll  // 是上面一行的简化版

#include <stdio.h>
scanf, sscanf  // 可以理解为是逆向版的printf

#include <string.h>
strtok  // 原地切分字符串，会把当前tocken后面的一个字符用`\0`替换
strtok_r  // 线程安全版strtok，需要单独传入一个指针用来维护切分位置

#include <errno.h>
// 定义常用的错误api，比如errno、perror
```

[char * 与char []区别总结](https://blog.csdn.net/bitcarmanlee/article/details/124166842)，简而言之就是，一个是常量指针，一个是指针常量。

在做 `基础设施` 章节的时候会用到很多字符串处理的函数，其中以 strtok 和 scanf 为主，在选择这两个函数时总是很纠结，
strtok 能够处理空字符串，scanf 必须提前过滤掉空字符串。如果不太考虑性能的话，其实先用 strtok 获取tocken，然后用scanf解析tocken逻辑会更清晰。


### si / info r / x

实现这些基础设施时需要充分考虑非法输入：

- 应该用哪种整数类型来接收 si/x 指令的步长？用word_t、int、unsign int、uint32_t、int64_t还是uint64_t？
- 如果步长传入非字符如何应对？
- 如果步长传入负数应该如何响应？参考一下gdb中x指令对负数步长的处理。
- 如果接收到类似 `x 5 0x80000004  abcd` 的指令应该如何应对？（额外做一次strtok）

响应内容需要友好，printf格式化字符串少不了要RTFM了。其实nemu已经内置了一些非常好用的格式化字符串宏，比如 `FMT_PADDR`, `PMEM_LEFT`, 甚至还可以方便的输出带颜色的字符！比如，`ANSI_FMT(FMT_PADDR, ANSI_FG_BLUE)`，RTFSC！

> NEMU默认会把单步执行的指令打印出来(这里面埋了一些坑, 你需要RTFSC看看指令是在哪里被打印的), 这样你就可以验证单步执行的效果了.

2021版pa在这里的坑应该是指si单步执行指令时打印的内存数据没有考虑小端序，但是2022版的pa好像把这个坑取消掉了，暂时没有发现其他坑。


## 表达式求值

### 正则

c语言的正则不支持 `\d` 这类表达式，另外，在构建正则表达式的时候需要注意转义字符。比如我希望在正则中解析一个普通的加号 `+`，那么正则解析函数需要接收到的字符串应该是 `\+`，但是c本身会把 `\+` 理解为转译加号，因此还需要为转译符加上一个转译符，即 `\\+`。逆向思考即可。

有精力的话还可以看看 `regcomp`, `regecex` 的文档。

### 递归求值

- [BNF (Backus–Naur form)](http://en.wikipedia.org/wiki/Backus%E2%80%93Naur_Form)
- [ANSI转义码的颜色功能](https://en.wikipedia.org/wiki/ANSI_escape_code#Colors)

> 我们在表达式求值中约定, 所有运算都是无符号运算. 你知道为什么要这样约定吗? 如果进行有符号运算, 有可能会发生什么问题?

暂时没有找到答案，理论上来说，对于加减运算完全没有影响，对于乘法运算没有显性的影响。反倒是在使用无符号数进行除法时，如果除数为负（识别为很大的正数），会导致除法结果为0，`12/(-3)`。

GPT 给的答案：在表达式求值中使用无符号运算的约定通常是为了避免在不同平台和编程语言之间出现不一致的结果。这种约定可以帮助确保数值计算的可移植性和一致性，尤其是涉及到底层的二进制表示和位操作的情况下。

> 如果生成的表达式有除0行为, 你编写的表达式生成器的行为又会怎么样呢?

> 乍看之下这个问题不好解决, 因为框架代码只负责生成表达式, 而检测除0行为至少要对表达式进行求值. 结合前两个蓝框题的回答(前提是你对它们的理解都足够深入了), 你就会找到解决方案了, 而且解决方案不唯一喔!

这里不是很理解，为什么一定要在表达式生成的时候判定除0行为，类似 `12 / (1 + 2 - 3)` 这样的表达式不是只有在求值的过程中才能发现存在除0行为么？如果是指在求值过程中解决除0行为，那还是很好办的。

其实这里用操作数栈和操作符栈来计算表达式，可能会更好实现一些，并且在处理错误表达式时也会更方便。

### 测试用例

居然测试案例也不是那么好写的。出于阅读测试用例生成源码和反复调试的需要，建议自己设置一下 gen-expr 目录下的 Makefile，减少肌肉劳损。完成后可以借助传递 `ARGS` 宏给make来实现生成指定数量的测试用例。

> 再稍微改造一下NEMU的main()函数, 让其读入input文件中的测试表达式后, 直接调用expr(), 并与结果进行比较. 为了容纳长表达式的求值, 你还需要对tokens数组的大小进行修改.

这时候，`getopt_long方法` 和 `main 方法的参数从何而来` 这两个问题掌握是否到位就体现出来了。

system() 和 popen()，都是基于fork的函数，直接RTFM即可。

> - 如何保证表达式进行无符号运算?

这里有两种理解上的争议：

1. 只是底层运算基于无符号数进行，计算结果应该是有符号数
2. 将表达式中的每个数都看作无符号数，计算结果应该是无符号数

上面两种理解这是有区别的， 比如 `(2 - 3) / 10`，

1. 在c语言中，数字常量默认是作为有符号数来进行的，`2 - 3 = -1`，表达式的结果应该是 0；
2. `2 - 3 = 4294967295`，表达式的结果应该是 429496729。

gdb 对上述表达式求值结果为0，而我个人目前实现的是第二种，从字面意思理解应该也是让实现第二种。

为什么会出现这种差异？明明无符号数和有符号数的二进制表示都是一样的。因为有符号和无符号整数乘、除法的实现不同。

- 乘法：若两数同号，有/无符号乘法得到的二进制结果总是相同，若两数异号，由于高32位用符号为补位，有符号数和无符号数计算得到的高32位不同，低32位总是相同。无论是否同号，截断后二进制总是相同。
- 除法：无符号数、带符号正整数(地板):移出的低位直接丢弃；带符号负整数(天板):加偏移量(2^k-1)，然后再右移k 位 ，低位截断(这里K 是右移位数)

故，在生成表达式测试用例时，如果希望使用第二种理解方式，就需要在每一个数字后面加上一个 `u` 字符（表达式解析对应的位置也需要改）。

> - 如何随机插入空格?

easy, pass.

> - 如何生成长表达式, 同时不会使buf溢出?
> - 如何过滤求值过程中有除0行为的表达式?

两种情况差不多，遇到异常表达式就重新生成。重点说一下除数为0的情况，
如果是直接出现除号后面突然生成了个 0，直接重新生成这个 0 就行了；
如果是除号后面生成的的表达式计算结果为 0，个人暂时没有能力在不做表达式求值的情况下识别该情况发生，把这个异常情况的处理留到编译阶段交给编译器处理，
如果监测到编译器报错，直接重新生成整个表达式。

### 栈求值

又单独做了一个基于栈的表达式求值，拓展了大部分c语言支持的操作运算符。

个人感想是，必须要有专门的方法用来确定操作符的优先级以及结合性，只要这两者被确定下来，无论是基于栈，还是基于BNF的方法，应该都是比较好实现的。

### 段错误

- Fault: 实现错误的代码, 例如if (p = NULL)
- Error: 程序执行时不符合预期的状态, 例如p被错误地赋值成NULL
- Failure: 能直接观测到的错误, 例如程序触发了段错误

调试其实就是从观测到的failure一步一步回溯寻找fault的过程, 找到了fault之后, 我们就很快知道应该如何修改错误的代码了.

segmentation fault 报错的产生原因：访问不存在的内存地址、访问系统保护的内存地址 、访问只读的内存地址、空指针废弃（eg:malloc与free释放后，继续使用）、堆栈溢出、内存越界（数组越界，变量类型不一致等）

**构造一个简单的段错误**

```c
#include <stdio.h>
#include <string.h>

int main(void) {
  char * str = "world";
  strcpy(str, "hello");
}
```

```bash
$ gcc tmp.c -o tmp && ./tmp
[1]    877167 segmentation fault (core dumped)  ./tmp
```

**排查方法一**

默认发生段错误后不会显示出错的位置，可以启用gcc的 `-rdynamic` 参数帮助排查段错误。

**排查方法二**

充分理解"核心转储 / core dumped"这个词，其实说的就是指生成coredump file，coredump file是程序运行状态的内存映象，如果生成则默认是在可执行文件的同一目录下，默认情况下一般是不会生成的（`ulimit -c` 查看允许生成core文件的大小）。

```bash
ulimit -c 1000  # 设置生成core文件最多1000 Byte
ulimit -c unlimited  # 设置不限制生成core文件的大小
```
简单实用core file

```bash
gdb -c <core file>
# 进入之后执行 `where` 就能看到程序是在哪里宕掉的
```

**其他排查方法**

也可以利用backtrace和objdump工具来排查问题。

**认识sanitize**

`man gcc` 查找 `-fsanitize` 相关的文档。

**调试器/断点实现原理**

- [how-debuggers-work-part-1](https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1/)
- [how-debuggers-work-part-2-breakpoints](https://eli.thegreenplace.net/2011/01/27/how-debuggers-work-part-2-breakpoints)
- [how-debuggers-work-part-3-debugging-information](https://eli.thegreenplace.net/2011/02/07/how-debuggers-work-part-3-debugging-information)
## pa1结束之前

### 阅读riscv32手册

- riscv32有哪几种指令格式?
- LUI指令的行为是什么?
- mstatus寄存器的结构是怎么样的?

### 代码统计

如果只是统计所有文件的修改数量，直接只用 `git diff pa0 --numstat` 即可。git diff 还有一个 `-X` 参数可以了解一下。

如果需要统计仅 .c, .h 文件：

```bash
# nemu/目录下的所有.c和.h和文件总共有多少行代码? 
cat $(find $NEMU_HOME -name "*.c" -o -name "*.h") | wc -l
# 除去空行之外, nemu/目录下的所有.c和.h文件总共有多少行代码?
cat $(find $NEMU_HOME -name "*.c" -o -name "*.h") | grep -v '^$' | wc -l
# 你在PA1中编写了多少行代码?
git diff pa0 -- $(find $NEMU_HOME -name "*.c" -o -name "*.h") | grep -E '^(\+|\-)' | grep -vE '^(\-{3}|\+{3})' | wc -l
# GNU 的 grep 工具不支持 `(?!expr)` 负向预查表达式，这里只好过滤两次
```


