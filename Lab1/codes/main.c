#include <stdio.h>
#include "tree.h"

extern int yyrestart(FILE* f);
extern int yyparse();

int error = 0;
Node* Root = NULL;

int main(int argc, char** argv) 
{
    if (argc <= 1) return 1;
    FILE* f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    
    yyrestart(f);
    yyparse();
    
    // 如果词法和语法分析过程中没有发现错误，才打印语法树
    if (error == 0 && Root != NULL) {
        printTree(Root, 0);
    }
    
    delNode(Root); // 释放语法树内存空间
    fclose(f);
    return 0;
}