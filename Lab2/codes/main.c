#include "tree.h"
#include "semantic.h" // 引入语义分析头文件
#include <stdio.h>

extern int yyrestart(FILE *f);
extern int yyparse();

int error = 0;     // 错误计数器
Node *Root = NULL; // 语法树根节点

int main(int argc, char **argv) {
  if (argc <= 1)
    return 1;
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror(argv[1]);
    return 1;
  }

  yyrestart(f);
  yyparse();

  // 如果词法和语法分析过程中没有发现错误，开始语义分析
  if (error == 0 && Root != NULL) {
    // printTree(Root, 0); // 不再打印语法树
    
    // 1. 初始化符号表
    initHashtable();
    
    // 2. 从语法树根节点开始自顶向下做语义分析
    Program(Root);
  }

  delNode(Root); // 释放语法树内存空间
  fclose(f);
  return 0;
}