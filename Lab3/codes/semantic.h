#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

#define HASH_SIZE 0x3fff

#define BASIC_INT 0
#define BASIC_FLOAT 1

/* 语义分析阶段对类型和符号表项的核心抽象。 */
typedef struct Type_ *Type;
typedef struct FieldList_ *FieldList;

typedef enum SymbolKind_ {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCTURE,
    SYMBOL_FIELD
} SymbolKind;

typedef struct Type_ {
    enum { BASIC, ARRAY, STRUCTURE, FUNCTION } kind;
    char *structName;
    union {
        int basic;
        struct {
            Type elem;
            int size;
        } array;
        FieldList structure;
        struct {
            FieldList params;
            Type funcType;
            int paramNum;
        } function;
    } u;
} Type_;

typedef struct FieldList_ {
    char *name;
    Type type;
    FieldList tail;
    FieldList hashNext;
    SymbolKind symbolKind;
    int lineno;
} FieldList_;

/**
 * @brief PJW 哈希函数，用于将符号名映射到哈希桶。
 */
unsigned int hash_pjw(char *name);

/**
 * @brief 初始化符号表哈希桶。
 */
void initHashtable(void);

/**
 * @brief 向符号表插入一个新符号。
 *
 * @return int 1 表示插入成功，0 表示已有重名符号或参数非法。
 */
int insert(FieldList f);

/**
 * @brief 在符号表中查找名字对应的符号项。
 */
FieldList search(char *name);

/**
 * @brief 判断两个类型在语义上是否等价。
 */
int TypeEqual(Type type1, Type type2);

/**
 * @brief 语义分析入口，对整棵语法树执行自顶向下的检查。
 */
void Program(Node *root);

/**
 * @brief 在符号表中预置 Lab3 的 read/write 内建函数。
 */
void addPredefinedFunctions(void);
void ExtDefList(Node *node);
void ExtDef(Node *node);

Type Specifier(Node *node);
void ExtDecList(Node *node, Type spec);
void FunDec(Node *node, Type spec);
void CompSt(Node *node, Type ftype);
FieldList VarDec(Node *node, Type spec);
Type StructSpecifier(Node *node);

FieldList DefList(Node *node, int isStruct);
FieldList Def(Node *node, int isStruct);
FieldList DecList(Node *node, Type spec, int isStruct);
FieldList Dec(Node *node, Type spec, int isStruct);

void Stmt(Node *node, Type ftype);
Type Exp(Node *root);

#endif
