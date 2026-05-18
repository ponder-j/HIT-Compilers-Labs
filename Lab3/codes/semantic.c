#include "semantic.h"

extern int error;

/* Lab2/Lab3 共用的全局符号表，采用哈希桶加链地址法组织。 */
static FieldList hashTable[HASH_SIZE];

static char *copyString(const char *s);
static Type newBasicType(int basic);
static Type newArrayType(Type elem, int size);
static Type newFunctionType(Type retType);
static FieldList newField(const char *name, Type type, SymbolKind kind, int lineno);
static void appendField(FieldList *head, FieldList *tail, FieldList field);
static FieldList findInList(FieldList list, const char *name);
static Type actualType(Type type);
static int isIntType(Type type);
static int isNumericType(Type type);
static int isLValue(Node *node);
static int isFunctionSymbol(FieldList field);
static int isStructureSymbol(FieldList field);
static FieldList collectArgs(Node *node, int *count);
static int argsMatch(FieldList args, FieldList params);
static int hasUnknownType(FieldList list);
static Type findFieldType(Type structure, const char *fieldName);

/**
 * @brief 计算符号名的哈希值。
 *
 * 这里使用 PJW 哈希函数，使标识符能较均匀地落入固定大小的哈希桶。
 */
unsigned int hash_pjw(char *name) {
    unsigned int val = 0, i;
    for (; *name; ++name) {
        val = (val << 2) + (unsigned char)(*name);
        if ((i = val & ~0x3fff)) {
            val = (val ^ (i >> 12)) & 0x3fff;
        }
    }
    return val % HASH_SIZE;
}

/**
 * @brief 清空符号表。
 */
void initHashtable(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hashTable[i] = NULL;
    }
}

/**
 * @brief 在符号表中查找指定名字的符号项。
 */
FieldList search(char *name) {
    if (name == NULL) return NULL;
    unsigned int index = hash_pjw(name);
    FieldList p = hashTable[index];
    while (p != NULL) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->hashNext;
    }
    return NULL;
}

/**
 * @brief 向符号表插入一个符号项。
 *
 * 符号表不允许重名；若插入失败，由调用方决定输出何种错误信息。
 */
int insert(FieldList f) {
    if (f == NULL || f->name == NULL) return 0;
    if (search(f->name) != NULL) return 0;

    unsigned int index = hash_pjw(f->name);
    f->hashNext = hashTable[index];
    hashTable[index] = f;
    return 1;
}

/**
 * @brief 向符号表预置 read/write 两个内建函数。
 *
 * IR 翻译会将它们分别映射为 READ 和 WRITE 指令，因此必须在
 * 语义检查前先将其作为普通函数加入符号表。
 */
void addPredefinedFunctions(void) {
    Type intType = newBasicType(BASIC_INT);

    Type readType = newFunctionType(intType);
    FieldList readField = newField("read", readType, SYMBOL_FUNCTION, 0);
    insert(readField);

    Type writeType = newFunctionType(newBasicType(BASIC_INT));
    FieldList param = newField("x", newBasicType(BASIC_INT), SYMBOL_FIELD, 0);
    writeType->u.function.params = param;
    writeType->u.function.paramNum = 1;
    FieldList writeField = newField("write", writeType, SYMBOL_FUNCTION, 0);
    insert(writeField);
}

/**
 * @brief 判断两个类型是否在语义上等价。
 *
 * 数组比较元素类型，结构体优先比较结构体名，函数比较返回值和形参列表。
 */
int TypeEqual(Type t1, Type t2) {
    t1 = actualType(t1);
    t2 = actualType(t2);

    if (t1 == NULL && t2 == NULL) return 1;
    if (t1 == NULL || t2 == NULL) return 0;
    if (t1->kind != t2->kind) return 0;

    if (t1->kind == BASIC) {
        return t1->u.basic == t2->u.basic;
    }

    if (t1->kind == ARRAY) {
        return TypeEqual(t1->u.array.elem, t2->u.array.elem);
    }

    if (t1->kind == STRUCTURE) {
        if (t1->structName == NULL || t2->structName == NULL) {
            return t1 == t2;
        }
        return strcmp(t1->structName, t2->structName) == 0;
    }

    if (t1->kind == FUNCTION) {
        return TypeEqual(t1->u.function.funcType, t2->u.function.funcType) &&
               argsMatch(t1->u.function.params, t2->u.function.params);
    }

    return 0;
}

/**
 * @brief 语义分析入口。
 */
void Program(Node *root) {
    if (root == NULL || root->childno == 0) return;
    ExtDefList(root->child[0]);
}

/**
 * @brief 递归处理顶层定义列表。
 */
void ExtDefList(Node *node) {
    if (node == NULL || node->childno == 0) return;
    ExtDef(node->child[0]);
    if (node->childno > 1) {
        ExtDefList(node->child[1]);
    }
}

/**
 * @brief 处理单个顶层定义，分派到变量声明或函数定义。
 */
void ExtDef(Node *node) {
    if (node == NULL || node->childno == 0) return;

    Type spec = Specifier(node->child[0]);
    if (node->childno < 2) return;

    if (strcmp(node->child[1]->name, "ExtDecList") == 0) {
        ExtDecList(node->child[1], spec);
    } else if (strcmp(node->child[1]->name, "FunDec") == 0) {
        FunDec(node->child[1], spec);
        if (node->childno > 2) {
            CompSt(node->child[2], spec);
        }
    }
}

/**
 * @brief 处理顶层变量声明列表并尝试写入符号表。
 */
void ExtDecList(Node *node, Type spec) {
    if (node == NULL || node->childno == 0) return;

    FieldList f = VarDec(node->child[0], spec);
    if (f != NULL) {
        f->symbolKind = SYMBOL_VARIABLE;
        if (insert(f) == 0) {
            printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", f->lineno, f->name);
            error++;
        }
    }

    if (node->childno > 1) {
        ExtDecList(node->child[2], spec);
    }
}

/**
 * @brief 将语法树中的 Specifier 节点转换为内部 Type。
 */
Type Specifier(Node *node) {
    if (node == NULL || node->childno == 0) return NULL;

    if (strcmp(node->child[0]->name, "TYPE") == 0) {
        if (strcmp(node->child[0]->yytext, "float") == 0) {
            return newBasicType(BASIC_FLOAT);
        }
        return newBasicType(BASIC_INT);
    }

    return StructSpecifier(node->child[0]);
}

/**
 * @brief 处理结构体类型定义或结构体类型引用。
 */
Type StructSpecifier(Node *node) {
    if (node == NULL || node->childno == 0) return NULL;

    int isDefinition = 0;
    Node *optTag = NULL;
    Node *defList = NULL;
    Node *tag = NULL;

    for (int i = 0; i < node->childno; i++) {
        if (strcmp(node->child[i]->name, "LC") == 0) {
            isDefinition = 1;
        } else if (strcmp(node->child[i]->name, "OptTag") == 0) {
            optTag = node->child[i];
        } else if (strcmp(node->child[i]->name, "DefList") == 0) {
            defList = node->child[i];
        } else if (strcmp(node->child[i]->name, "Tag") == 0) {
            tag = node->child[i];
        }
    }

    if (isDefinition) {
        char *structName = NULL;
        if (optTag != NULL && optTag->childno > 0) {
            structName = optTag->child[0]->yytext;
        }

        Type t = (Type)malloc(sizeof(Type_));
        t->kind = STRUCTURE;
        t->structName = copyString(structName);
        t->u.structure = DefList(defList, 1); // 结构体内部字段单独走一套 DefList 逻辑

        if (structName != NULL) {
            FieldList f = newField(structName, t, SYMBOL_STRUCTURE, node->lineno);
            if (insert(f) == 0) {
                printf("Error type 16 at Line %d: Duplicated name \"%s\".\n", node->lineno, structName);
                error++;
            }
        }

        return t;
    }

    if (tag != NULL && tag->childno > 0) {
        char *structName = tag->child[0]->yytext;
        FieldList f = search(structName);
        if (!isStructureSymbol(f)) {
            printf("Error type 17 at Line %d: Undefined structure \"%s\".\n", node->lineno, structName);
            error++;
            return NULL;
        }
        return f->type;
    }

    return NULL;
}

/**
 * @brief 处理函数定义头部，建立函数类型并登记形参。
 */
void FunDec(Node *node, Type spec) {
    if (node == NULL || node->childno == 0) return;

    char *funcName = node->child[0]->yytext;
    Type type = newFunctionType(spec);
    FieldList paramHead = NULL;
    FieldList paramTail = NULL;

    if (node->childno == 4) {
        Node *varList = node->child[2];
        while (varList != NULL) {
            Node *paramDec = varList->child[0];
            Type paramType = Specifier(paramDec->child[0]);
            FieldList param = VarDec(paramDec->child[1], paramType);

            if (param != NULL) {
                param->symbolKind = SYMBOL_VARIABLE;
                appendField(&paramHead, &paramTail, param);
                type->u.function.paramNum++;
                if (insert(param) == 0) {
                    /* 形参在当前实现里也进入全局符号表，因此同样要检查重名。 */
                    printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", param->lineno, param->name);
                    error++;
                }
            }

            if (varList->childno == 3) {
                varList = varList->child[2];
            } else {
                break;
            }
        }
    }

    type->u.function.params = paramHead;

    FieldList field = newField(funcName, type, SYMBOL_FUNCTION, node->lineno);
    if (insert(field) == 0) {
        printf("Error type 4 at Line %d: Redefined function \"%s\".\n", node->lineno, funcName);
        error++;
    }
}

/**
 * @brief 处理复合语句块，依次分析其中的局部定义和语句。
 */
void CompSt(Node *node, Type ftype) {
    if (node == NULL) return;

    for (int i = 0; i < node->childno; i++) {
        if (strcmp(node->child[i]->name, "DefList") == 0) {
            DefList(node->child[i], 0);
        } else if (strcmp(node->child[i]->name, "StmtList") == 0) {
            Node *stmtList = node->child[i];
            while (stmtList != NULL && stmtList->childno > 0) {
                Stmt(stmtList->child[0], ftype);
                stmtList = stmtList->childno > 1 ? stmtList->child[1] : NULL;
            }
        }
    }
}

/**
 * @brief 处理定义列表。
 *
 * 当 isStruct 为 1 时，返回的是结构体字段链表；否则对应函数体内的局部变量定义。
 */
FieldList DefList(Node *node, int isStruct) {
    FieldList head = NULL;
    FieldList tail = NULL;

    while (node != NULL && node->childno > 0) {
        FieldList fields = Def(node->child[0], isStruct);
        for (FieldList p = fields; p != NULL;) {
            FieldList next = p->tail;
            if (isStruct && findInList(head, p->name) != NULL) {
                /* 结构体字段重名只在同一结构体内部判错。 */
                printf("Error type 15 at Line %d: Redefined field \"%s\".\n", p->lineno, p->name);
                error++;
            }
            appendField(&head, &tail, p);
            p = next;
        }
        node = node->childno > 1 ? node->child[1] : NULL;
    }

    return head;
}

/**
 * @brief 处理单个 Def 节点。
 */
FieldList Def(Node *node, int isStruct) {
    if (node == NULL || node->childno < 2) return NULL;

    Type spec = Specifier(node->child[0]);
    return DecList(node->child[1], spec, isStruct);
}

/**
 * @brief 处理以逗号连接的声明列表。
 */
FieldList DecList(Node *node, Type spec, int isStruct) {
    FieldList head = NULL;
    FieldList tail = NULL;

    while (node != NULL && node->childno > 0) {
        FieldList field = Dec(node->child[0], spec, isStruct);
        if (field != NULL) {
            appendField(&head, &tail, field);
        }
        node = node->childno > 1 ? node->child[2] : NULL;
    }

    return head;
}

/**
 * @brief 处理单个声明，可区分结构体字段和普通局部变量。
 */
FieldList Dec(Node *node, Type spec, int isStruct) {
    if (node == NULL || node->childno == 0) return NULL;

    FieldList f = VarDec(node->child[0], spec);
    if (f == NULL) return NULL;

    if (isStruct) {
        f->symbolKind = SYMBOL_FIELD;

        if (node->childno == 3) {
            printf("Error type 15 at Line %d: Initialized field \"%s\".\n", node->lineno, f->name);
            error++;
        }
        return f;
    }

    f->symbolKind = SYMBOL_VARIABLE;
    if (insert(f) == 0) {
        printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", f->lineno, f->name);
        error++;
    }

    if (node->childno == 3) {
        Type t2 = Exp(node->child[2]);
        if (f->type != NULL && t2 != NULL && TypeEqual(f->type, t2) == 0) {
            printf("Error type 5 at Line %d: Type mismatched for assignment.\n", node->lineno);
            error++;
        }
    }

    return f;
}

/**
 * @brief 处理变量声明，支持递归构造数组类型。
 */
FieldList VarDec(Node *node, Type spec) {
    if (node == NULL || node->childno == 0) return NULL;

    if (strcmp(node->child[0]->name, "ID") == 0) {
        return newField(node->child[0]->yytext, spec, SYMBOL_VARIABLE, node->lineno);
    }

    FieldList field = VarDec(node->child[0], spec);
    if (field == NULL) return NULL;

    int size = atoi(node->child[2]->yytext);
    field->type = newArrayType(field->type, size);
    return field;
}

/**
 * @brief 处理语句节点，包括表达式语句、复合语句、return、if、while。
 */
void Stmt(Node *node, Type ftype) {
    if (node == NULL || node->childno == 0) return;

    if (strcmp(node->child[0]->name, "Exp") == 0) {
        Exp(node->child[0]);
    } else if (strcmp(node->child[0]->name, "CompSt") == 0) {
        CompSt(node->child[0], ftype);
    } else if (strcmp(node->child[0]->name, "RETURN") == 0) {
        Type retType = Exp(node->child[1]);
        if (ftype != NULL && retType != NULL && TypeEqual(ftype, retType) == 0) {
            printf("Error type 8 at Line %d: Type mismatched for return.\n", node->lineno);
            error++;
        }
    } else if (strcmp(node->child[0]->name, "IF") == 0) {
        Type condType = Exp(node->child[2]);
        if (condType != NULL && !isIntType(condType)) {
            printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
            error++;
        }
        Stmt(node->child[4], ftype);
        if (node->childno == 7) {
            Stmt(node->child[6], ftype);
        }
    } else if (strcmp(node->child[0]->name, "WHILE") == 0) {
        Type condType = Exp(node->child[2]);
        if (condType != NULL && !isIntType(condType)) {
            printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
            error++;
        }
        Stmt(node->child[4], ftype);
    }
}

/**
 * @brief 递归分析表达式并返回其类型。
 *
 * 这是语义分析阶段的核心函数，覆盖左值检查、数组访问、结构体访问和函数调用。
 */
Type Exp(Node *node) {
    if (node == NULL || node->childno == 0) return NULL;

    if (node->childno == 1) {
        if (strcmp(node->child[0]->name, "INT") == 0) {
            return newBasicType(BASIC_INT);
        }
        if (strcmp(node->child[0]->name, "FLOAT") == 0) {
            return newBasicType(BASIC_FLOAT);
        }
        if (strcmp(node->child[0]->name, "ID") == 0) {
            FieldList f = search(node->child[0]->yytext);
            if (f == NULL || f->symbolKind != SYMBOL_VARIABLE) {
                printf("Error type 1 at Line %d: Undefined variable \"%s\".\n", node->lineno, node->child[0]->yytext);
                error++;
                return NULL;
            }
            return f->type;
        }
    }

    if (node->childno == 2) {
        Type t = Exp(node->child[1]);
        if (t == NULL) return NULL;

        if (strcmp(node->child[0]->name, "MINUS") == 0) {
            if (!isNumericType(t)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
                error++;
                return NULL;
            }
            return t;
        }

        if (strcmp(node->child[0]->name, "NOT") == 0) {
            if (!isIntType(t)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
                error++;
                return NULL;
            }
            return newBasicType(BASIC_INT);
        }
    }

    if (node->childno == 3 && strcmp(node->child[0]->name, "LP") == 0) {
        return Exp(node->child[1]);
    }

    if (node->childno == 3 && strcmp(node->child[1]->name, "ASSIGNOP") == 0) {
        if (!isLValue(node->child[0])) {
            /* 继续递归两侧子表达式，尽量保留更底层的语义错误信息。 */
            printf("Error type 6 at Line %d: The left-hand side of an assignment must be a variable.\n", node->child[0]->lineno);
            error++;
            Exp(node->child[0]);
            Exp(node->child[2]);
            return NULL;
        }

        Type t1 = Exp(node->child[0]);
        Type t2 = Exp(node->child[2]);
        if (t1 != NULL && t2 != NULL && TypeEqual(t1, t2) == 0) {
            printf("Error type 5 at Line %d: Type mismatched for assignment.\n", node->lineno);
            error++;
            return NULL;
        }
        return t1;
    }

    if (node->childno == 3 &&
        (strcmp(node->child[1]->name, "PLUS") == 0 ||
         strcmp(node->child[1]->name, "MINUS") == 0 ||
         strcmp(node->child[1]->name, "STAR") == 0 ||
         strcmp(node->child[1]->name, "DIV") == 0)) {
        Type t1 = Exp(node->child[0]);
        Type t2 = Exp(node->child[2]);
        if (t1 != NULL && t2 != NULL) {
            if (!isNumericType(t1) || !isNumericType(t2) || !TypeEqual(t1, t2)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
                error++;
                return NULL;
            }
            return t1;
        }
        return NULL;
    }

    if (node->childno == 3 && strcmp(node->child[1]->name, "RELOP") == 0) {
        Type t1 = Exp(node->child[0]);
        Type t2 = Exp(node->child[2]);
        if (t1 != NULL && t2 != NULL) {
            if (!isNumericType(t1) || !isNumericType(t2) || !TypeEqual(t1, t2)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
                error++;
                return NULL;
            }
            return newBasicType(BASIC_INT);
        }
        return NULL;
    }

    if (node->childno == 3 &&
        (strcmp(node->child[1]->name, "AND") == 0 ||
         strcmp(node->child[1]->name, "OR") == 0)) {
        Type t1 = Exp(node->child[0]);
        Type t2 = Exp(node->child[2]);
        if (t1 != NULL && t2 != NULL) {
            if (!isIntType(t1) || !isIntType(t2)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", node->lineno);
                error++;
                return NULL;
            }
            return newBasicType(BASIC_INT);
        }
        return NULL;
    }

    if (node->childno == 4 && strcmp(node->child[1]->name, "LB") == 0) {
        Type arrayType = Exp(node->child[0]);
        Type indexType = Exp(node->child[2]);

        if (arrayType != NULL && actualType(arrayType)->kind != ARRAY) {
            printf("Error type 10 at Line %d: Expected array type.\n", node->lineno);
            error++;
            return NULL;
        }

        if (indexType != NULL && !isIntType(indexType)) {
            printf("Error type 12 at Line %d: Array index is not an integer.\n", node->lineno);
            error++;
            return NULL;
        }

        if (arrayType != NULL) return actualType(arrayType)->u.array.elem; // 数组访问后类型会降一维
        return NULL;
    }

    if (node->childno == 3 && strcmp(node->child[1]->name, "DOT") == 0) {
        Type t = Exp(node->child[0]);
        if (t == NULL) return NULL;

        t = actualType(t);
        if (t->kind != STRUCTURE) {
            printf("Error type 13 at Line %d: Illegal use of \".\".\n", node->lineno);
            error++;
            return NULL;
        }

        Type fieldType = findFieldType(t, node->child[2]->yytext);
        if (fieldType == NULL) {
            printf("Error type 14 at Line %d: Non-existent field \"%s\".\n", node->lineno, node->child[2]->yytext);
            error++;
            return NULL;
        }
        return fieldType;
    }

    if (strcmp(node->child[0]->name, "ID") == 0 && node->childno >= 3) {
        char *funcName = node->child[0]->yytext;
        FieldList f = search(funcName);

        if (f == NULL || isStructureSymbol(f)) {
            printf("Error type 2 at Line %d: Undefined function \"%s\".\n", node->lineno, funcName);
            error++;
            return NULL;
        }

        if (!isFunctionSymbol(f)) {
            printf("Error type 11 at Line %d: \"%s\" is not a function.\n", node->lineno, funcName);
            error++;
            return NULL;
        }

        int argCount = 0;
        FieldList args = NULL;
        if (node->childno == 4) {
            args = collectArgs(node->child[2], &argCount);
        }

        /* 实参自身若已出错，先跳过匹配检查，避免产生连锁误报。 */
        if (!hasUnknownType(args) &&
            !hasUnknownType(f->type->u.function.params) &&
            (argCount != f->type->u.function.paramNum ||
             !argsMatch(args, f->type->u.function.params))) {
            printf("Error type 9 at Line %d: Function \"%s\" is not applicable for given arguments.\n", node->lineno, funcName);
            error++;
        }

        return f->type->u.function.funcType;
    }

    return NULL;
}

/**
 * @brief 复制字符串，统一封装 malloc + strcpy。
 */
static char *copyString(const char *s) {
    if (s == NULL) return NULL;
    char *copy = (char *)malloc(strlen(s) + 1);
    strcpy(copy, s);
    return copy;
}

/**
 * @brief 构造基本类型节点。
 */
static Type newBasicType(int basic) {
    Type t = (Type)malloc(sizeof(Type_));
    t->kind = BASIC;
    t->structName = NULL;
    t->u.basic = basic;
    return t;
}

/**
 * @brief 构造数组类型节点。
 */
static Type newArrayType(Type elem, int size) {
    Type t = (Type)malloc(sizeof(Type_));
    t->kind = ARRAY;
    t->structName = NULL;
    t->u.array.elem = elem;
    t->u.array.size = size;
    return t;
}

/**
 * @brief 构造函数类型节点，初始时形参列表为空。
 */
static Type newFunctionType(Type retType) {
    Type t = (Type)malloc(sizeof(Type_));
    t->kind = FUNCTION;
    t->structName = NULL;
    t->u.function.funcType = retType;
    t->u.function.params = NULL;
    t->u.function.paramNum = 0;
    return t;
}

/**
 * @brief 构造符号表项。
 */
static FieldList newField(const char *name, Type type, SymbolKind kind, int lineno) {
    FieldList f = (FieldList)malloc(sizeof(FieldList_));
    f->name = copyString(name);
    f->type = type;
    f->tail = NULL;
    f->hashNext = NULL;
    f->symbolKind = kind;
    f->lineno = lineno;
    return f;
}

/**
 * @brief 将字段节点追加到链表尾部。
 */
static void appendField(FieldList *head, FieldList *tail, FieldList field) {
    if (field == NULL) return;
    field->tail = NULL;
    if (*head == NULL) {
        *head = field;
        *tail = field;
    } else {
        (*tail)->tail = field;
        *tail = field;
    }
}

/**
 * @brief 在线性字段链表中按名字查找节点。
 */
static FieldList findInList(FieldList list, const char *name) {
    for (FieldList p = list; p != NULL; p = p->tail) {
        if (p->name != NULL && strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}

/**
 * @brief 返回类型的实际语义类型。
 *
 * 当前实现没有别名或 typedef 展开逻辑，因此直接返回自身。
 */
static Type actualType(Type type) {
    return type;
}

/**
 * @brief 判断类型是否为 int。
 */
static int isIntType(Type type) {
    type = actualType(type);
    return type != NULL && type->kind == BASIC && type->u.basic == BASIC_INT;
}

/**
 * @brief 判断类型是否为数值类型。
 */
static int isNumericType(Type type) {
    type = actualType(type);
    return type != NULL && type->kind == BASIC &&
           (type->u.basic == BASIC_INT || type->u.basic == BASIC_FLOAT);
}

/**
 * @brief 判断表达式是否可以出现在赋值左侧。
 */
static int isLValue(Node *node) {
    if (node == NULL) return 0;
    if (node->childno == 1 && strcmp(node->child[0]->name, "ID") == 0) return 1;
    if (node->childno == 4 && strcmp(node->child[1]->name, "LB") == 0) return 1;
    if (node->childno == 3 && strcmp(node->child[1]->name, "DOT") == 0) return 1;
    return 0;
}

/**
 * @brief 判断符号表项是否表示函数。
 */
static int isFunctionSymbol(FieldList field) {
    return field != NULL &&
           (field->symbolKind == SYMBOL_FUNCTION ||
            (field->type != NULL && field->type->kind == FUNCTION));
}

/**
 * @brief 判断符号表项是否表示结构体类型名。
 */
static int isStructureSymbol(FieldList field) {
    return field != NULL &&
           field->symbolKind == SYMBOL_STRUCTURE &&
           field->type != NULL &&
           field->type->kind == STRUCTURE;
}

/**
 * @brief 收集函数实参类型列表，同时统计参数个数。
 */
static FieldList collectArgs(Node *node, int *count) {
    FieldList head = NULL;
    FieldList tail = NULL;

    while (node != NULL && node->childno > 0) {
        Type argType = Exp(node->child[0]);
        FieldList arg = newField("", argType, SYMBOL_FIELD, node->lineno);
        appendField(&head, &tail, arg);
        (*count)++;

        if (node->childno == 3) {
            node = node->child[2];
        } else {
            break;
        }
    }

    return head;
}

/**
 * @brief 判断实参与形参列表是否逐项匹配。
 */
static int argsMatch(FieldList args, FieldList params) {
    FieldList a = args;
    FieldList p = params;

    while (a != NULL && p != NULL) {
        if (a->type == NULL || p->type == NULL || TypeEqual(a->type, p->type) == 0) {
            return 0;
        }
        a = a->tail;
        p = p->tail;
    }

    return a == NULL && p == NULL;
}

/**
 * @brief 判断一组类型中是否包含未知类型。
 */
static int hasUnknownType(FieldList list) {
    for (FieldList p = list; p != NULL; p = p->tail) {
        if (p->type == NULL) return 1;
    }
    return 0;
}

/**
 * @brief 在结构体字段链表中查找指定字段的类型。
 */
static Type findFieldType(Type structure, const char *fieldName) {
    structure = actualType(structure);
    if (structure == NULL || structure->kind != STRUCTURE) return NULL;

    FieldList field = findInList(structure->u.structure, fieldName);
    if (field == NULL) return NULL;
    return field->type;
}
