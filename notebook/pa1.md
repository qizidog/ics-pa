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

### 字符串转数字

```c
#include <stdlib.h>
strtol、strtoll、strtoul、strtoull
atoi, atol, atoll  // 是上面一行的简化版

#include <stdio.h>
scanf, sscanf  // 可以理解为是逆向版的printf

#include <string.h>
strtok  // 原地切分字符串的方法

#include <errno.h>
// 定义常用的错误api，比如errno、perror
```

[char * 与char []区别总结](https://blog.csdn.net/bitcarmanlee/article/details/124166842)，简而言之就是，一个是常量指针，一个是指针常量。

完成 `printf` 格式输出RTFM。



