#ifndef TREE_H
#define TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


extern int yylineno;
struct Node{   //可以按需修改结构体
    char name[32]; /* 文法符号 */
    char yytext[32]; /* （当节点为词法单元/终结符时）词法单元的词素 */
    struct Node *child[10]; /* 子节点 */
    int childno; /* 子节点数 */
    int lineno; /* 程序行号 */
    int isToken; /* 是否为词法单元/终结符 */
};
//按需自定义相关函数
typedef struct Node Node;
void delNode(Node* node);  //释放语法树内存空间
Node* createNode(char* name, char* text, int lineno, int isToken);  //创建新节点,形参根据需要自定义
void addNode(int childsum, Node* parent, ...);  //为父节点添加子节点，形参根据需要自定义
void printTree(Node *root, int depth);  //打印语法树, depth表示当前节点的层数，初始调用时传0

#endif