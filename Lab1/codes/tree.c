#include "tree.h"

// 删除节点，递归释放内存
void delNode(Node* node){
    if(node == NULL) return;
    // 遍历子节点，递归释放内存
    for(int i = 0; i < node->childno; i++){
        delNode(node->child[i]);
    }
    free(node);
}

// 创建新节点
Node* createNode(char* name, char* text, int lineno, int isToken){
    Node* node = (Node*)malloc(sizeof(Node));
    strncpy(node->name, name, 31);
    if (text != NULL) {
        strncpy(node->yytext, text, 31);
    } else {
        node->yytext[0] = '\0';
    }
    node->lineno = lineno;
    node->isToken = isToken;

    // 初始化子节点数和子节点指针
    node->childno = 0;
    for(int i = 0; i < 10; i++) {
        node->child[i] = NULL;
    }
    return node;
}

// 为父节点添加子节点
void addNode(int childsum, Node* parent, ...){
    // 使用可变参数列表来添加子节点
    va_list valist;
    // va_start(游标, 最后一个固定参数)
    // 传入 parent 意味着 parent 是最后一个固定参数
    // 后面都是可变参数（子节点）
    va_start(valist, parent);

    int valid_child_count = 0; // 记录有效子节点的数量
    
    for(int i = 0; i < childsum; i++){
        // va_arg(游标, 数据类型)
        // 按照指定的数据类型，从游标当前的位置拿出一个参数，并且把游标自动挪到下一个参数的位置
        Node* child = va_arg(valist, Node*);
        if (child != NULL) {
            parent->child[valid_child_count] = child;
            if (valid_child_count == 0) {
                // 父节点的行号取第一个子节点的行号，这样做可以在语法分析阶段省力
                parent->lineno = child->lineno;
            }
            valid_child_count++;
        }
    }
    parent->childno = valid_child_count;
    // 结束可变参数的获取
    va_end(valist);
}

// 打印语法树
void printTree(Node *root, int depth){
    if(root == NULL) return;

    // 如果是非终结符且没有子节点（即产生了 ε 空串），则不打印
    if (root->isToken == 0 && root->childno == 0) return;

    // 先输出 depth*2 个空格，表示当前节点的层数
    for(int i = 0; i < depth; i++) printf("  ");

    // 打印终结符和非终结符信息
    if (root->isToken) {
        // 终结符打印格式
        if (strcmp(root->name, "ID") == 0 || strcmp(root->name, "TYPE") == 0) {
            printf("%s: %s\n", root->name, root->yytext);
        } else if (strcmp(root->name, "INT") == 0) {
            // 使用 strtol 自动将 10进制、8进制(0开头)、16进制(0x开头) 转换为十进制数值
            // strtol(字符串, 结束位置指针, 数字基数)，当数字基数为0时，函数会根据字符串的格式自动判断是10进制、8进制还是16进制
            printf("%s: %ld\n", root->name, strtol(root->yytext, NULL, 0));
        } else if (strcmp(root->name, "FLOAT") == 0) {
            printf("%s: %f\n", root->name, atof(root->yytext));
        } else {
            // 其他终结符直接打印名字
            printf("%s\n", root->name);
        }
    } else {
        // 非终结符打印名字和行号
        printf("%s (%d)\n", root->name, root->lineno);
    }

    // 递归打印子节点，层数加1
    for(int i = 0; i < root->childno; i++){
        printTree(root->child[i], depth + 1);
    }
}