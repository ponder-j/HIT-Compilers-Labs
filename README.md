# HIT-Compilers-Labs

哈尔滨工业大学编译原理课程实验 2026 春季学期

本仓库按实验划分为 `Lab1`、`Lab2`、`Lab3` 三个目录。每个实验的源码都放在对应的 `codes` 目录下，并使用独立的 `makefile` 构建。

## 实验内容

### Lab 1: 词法分析、语法分析

使用 Flex 和 Bison 对 C-- 语言进行词法分析和语法分析。在输入程序没有词法或语法错误时，输出语法树。

### Lab 2: 语义分析

在 Lab 1 的基础上构建语法树并执行语义分析，检查变量、函数、结构体、数组和表达式类型等语义错误。

### Lab 3: 中间代码生成

在词法、语法和语义分析均通过后，生成 C-- 程序对应的线性中间代码。

## 目录结构

```text
.
├── Lab1/
│   └── codes/
├── Lab2/
│   └── codes/
├── Lab3/
│   └── codes/
└── README.md
```

各 `codes` 目录中的主要文件如下：

- `lexical.l`：Flex 词法规则
- `syntax.y`：Bison 语法规则
- `main.c`：编译器入口
- `tree.c` / `tree.h`：语法树结构与工具函数
- `semantic.c` / `semantic.h`：语义分析实现，Lab2 和 Lab3 使用
- `intercode.c` / `intercode.h`：中间代码生成实现，Lab3 使用
- `Test/`：实验测试用例
- `makefile`：当前实验的构建、测试和清理规则

## 构建环境

需要安装以下工具：

- `gcc`
- `flex`
- `bison`
- `make`

## 构建与运行

进入对应实验的 `codes` 目录后执行：

```sh
make
```

Lab1 和 Lab2 的可执行文件用法：

```sh
./parser Test/1.cmm
```

Lab3 的可执行文件用法：

```sh
./parser Test/1.cmm Result/1.ir
```

## 测试

各实验目录均提供 `test` 目标：

```sh
make test
```

Lab1 和 Lab2 会把每个 `Test/*.cmm` 的输出写入 `Result/*.txt`。Lab3 会把每个测试用例生成的中间代码写入 `Result/*.ir`。

## 清理

在对应实验的 `codes` 目录下执行：

```sh
make clean
```

该命令会删除 `parser`、Flex/Bison 自动生成文件、目标文件、依赖文件、测试输出和临时备份文件。
