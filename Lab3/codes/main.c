#include "tree.h"
#include "semantic.h" // 引入语义分析头文件
#include "intercode.h"
#include <stdio.h>

extern int yyrestart(FILE *f);
extern int yyparse();

int error = 0;     // 错误计数器
Node *Root = NULL; // 语法树根节点

/**
 * @brief 编译器主入口。
 *
 * 程序先完成词法/语法分析并构造语法树，再在无错误的前提下执行语义分析；
 * 若语义分析也通过，则继续生成 Lab3 要求的线性中间代码文件。
 *
 * @param argc 命令行参数个数，期望为 3。
 * @param argv 命令行参数数组，格式为 parser input.cmm output.ir。
 * @return int 0 表示成功，1 表示参数错误、文件打开失败或 IR 生成失败。
 */
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s input.cmm output.ir\n", argv[0]);
    return 1;
  }

  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror(argv[1]);
    return 1;
  }

  yyrestart(f);
  yyparse(); // 词法/语法分析阶段会构造全局语法树 Root，并更新全局 error

  // 如果词法和语法分析过程中没有发现错误，开始语义分析
  if (error == 0 && Root != NULL) {
    // printTree(Root, 0); // 不再打印语法树
    
    // 1. 初始化符号表
    initHashtable();
    addPredefinedFunctions(); // 预置 read/write
    
    // 2. 从语法树根节点开始自顶向下做语义分析
    Program(Root);

    // 3. 语义分析通过后生成实验三要求的中间代码文件
    if (error == 0) {
      if (!translateProgram(Root, argv[2])) {
        delNode(Root);
        fclose(f);
        return 1;
      }
    }
  }

  delNode(Root); // 释放语法树内存空间
  fclose(f);
  return 0;
}
