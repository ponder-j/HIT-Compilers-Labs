#include "intercode.h"
#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 线性三地址码表示 */
// 操作数种类
typedef enum OperandKind_ {
    OP_VARIABLE,   // 源程序变量，统一使用 v_ 前缀
    OP_TEMP,       // 临时变量，统一使用 tN 格式
    OP_CONSTANT,   // 立即数常量
    OP_LABEL,      // 代码标签
    OP_FUNCTION,   // 函数名
    OP_ADDRESS,    // 取地址（&x）
    OP_DEREF       // 解引用（*p）
} OperandKind;

// 操作数结构体
typedef struct Operand_ {
    OperandKind kind;
    int no;                 // 编号（仅对临时变量和标签有效）
    int value;              // 立即数值（仅对常量有效）
    char *name;             // 名字（仅对变量/函数名有效，前缀区分变量/函数）
    struct Operand_ *inner; // 内部操作数（仅对地址和解引用有效）
} Operand;

// 中间代码种类
typedef enum InterCodeKind_ {
    IR_LABEL,
    IR_FUNCTION,
    IR_ASSIGN,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_GOTO,
    IR_IF_GOTO,
    IR_RETURN,
    IR_DEC,
    IR_ARG,
    IR_CALL,
    IR_PARAM,
    IR_READ,
    IR_WRITE
} InterCodeKind;

// 中间代码结构体
typedef struct InterCode_ {
    InterCodeKind kind;
    // 3 个操作数
    Operand *op1;
    Operand *op2;
    Operand *op3;
    // 存关系运算符
    char relop[8];
    // 要申请的内存大小（仅对 DEC 有效）
    int size;
    struct InterCode_ *prev;
    struct InterCode_ *next;
} InterCode;

// 参数列表结构体（Operand 结构体构成的链表）
typedef struct ArgList_ {
    Operand *op;
    struct ArgList_ *next;
} ArgList;

// 地址信息结构体
typedef struct AddrInfo_ {
    Operand *addr;
    Type type;
} AddrInfo;

static InterCode *codeHead = NULL;
static InterCode *codeTail = NULL;
static int tempNo = 1;
static int labelNo = 1;
static int translateFailed = 0;

static char *copyString(const char *s);
static Operand *newVariable(const char *name);
static Operand *newTemp(void);
static Operand *newConstant(int value);
static Operand *newLabel(void);
static Operand *newFunction(const char *name);
static Operand *newAddress(Operand *inner);
static Operand *newDeref(Operand *inner);
static InterCode *newCode(InterCodeKind kind);
static void appendCode(InterCode *code);

static void emitLabel(Operand *label);
static void emitFunction(const char *name);
static void emitAssign(Operand *left, Operand *right);
static void emitBinop(InterCodeKind kind, Operand *result, Operand *op1, Operand *op2);
static void emitGoto(Operand *label);
static void emitIfGoto(Operand *op1, const char *relop, Operand *op2, Operand *label);
static void emitReturn(Operand *op);
static void emitDec(Operand *op, int size);
static void emitArg(Operand *op);
static void emitCall(Operand *result, const char *funcName);
static void emitParam(Operand *op);
static void emitRead(Operand *op);
static void emitWrite(Operand *op);

static void printCodes(FILE *out);
static void printOperand(FILE *out, Operand *op);

static void translateExtDefList(Node *node);
static void translateExtDef(Node *node);
static void translateFunDec(Node *node);
static void translateCompSt(Node *node);
static void translateDefList(Node *node);
static void translateDef(Node *node);
static void translateDecList(Node *node);
static void translateDec(Node *node);
static void translateStmtList(Node *node);
static void translateStmt(Node *node);
static Operand *translateExp(Node *node, Operand *place);
static void translateCond(Node *node, Operand *labelTrue, Operand *labelFalse);
static ArgList *translateArgs(Node *node);
static AddrInfo translateAddr(Node *node);

static int isNode(Node *node, const char *name);
static int isRelopExp(Node *node);
static int isLogicExp(Node *node, const char *opName);
static int isArrayType(Type type);
static int isStructType(Type type);
static int typeSize(Type type);
static int hasUnsupportedType(Type type);
static int hasArrayParam(Node *varDec);
static FieldList lookupField(Type structure, const char *name, int *offset);
static Type expType(Node *node);
static Type varDecType(Node *node);
static const char *varDecName(Node *node);
static void addArgFront(ArgList **head, Operand *op);
static void freeArgList(ArgList *args);
static void failTranslate(const char *message);

/**
 * @brief 将语法树翻译为中间代码并输出到文件。
 *
 * 该函数会重置整套 IR 状态，随后遍历顶层定义；一旦遇到当前实现不支持
 * 的翻译场景，就会设置 translateFailed 并终止输出文件生成。
 */
int translateProgram(Node *root, const char *outputPath) {
    if (root == NULL || outputPath == NULL) return 0;

    /* 清空 IR 链表状态；重置临时变量编号和标签编号 */
    codeHead = NULL;      // IR 双向链表头指针
    codeTail = NULL;      // IR 双向链表尾指针
    tempNo = 1;           // 临时变量编号从 1 开始，0 留给特殊用途
    labelNo = 1;          // 标签编号从 1 开始，0 留给特殊用途
    translateFailed = 0;  // 翻译失败标志

    /* 从语法树根节点开始翻译 */
    translateExtDefList(root->childno > 0 ? root->child[0] : NULL);
    if (translateFailed) return 0;

    FILE *out = fopen(outputPath, "w");
    if (out == NULL) {
        perror(outputPath);
        return 0;
    }

    printCodes(out);
    fclose(out);
    return 1;
}

/**
 * @brief 复制字符串，统一封装堆内存分配。
 */
static char *copyString(const char *s) {
    if (s == NULL) return NULL;
    char *copy = (char *)malloc(strlen(s) + 1);
    strcpy(copy, s);
    return copy;
}

/**
 * @brief 创建一个指定种类的操作数，并完成公共字段初始化。
 */
static Operand *newOperand(OperandKind kind) {
    Operand *op = (Operand *)malloc(sizeof(Operand));
    op->kind = kind;
    op->no = 0;
    op->value = 0;
    op->name = NULL;
    op->inner = NULL;
    return op;
}

/**
 * @brief 创建变量操作数。
 *
 * IR 中所有源程序变量统一加上 v_ 前缀，避免和临时变量命名冲突。
 */
static Operand *newVariable(const char *name) {
    Operand *op = newOperand(OP_VARIABLE);
    if (name == NULL) {
        op->name = copyString("v_");
    } else {
        size_t len = strlen(name) + 3;
        op->name = (char *)malloc(len);
        snprintf(op->name, len, "v_%s", name);
    }
    return op;
}

/**
 * @brief 创建新的临时变量操作数。
 */
static Operand *newTemp(void) {
    Operand *op = newOperand(OP_TEMP);
    op->no = tempNo++;
    return op;
}

/**
 * @brief 创建立即数操作数。
 */
static Operand *newConstant(int value) {
    Operand *op = newOperand(OP_CONSTANT);
    op->value = value;
    return op;
}

/**
 * @brief 创建新的标签操作数。
 */
static Operand *newLabel(void) {
    Operand *op = newOperand(OP_LABEL);
    op->no = labelNo++;
    return op;
}

/**
 * @brief 创建函数名操作数。
 */
static Operand *newFunction(const char *name) {
    Operand *op = newOperand(OP_FUNCTION);
    op->name = copyString(name);
    return op;
}

/**
 * @brief 创建取地址操作数。
 */
static Operand *newAddress(Operand *inner) {
    Operand *op = newOperand(OP_ADDRESS);
    op->inner = inner;
    return op;
}

/**
 * @brief 创建解引用操作数。
 */
static Operand *newDeref(Operand *inner) {
    Operand *op = newOperand(OP_DEREF);
    op->inner = inner;
    return op;
}

/**
 * @brief 创建一条尚未填写操作数的 IR 节点。
 */
static InterCode *newCode(InterCodeKind kind) {
    InterCode *code = (InterCode *)malloc(sizeof(InterCode));
    code->kind = kind;
    code->op1 = NULL;
    code->op2 = NULL;
    code->op3 = NULL;
    code->relop[0] = '\0';
    code->size = 0;
    code->prev = NULL;
    code->next = NULL;
    return code;
}

/**
 * @brief 将一条 IR 追加到全局双向链表末尾。
 */
static void appendCode(InterCode *code) {
    if (code == NULL || translateFailed) return;
    if (codeHead == NULL) {
        codeHead = code;
        codeTail = code;
    } else {
        codeTail->next = code;
        code->prev = codeTail;
        codeTail = code;
    }
}

/**
 * @brief 生成 LABEL 指令。
 */
static void emitLabel(Operand *label) {
    InterCode *code = newCode(IR_LABEL);
    code->op1 = label;
    appendCode(code);
}

/**
 * @brief 生成 FUNCTION 指令。
 */
static void emitFunction(const char *name) {
    InterCode *code = newCode(IR_FUNCTION);
    code->op1 = newFunction(name);
    appendCode(code);
}

/**
 * @brief 生成简单赋值指令。
 */
static void emitAssign(Operand *left, Operand *right) {
    if (left == NULL || right == NULL) return;
    InterCode *code = newCode(IR_ASSIGN);
    code->op1 = left;
    code->op2 = right;
    appendCode(code);
}

/**
 * @brief 生成二元算术指令。
 */
static void emitBinop(InterCodeKind kind, Operand *result, Operand *op1, Operand *op2) {
    if (result == NULL || op1 == NULL || op2 == NULL) return;
    InterCode *code = newCode(kind);
    code->op1 = result;
    code->op2 = op1;
    code->op3 = op2;
    appendCode(code);
}

/**
 * @brief 生成无条件跳转指令。
 */
static void emitGoto(Operand *label) {
    InterCode *code = newCode(IR_GOTO);
    code->op1 = label;
    appendCode(code);
}

/**
 * @brief 生成条件跳转指令。
 */
static void emitIfGoto(Operand *op1, const char *relop, Operand *op2, Operand *label) {
    if (op1 == NULL || op2 == NULL || label == NULL) return;
    InterCode *code = newCode(IR_IF_GOTO);
    code->op1 = op1;
    code->op2 = op2;
    code->op3 = label;
    strncpy(code->relop, relop, sizeof(code->relop) - 1);
    code->relop[sizeof(code->relop) - 1] = '\0';
    appendCode(code);
}

/**
 * @brief 生成 RETURN 指令。
 */
static void emitReturn(Operand *op) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_RETURN);
    code->op1 = op;
    appendCode(code);
}

/**
 * @brief 生成 DEC 指令，为数组等对象申请连续空间。
 */
static void emitDec(Operand *op, int size) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_DEC);
    code->op1 = op;
    code->size = size;
    appendCode(code);
}

/**
 * @brief 生成 ARG 指令。
 */
static void emitArg(Operand *op) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_ARG);
    code->op1 = op;
    appendCode(code);
}

/**
 * @brief 生成 CALL 指令。
 */
static void emitCall(Operand *result, const char *funcName) {
    if (result == NULL) return;
    InterCode *code = newCode(IR_CALL);
    code->op1 = result;
    code->op2 = newFunction(funcName);
    appendCode(code);
}

/**
 * @brief 生成 PARAM 指令。
 */
static void emitParam(Operand *op) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_PARAM);
    code->op1 = op;
    appendCode(code);
}

/**
 * @brief 生成 READ 指令。
 */
static void emitRead(Operand *op) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_READ);
    code->op1 = op;
    appendCode(code);
}

/**
 * @brief 生成 WRITE 指令。
 */
static void emitWrite(Operand *op) {
    if (op == NULL) return;
    InterCode *code = newCode(IR_WRITE);
    code->op1 = op;
    appendCode(code);
}

/**
 * @brief 按 IR 文本格式打印单个操作数。
 */
static void printOperand(FILE *out, Operand *op) {
    if (op == NULL) return;

    switch (op->kind) {
        case OP_VARIABLE:
            fprintf(out, "%s", op->name);
            break;
        case OP_TEMP:
            fprintf(out, "t%d", op->no);
            break;
        case OP_CONSTANT:
            fprintf(out, "#%d", op->value);
            break;
        case OP_LABEL:
            fprintf(out, "label%d", op->no);
            break;
        case OP_FUNCTION:
            fprintf(out, "%s", op->name);
            break;
        case OP_ADDRESS:
            fprintf(out, "&");
            printOperand(out, op->inner);
            break;
        case OP_DEREF:
            fprintf(out, "*");
            printOperand(out, op->inner);
            break;
    }
}

/**
 * @brief 顺序打印整条 IR 链表。
 */
static void printCodes(FILE *out) {
    for (InterCode *code = codeHead; code != NULL; code = code->next) {
        switch (code->kind) {
            case IR_LABEL:
                fprintf(out, "LABEL ");
                printOperand(out, code->op1);
                fprintf(out, " :\n");
                break;
            case IR_FUNCTION:
                fprintf(out, "FUNCTION ");
                printOperand(out, code->op1);
                fprintf(out, " :\n");
                break;
            case IR_ASSIGN:
                printOperand(out, code->op1);
                fprintf(out, " := ");
                printOperand(out, code->op2);
                fprintf(out, "\n");
                break;
            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV:
                printOperand(out, code->op1);
                fprintf(out, " := ");
                printOperand(out, code->op2);
                fprintf(out, " %c ", code->kind == IR_ADD ? '+' :
                                      code->kind == IR_SUB ? '-' :
                                      code->kind == IR_MUL ? '*' : '/');
                printOperand(out, code->op3);
                fprintf(out, "\n");
                break;
            case IR_GOTO:
                fprintf(out, "GOTO ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
            case IR_IF_GOTO:
                fprintf(out, "IF ");
                printOperand(out, code->op1);
                fprintf(out, " %s ", code->relop);
                printOperand(out, code->op2);
                fprintf(out, " GOTO ");
                printOperand(out, code->op3);
                fprintf(out, "\n");
                break;
            case IR_RETURN:
                fprintf(out, "RETURN ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
            case IR_DEC:
                fprintf(out, "DEC ");
                printOperand(out, code->op1);
                fprintf(out, " %d\n", code->size);
                break;
            case IR_ARG:
                fprintf(out, "ARG ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
            case IR_CALL:
                printOperand(out, code->op1);
                fprintf(out, " := CALL ");
                printOperand(out, code->op2);
                fprintf(out, "\n");
                break;
            case IR_PARAM:
                fprintf(out, "PARAM ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
            case IR_READ:
                fprintf(out, "READ ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
            case IR_WRITE:
                fprintf(out, "WRITE ");
                printOperand(out, code->op1);
                fprintf(out, "\n");
                break;
        }
    }
}

/**
 * @brief 处理顶层定义列表，只翻译函数定义。
 */
static void translateExtDefList(Node *node) {
    while (node != NULL && node->childno > 0 && !translateFailed) {
        translateExtDef(node->child[0]);
        node = node->childno > 1 ? node->child[1] : NULL;
    }
}

/**
 * @brief 处理单个顶层定义。
 */
static void translateExtDef(Node *node) {
    if (node == NULL || node->childno < 2 || translateFailed) return;
    if (isNode(node->child[1], "FunDec")) {
        translateFunDec(node->child[1]);
        if (node->childno > 2) {
            translateCompSt(node->child[2]);
        }
    }
}

/**
 * @brief 翻译函数头部并输出 FUNCTION/PARAM 指令。
 */
static void translateFunDec(Node *node) {
    if (node == NULL || node->childno == 0) return;

    char *funcName = node->child[0]->yytext;
    emitFunction(funcName);

    if (node->childno == 4) {
        Node *varList = node->child[2];
        while (varList != NULL && varList->childno > 0) {
            Node *paramDec = varList->child[0];
            Node *varDec = paramDec->child[1];
            Type paramType = varDecType(varDec);

            if (isStructType(paramType)) {
                failTranslate("Cannot translate: Code contains variables or parameters of structure type.");
                return;
            }
            if (hasArrayParam(varDec)) {
                failTranslate("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
                return;
            }

            emitParam(newVariable(varDecName(varDec))); // PARAM 顺序与源程序形参顺序保持一致
            varList = varList->childno == 3 ? varList->child[2] : NULL;
        }
    }
}

/**
 * @brief 翻译复合语句块中的定义和语句。
 */
static void translateCompSt(Node *node) {
    if (node == NULL || translateFailed) return;

    for (int i = 0; i < node->childno; i++) {
        if (isNode(node->child[i], "DefList")) {
            translateDefList(node->child[i]);
        } else if (isNode(node->child[i], "StmtList")) {
            translateStmtList(node->child[i]);
        }
    }
}

/**
 * @brief 翻译局部定义列表。
 */
static void translateDefList(Node *node) {
    while (node != NULL && node->childno > 0 && !translateFailed) {
        translateDef(node->child[0]);
        node = node->childno > 1 ? node->child[1] : NULL;
    }
}

/**
 * @brief 翻译单个局部定义。
 */
static void translateDef(Node *node) {
    if (node == NULL || node->childno < 2) return;
    translateDecList(node->child[1]);
}

/**
 * @brief 翻译逗号连接的声明列表。
 */
static void translateDecList(Node *node) {
    while (node != NULL && node->childno > 0 && !translateFailed) {
        translateDec(node->child[0]);
        node = node->childno > 1 ? node->child[2] : NULL;
    }
}

/**
 * @brief 翻译单个声明。
 *
 * 普通变量只在有初始化时输出赋值；数组变量会先输出 DEC 再处理初始化。
 */
static void translateDec(Node *node) {
    if (node == NULL || node->childno == 0 || translateFailed) return;

    Node *varDec = node->child[0];
    Type type = varDecType(varDec);
    const char *name = varDecName(varDec);

    if (hasUnsupportedType(type)) return;

    // 处理数组类型：当前实现只支持局部一维数组，目前暂不支持高维数组；为该数组输出 DEC 空间申请指令
    if (isArrayType(type)) {
        Type elem = type;
        while (elem != NULL && elem->kind == ARRAY) {
            if (elem->u.array.elem != NULL && elem->u.array.elem->kind == ARRAY) {
                failTranslate("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
                return;
            }
            elem = elem->u.array.elem;
        }
        emitDec(newVariable(name), typeSize(type)); // 一维数组按字节数一次性申请连续空间
    }

    // 处理普通变量：如果有初始化表达式，则翻译该表达式并输出赋值指令
    if (node->childno == 3) {
        Operand *value = translateExp(node->child[2], newTemp());
        emitAssign(newVariable(name), value);
    }
}

/**
 * @brief 翻译语句列表。
 */
static void translateStmtList(Node *node) {
    while (node != NULL && node->childno > 0 && !translateFailed) {
        translateStmt(node->child[0]);
        node = node->childno > 1 ? node->child[1] : NULL;
    }
}

/**
 * @brief 翻译单条语句。
 */
static void translateStmt(Node *node) {
    if (node == NULL || node->childno == 0 || translateFailed) return;

    if (isNode(node->child[0], "Exp")) {
        translateExp(node->child[0], NULL);
        return;
    }

    if (isNode(node->child[0], "CompSt")) {
        translateCompSt(node->child[0]);
        return;
    }

    // 处理返回语句：翻译返回值表达式，生成 RETURN 指令。
    if (isNode(node->child[0], "RETURN")) {
        Operand *value = translateExp(node->child[1], newTemp());
        emitReturn(value);
        return;
    }

    // 处理条件语句：翻译条件表达式，生成基于标签的跳转序列；翻译 then/else 体，并在必要时生成额外跳转指令确保控制流正确。
    if (isNode(node->child[0], "IF")) {
        Operand *label1 = newLabel();
        Operand *label2 = newLabel();
        translateCond(node->child[2], label1, label2);
        emitLabel(label1);
        translateStmt(node->child[4]);
        if (node->childno == 7) {
            /* 有 else 分支时，需要额外标签跳过 else 体。 */
            Operand *label3 = newLabel();
            emitGoto(label3);
            emitLabel(label2);
            translateStmt(node->child[6]);
            emitLabel(label3);
        } else {
            emitLabel(label2);
        }
        return;
    }

    // 处理循环语句：翻译循环条件，生成基于标签的跳转序列；翻译循环体，并生成必要的跳转指令实现循环控制流。
    if (isNode(node->child[0], "WHILE")) {
        Operand *label1 = newLabel();
        Operand *label2 = newLabel();
        Operand *label3 = newLabel();
        emitLabel(label1); // label1 是循环入口，label3 是退出位置
        translateCond(node->child[2], label2, label3);
        emitLabel(label2);
        translateStmt(node->child[4]);
        emitGoto(label1);
        emitLabel(label3);
    }
}

/**
 * @brief 翻译表达式。
 *
 * @param node 当前表达式节点。
 * @param place 若非空，表示表达式结果应存入该操作数；否则返回一个可直接使用的操作数。
 */
static Operand *translateExp(Node *node, Operand *place) {
    if (node == NULL || node->childno == 0 || translateFailed) return place;

    // 处理基本表达式：整数常量和变量
    if (node->childno == 1) {
        if (isNode(node->child[0], "INT")) {
            Operand *c = newConstant((int)strtol(node->child[0]->yytext, NULL, 0));
            // 生成赋值指令将常量存入 place 或 直接返回常量操作数（place 为空）
            if (place != NULL) {
                emitAssign(place, c);
                return place;
            }
            return c;
        }
        if (isNode(node->child[0], "ID")) {
            Operand *var = newVariable(node->child[0]->yytext);
            if (place != NULL) {
                emitAssign(place, var);
                return place;
            }
            return var;
        }
    }

    // 处理一元表达式：负号和逻辑非
    if (node->childno == 2 && isNode(node->child[0], "MINUS")) {
        Operand *result = place != NULL ? place : newTemp();
        Operand *t1 = translateExp(node->child[1], newTemp());
        emitBinop(IR_SUB, result, newConstant(0), t1);
        return result;
    }

    // 处理条件表达式：关系运算、逻辑运算和非运算。翻译为基于标签的跳转序列，并在必要时将结果存入 place。
    if ((node->childno == 2 && isNode(node->child[0], "NOT")) || isRelopExp(node) ||
        isLogicExp(node, "AND") || isLogicExp(node, "OR")) {
        Operand *result = place != NULL ? place : newTemp();
        Operand *label1 = newLabel();
        Operand *label2 = newLabel();
        emitAssign(result, newConstant(0));  // 默认将真假表达式结果初始化为 0
        translateCond(node, label1, label2); // 调用 translateCond 翻译条件表达式，生成跳转指令
        emitLabel(label1);
        emitAssign(result, newConstant(1));  // 条件成立时再覆盖为 1
        emitLabel(label2);
        return result;
    }

    // 处理括号表达式：直接翻译子表达式，结果存入 place 或返回子表达式结果。
    if (node->childno == 3 && isNode(node->child[0], "LP")) {
        return translateExp(node->child[1], place);
    }

    // 处理赋值表达式
    if (node->childno == 3 && isNode(node->child[1], "ASSIGNOP")) {
        Operand *right = translateExp(node->child[2], newTemp());
        Type leftType = expType(node->child[0]);
        if (hasUnsupportedType(leftType)) return place;

        // 如果赋值左值是简单变量，直接生成赋值指令
        if (node->child[0]->childno == 1 && isNode(node->child[0]->child[0], "ID")) {
            Operand *left = newVariable(node->child[0]->child[0]->yytext);
            emitAssign(left, right);
            if (place != NULL) emitAssign(place, left);
            return place != NULL ? place : left;
        }

        // 否则需要先计算左值地址，再生成间接赋值指令
        AddrInfo addr = translateAddr(node->child[0]);
        emitAssign(newDeref(addr.addr), right);
        if (place != NULL) emitAssign(place, right);
        return place != NULL ? place : right;
    }

    // 处理二元算术表达式：翻译左右子表达式，生成对应的算术指令，结果存入 place 或返回新临时变量。
    if (node->childno == 3 &&
        (isNode(node->child[1], "PLUS") || isNode(node->child[1], "MINUS") ||
         isNode(node->child[1], "STAR") || isNode(node->child[1], "DIV"))) {
        Operand *result = place != NULL ? place : newTemp();
        Operand *t1 = translateExp(node->child[0], newTemp());
        Operand *t2 = translateExp(node->child[2], newTemp());
        if (isNode(node->child[1], "PLUS")) {
            emitBinop(IR_ADD, result, t1, t2);
        } else if (isNode(node->child[1], "MINUS")) {
            emitBinop(IR_SUB, result, t1, t2);
        } else if (isNode(node->child[1], "STAR")) {
            emitBinop(IR_MUL, result, t1, t2);
        } else {
            emitBinop(IR_DIV, result, t1, t2);
        }
        return result;
    }

    // 处理数组元素访问：翻译数组基地址和索引表达式，计算元素地址，生成间接赋值指令。
    if (node->childno == 4 && isNode(node->child[1], "LB")) {
        Type t = expType(node);
        if (hasUnsupportedType(t)) return place;
        AddrInfo addr = translateAddr(node);
        Operand *result = place != NULL ? place : newTemp();
        emitAssign(result, newDeref(addr.addr));
        return result;
    }

    // 处理结构体字段访问：先计算地址，再生成解引用指令，结果存入 place 或返回新临时变量。（暂未支持结构体翻译）
    if (node->childno == 3 && isNode(node->child[1], "DOT")) {
        Type t = expType(node);
        if (hasUnsupportedType(t)) return place;
        AddrInfo addr = translateAddr(node);
        Operand *result = place != NULL ? place : newTemp();
        emitAssign(result, newDeref(addr.addr));
        return result;
    }

    // 处理函数调用：翻译实参列表，生成 ARG 指令；生成 CALL 指令，结果存入 place 或返回新临时变量。
    if (isNode(node->child[0], "ID") && node->childno >= 3) {
        char *funcName = node->child[0]->yytext;
        Operand *result = place != NULL ? place : newTemp();

        // read 和 write 需要特殊处理：直接生成 READ/WRITE 指令，无需 CALL
        if (strcmp(funcName, "read") == 0 && node->childno == 3) {
            emitRead(result);
            return result;
        }

        if (strcmp(funcName, "write") == 0 && node->childno == 4) {
            ArgList *args = translateArgs(node->child[2]);
            if (args != NULL) emitWrite(args->op);
            if (place != NULL) emitAssign(result, newConstant(0)); // write 按实验约定固定返回 0
            freeArgList(args);
            return result;
        }

        ArgList *args = NULL;
        if (node->childno == 4) {
            args = translateArgs(node->child[2]);
        }
        for (ArgList *p = args; p != NULL; p = p->next) {
            /* translateArgs 已逆序收集参数，这里顺序输出即可得到 ARG 的反序传参。 */
            emitArg(p->op);
        }
        emitCall(result, funcName);
        freeArgList(args);
        return result;
    }

    return place;
}

/**
 * @brief 将条件表达式翻译为基于标签的跳转序列。
 */
static void translateCond(Node *node, Operand *labelTrue, Operand *labelFalse) {
    if (node == NULL || translateFailed) return;

    if (isRelopExp(node)) {
        Operand *t1 = translateExp(node->child[0], newTemp());
        Operand *t2 = translateExp(node->child[2], newTemp());
        emitIfGoto(t1, node->child[1]->yytext, t2, labelTrue);
        emitGoto(labelFalse);
        return;
    }

    if (node->childno == 2 && isNode(node->child[0], "NOT")) {
        translateCond(node->child[1], labelFalse, labelTrue);
        return;
    }

    if (isLogicExp(node, "AND")) {
        Operand *label1 = newLabel();
        translateCond(node->child[0], label1, labelFalse); // 左边为假时直接短路到 false
        emitLabel(label1);
        translateCond(node->child[2], labelTrue, labelFalse);
        return;
    }

    if (isLogicExp(node, "OR")) {
        Operand *label1 = newLabel();
        translateCond(node->child[0], labelTrue, label1); // 左边为真时直接短路到 true
        emitLabel(label1);
        translateCond(node->child[2], labelTrue, labelFalse);
        return;
    }

    Operand *t1 = translateExp(node, newTemp());
    emitIfGoto(t1, "!=", newConstant(0), labelTrue);
    emitGoto(labelFalse);
}

/**
 * @brief 翻译实参列表。
 *
 * 返回链表中的顺序已经调整为 ARG 输出顺序，可直接线性遍历生成指令。
 */
static ArgList *translateArgs(Node *node) {
    ArgList *args = NULL;

    while (node != NULL && node->childno > 0 && !translateFailed) {
        Type argType = expType(node->child[0]);
        Operand *arg = NULL;

        if (isStructType(argType)) {
            failTranslate("Cannot translate: Code contains variables or parameters of structure type.");
            return args;
        }

        if (isArrayType(argType)) {
            /* 数组实参按地址传递。 */
            AddrInfo addr = translateAddr(node->child[0]);
            arg = addr.addr;
        } else {
            arg = translateExp(node->child[0], newTemp());
        }

        addArgFront(&args, arg); // 头插可自然得到 ARG 所需的逆序
        node = node->childno == 3 ? node->child[2] : NULL;
    }

    return args;
}

/**
 * @brief 计算左值表达式的地址。
 *
 * 当前实现覆盖变量、一维数组元素和结构体字段三类可寻址对象。
 */
static AddrInfo translateAddr(Node *node) {
    AddrInfo result;
    result.addr = NULL;
    result.type = NULL;

    if (node == NULL || translateFailed) return result;

    if (node->childno == 1 && isNode(node->child[0], "ID")) {
        FieldList field = search(node->child[0]->yytext);
        if (field == NULL) return result;
        result.type = field->type;
        result.addr = newAddress(newVariable(node->child[0]->yytext));
        return result;
    }

    if (node->childno == 4 && isNode(node->child[1], "LB")) {
        AddrInfo base = translateAddr(node->child[0]);
        Type baseType = expType(node->child[0]);
        if (!isArrayType(baseType)) return result;

        Type elemType = baseType->u.array.elem;
        if (isStructType(elemType) || (elemType != NULL && elemType->kind == ARRAY)) {
            failTranslate("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
            return result;
        }

        Operand *index = translateExp(node->child[2], newTemp());
        Operand *offset = newTemp();
        emitBinop(IR_MUL, offset, index, newConstant(typeSize(elemType))); // 偏移 = 下标 * 元素宽度
        Operand *addr = newTemp();
        emitBinop(IR_ADD, addr, base.addr, offset); // 元素地址 = 基地址 + 偏移

        result.addr = addr;
        result.type = elemType;
        return result;
    }

    if (node->childno == 3 && isNode(node->child[1], "DOT")) {
        AddrInfo base = translateAddr(node->child[0]);
        Type structure = expType(node->child[0]);
        int offset = 0;
        FieldList field = lookupField(structure, node->child[2]->yytext, &offset);
        if (field == NULL) return result;

        if (isStructType(field->type)) {
            failTranslate("Cannot translate: Code contains variables or parameters of structure type.");
            return result;
        }

        if (offset == 0) {
            result.addr = base.addr;
        } else {
            result.addr = newTemp();
            emitBinop(IR_ADD, result.addr, base.addr, newConstant(offset));
        }
        result.type = field->type;
        return result;
    }

    return result;
}

/**
 * @brief 判断节点名字是否与目标语法符号一致。
 */
static int isNode(Node *node, const char *name) {
    return node != NULL && strcmp(node->name, name) == 0;
}

/**
 * @brief 判断表达式是否是关系运算表达式。
 */
static int isRelopExp(Node *node) {
    return node != NULL && node->childno == 3 && isNode(node->child[1], "RELOP");
}

/**
 * @brief 判断表达式是否是指定逻辑运算符的二元表达式。
 */
static int isLogicExp(Node *node, const char *opName) {
    return node != NULL && node->childno == 3 && isNode(node->child[1], opName);
}

/**
 * @brief 判断类型是否为数组。
 */
static int isArrayType(Type type) {
    return type != NULL && type->kind == ARRAY;
}

/**
 * @brief 判断类型是否为结构体。
 */
static int isStructType(Type type) {
    return type != NULL && type->kind == STRUCTURE;
}

/**
 * @brief 计算类型占用的字节数。
 */
static int typeSize(Type type) {
    if (type == NULL) return 4;
    if (type->kind == BASIC) return 4;
    if (type->kind == ARRAY) return type->u.array.size * typeSize(type->u.array.elem);
    if (type->kind == STRUCTURE) {
        int size = 0;
        for (FieldList p = type->u.structure; p != NULL; p = p->tail) {
            size += typeSize(p->type);
        }
        return size;
    }
    return 4;
}

/**
 * @brief 判断类型是否超出当前翻译器的支持范围。
 *
 * 若遇到结构体、高维数组或数组元素为结构体等情况，会立即设置翻译失败。
 */
static int hasUnsupportedType(Type type) {
    if (isStructType(type)) {
        failTranslate("Cannot translate: Code contains variables or parameters of structure type.");
        return 1;
    }
    if (type != NULL && type->kind == ARRAY && isStructType(type->u.array.elem)) {
        failTranslate("Cannot translate: Code contains variables or parameters of structure type.");
        return 1;
    }
    if (type != NULL && type->kind == ARRAY && type->u.array.elem != NULL &&
        type->u.array.elem->kind == ARRAY) {
        failTranslate("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
        return 1;
    }
    return 0;
}

/**
 * @brief 判断形参声明是否是数组参数。
 */
static int hasArrayParam(Node *varDec) {
    return varDec != NULL && varDec->childno == 4 && isNode(varDec->child[1], "LB");
}

/**
 * @brief 在结构体字段链表中查找目标字段，并计算其相对偏移。
 */
static FieldList lookupField(Type structure, const char *name, int *offset) {
    if (offset != NULL) *offset = 0;
    if (structure == NULL || structure->kind != STRUCTURE) return NULL;

    int current = 0;
    for (FieldList p = structure->u.structure; p != NULL; p = p->tail) {
        if (p->name != NULL && strcmp(p->name, name) == 0) {
            if (offset != NULL) *offset = current;
            return p;
        }
        current += typeSize(p->type);
    }

    return NULL;
}

/**
 * @brief 推导表达式的类型，仅供 IR 翻译辅助使用。
 *
 * 这里不负责报错，相关合法性已在语义分析阶段保证。
 */
static Type expType(Node *node) {
    if (node == NULL || node->childno == 0) return NULL;

    if (node->childno == 1) {
        if (isNode(node->child[0], "INT")) return NULL;
        if (isNode(node->child[0], "ID")) {
            FieldList f = search(node->child[0]->yytext);
            return f != NULL ? f->type : NULL;
        }
    }

    if (node->childno == 2) {
        return expType(node->child[1]);
    }

    if (node->childno == 3 && isNode(node->child[0], "LP")) {
        return expType(node->child[1]);
    }

    if (node->childno == 3 && isNode(node->child[1], "ASSIGNOP")) {
        return expType(node->child[0]);
    }

    if (node->childno == 4 && isNode(node->child[1], "LB")) {
        Type arrayType = expType(node->child[0]);
        if (arrayType != NULL && arrayType->kind == ARRAY) {
            return arrayType->u.array.elem;
        }
        return NULL;
    }

    if (node->childno == 3 && isNode(node->child[1], "DOT")) {
        Type structure = expType(node->child[0]);
        FieldList field = lookupField(structure, node->child[2]->yytext, NULL);
        return field != NULL ? field->type : NULL;
    }

    if (isNode(node->child[0], "ID") && node->childno >= 3) {
        FieldList f = search(node->child[0]->yytext);
        if (f != NULL && f->type != NULL && f->type->kind == FUNCTION) {
            return f->type->u.function.funcType;
        }
        return NULL;
    }

    return NULL;
}

/**
 * @brief 根据 VarDec 节点查询其最终类型。
 */
static Type varDecType(Node *node) {
    const char *name = varDecName(node);
    FieldList field = search((char *)name);
    return field != NULL ? field->type : NULL;
}

/**
 * @brief 递归取得 VarDec 最底层的变量名。
 */
static const char *varDecName(Node *node) {
    if (node == NULL || node->childno == 0) return "";
    if (node->childno == 1 && isNode(node->child[0], "ID")) {
        return node->child[0]->yytext;
    }
    return varDecName(node->child[0]);
}

/**
 * @brief 将实参加入链表头部，用于构造 ARG 的逆序输出。
 */
static void addArgFront(ArgList **head, Operand *op) {
    ArgList *arg = (ArgList *)malloc(sizeof(ArgList));
    arg->op = op;
    arg->next = *head;
    *head = arg;
}

/**
 * @brief 释放实参链表节点。
 */
static void freeArgList(ArgList *args) {
    while (args != NULL) {
        ArgList *next = args->next;
        free(args);
        args = next;
    }
}

/**
 * @brief 记录翻译失败状态，并只输出一次失败原因。
 */
static void failTranslate(const char *message) {
    if (!translateFailed) {
        printf("%s\n", message);
    }
    translateFailed = 1;
}
