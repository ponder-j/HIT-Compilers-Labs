#ifndef INTERCODE_H
#define INTERCODE_H

#include "tree.h"

/**
 * @brief 将语法树翻译为 Lab3 要求的线性中间代码并写入文件。
 *
 * 调用方应保证词法、语法和语义分析都已通过；若翻译过程中遇到
 * 当前实现不支持的结构体、高维数组或数组参数，将返回失败。
 *
 * @param root 语法树根节点。
 * @param outputPath 输出 IR 文件路径。
 * @return int 1 表示成功生成并写出 IR，0 表示失败。
 */
int translateProgram(Node *root, const char *outputPath);

#endif
