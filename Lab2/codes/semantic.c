#include "semantic.h" 

FieldList hashTable[HASH_SIZE];


//散列函数
//常数 0x3fff 确定了符号表的大小（即16384）
unsigned int hash_pjw(char *name){
	unsigned int val = 0, i;
	for(;*name;++name){
		val = (val << 2) + *name;
		if(i=val & ~0x3fff){
			val = (val ^ (i>>12)) & 0x3fff;
		}
	}
    return val % HASH_SIZE;
}

void initHashtable(){
	for(int i=0; i<HASH_SIZE; i++){
		hashTable[i] = NULL;
	}
}


int TypeEqual(Type t1,Type t2){
	if((t1==NULL)||(t2==NULL)) return 0;
	if(t1->kind!=t2->kind) return 0;
    if(t1->kind == BASIC)
    {
        return (t1->u.basic == t2->u.basic);
    }
    if(t1->kind == ARRAY)
    {
        return (TypeEqual(t1->u.array.elem,t2->u.array.elem));
    }
    
    if(t1 ->kind ==FUNCTION)
    {
        //参数类型和个数相等
        if(t1->u.function.paramNum != t2->u.function.paramNum) return 0;
        if(TypeEqual(t1->u.function.funcType,t2->u.function.funcType) == 0) return 0;
        FieldList p1 = t1->u.function.params;
        FieldList p2 = t2->u.function.params;
        for(int i = 0; i<t1->u.function.paramNum;i++)
        {
            if(TypeEqual(p1->type,p2->type) == 0) return 0;
            p1 = p1->tail; p2 = p2->tail;
        }
        return 1;
    }
    return 0;
}

// 程序 
//Program     : ExtDefList
void Program(Node *root){
    if(root==NULL)
        return;
    if(!strcmp("ExtDefList" , root->child[0]->name))
    {
        ExtDefList(root ->child[0]);
    }
}


// 全局变量、结构体或函数 
// ExtDef      : Specifier ExtDecList SEMI  全局变量
//             | Specifier SEMI             结构体 
//             | Specifier FunDec CompSt    函数


// 函数头
// FunDec      : ID LP VarList RP             
//             | ID LP RP                             
//             ;
void FunDec(Node *node,Type spec)
{
    //ID
    FieldList field = (FieldList)malloc(sizeof(FieldList_));
    field->name = node->child[0]->yytext;
    Type type = (Type)malloc(sizeof(Type_));
    type->kind = FUNCTION;
    type->u.function.funcType = spec;
    //             | ID LP RP                 
    type->u.function.paramNum = 0;
    type->u.function.params = NULL;
    //ID LP VarList RP 
    if(strcmp(node->child[2]->name,"VarList")==0)
    {
        // 形参列表
        // VarList     : ParamDec COMMA VarList 
        //             | ParamDec                    
        //             ;
        Node * varList = node->child[2];
        while(1)
        {
            // 对一个形参的定义
            // ParamDec    : Specifier VarDec
            Node * paramDec =varList->child[0];
            Type t = Specifier(paramDec->child[0]);  //调用解析Specifier节点子树的函数获得类型，并返回该类型
            FieldList p =VarDec(paramDec->child[1],t);
            //查看是否重复定义 函数内部
            if (search(p->name) != NULL)
                printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", varList->lineno, p->name);
            else
                insert(p);
            type->u.function.paramNum++;
            //头插入
            p->tail = type->u.function.params;
            type->u.function.params = p;
            // 后面还有形参
            if(varList->childno == 3)
            {
                varList=varList->child[2];
            }
            else
            {
                break;
            }
        }
        
    }
    //将函数加入符号表
    field->type = type;
    FieldList temp =search(field->name) ;
    if(temp !=NULL)
    {
        printf("Error type 4 at Line %d: Redefined function \"%s\".\n", node->lineno, temp->name);
    }
    else
    insert(field);
}

// 函数体
// CompSt      : LC DefList StmtList RC                   
//             ;
void CompSt(Node *node,Type ftype)
{
    //to do: 函数体的处理
}

// 一条语句
// Stmt    : Exp SEMI                                
//         | CompSt                                  
//         | RETURN Exp SEMI                         
//         | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE 
//         | IF LP Exp RP Stmt ELSE Stmt                        
//         | WHILE LP Exp RP Stmt                    
//         ;   
//语句  传进来ftype是因为返回值
void Stmt(Node *node,Type ftype)
{
   //to do: 语句的处理
    
}

// 零个或多个Def
// DefList : Def DefList             
//         | /* empty */              
//         ;


// DecList : Dec                 
//         | Dec COMMA DecList        
//         ;
void DecList(Node *node,Type spec)
{
    //to do: 处理变量和数组的声明
}

// Dec     : VarDec               
//         | VarDec ASSIGNOP Exp  a = 5
//         ;

void Dec(Node *node,Type spec)
{
    //to do: 处理单个变量和数组的声明
}

// 表达式
// Exp     : Exp ASSIGNOP Exp  a = 5
//         | Exp AND Exp       a&b
//         | Exp OR Exp        a|b
//         | Exp RELOP Exp     a>b
//         | Exp PLUS Exp      a+b
//         | Exp MINUS Exp     a-b
//         | Exp STAR Exp      a*b
//         | Exp DIV Exp       a/b
//         | LP Exp RP         (a)
//         | MINUS Exp         -a
//         | NOT Exp           ~a

//         | ID LP Args RP     a(b)
//         | ID LP RP          a()
//         | Exp LB Exp RB     a[b]
//         | Exp DOT ID        a.b
//         | ID                a
//         | INT               1
//         | FLOAT             1.0
//         ;
Type Exp(Node *node)
{
    if(node == NULL) return NULL;
    // Exp ASSIGNOP Exp
    if (node->childno ==3 && strcmp(node->child[1]->name, "ASSIGNOP") == 0)
    {   
        Node * exp1 = node->child[0];
        Node * exp2 = node->child[2];
        // 赋值号左边出现一个只有右值的表达式。
        // ID 
        if(exp1->childno == 1 && !(strcmp(exp1->child[0]->name, "ID") == 0) )
        {
            printf("Error type 6 at Line %d: The left-hand side of an assignment must be a variable.\n", exp1->lineno);
        }
        //赋值号两边的表达式类型不匹配。
        Type t1 = Exp(exp1);
        Type t2 = Exp(exp2);
        if(TypeEqual(t1,t2) == 0)
        {
            //防止重复报错（1.cmm、12.cmm）
            if(t1!=NULL && t2!=NULL)
			printf("Error type 5 at Line %d: Type mismatched for assignment.\n",node->lineno);
            return NULL;
        }
        else 
        return t1;
    }
    
    //to do: 其他产生式的处理
    // Exp AND Exp | Exp OR Exp | Exp RELOP Exp | Exp PLUS Exp | Exp MINUS Exp | Exp STAR Exp | Exp DIV Exp
    
}

// 零个或多个对一个变量的定义VarDec
// ExtDecList  : VarDec                  
//             | VarDec COMMA ExtDecList 
//             ;
void ExtDecList(Node * node,Type spec)
{
    //to do: 处理变量和数组的声明
}

// 对一个变量的定义
// VarDec      : ID                
//             | VarDec LB INT RB 
//             ;
// a a[10][2]
FieldList VarDec(Node *node,Type spec)
{
    Node * temp = node;
    int num =0 ;
    // VarDec 循环到最后的ID得到标识符名
    while(strcmp(temp->child[0]->name,"ID") != 0)
    {
        temp = temp->child[0];
        num++;
    }
    FieldList field = (FieldList)malloc(sizeof(FieldList_));
    field->name = temp->child[0]->yytext;    //获得标识符名
    //若num == 0  ，即 VarDec: ID，简单变量
    if(strcmp(node->child[0]->name,"ID") == 0)
    {
        field->type = spec;
        return field;
    }
    //若num > 0，即 VarDec: VarDec LB INT RB，数组
    Node * temp2 = node;
    Type last = spec;
    for(int i=0;i<num;i++) //从上向下递归地构造数组类型，如a[2][3][4],num = 3维，先构造最后面的子维度，即大小为4的维度
    {
        Type ti = (Type)malloc(sizeof(Type_));
        ti->kind = ARRAY;
        ti->u.array.size = atoi(temp2->child[2]->yytext);
        // size每一维都要递进
        temp2 = temp2->child[0];
        ti->u.array.elem = last;
        // elem每一维都要递进
        last = ti;
        field->type = ti;
    }
    return field;
}

// Specifier   : TYPE            
//             | StructSpecifier 
//             ;
Type Specifier(Node *node)
{
    // TYPE
    if(strcmp(node->child[0]->name,"TYPE")==0)
    {
        Type spec = (Type)malloc(sizeof(Type_));
        spec->kind = BASIC;
        if(strcmp(node->child[0]->yytext,"float")==0)
            spec->u.basic=FLOAT_TYPE;
        else
            spec->u.basic=INT_TYPE;
        return spec;
    }
    // StructSpecifier
    else
    {
        if(strcmp(node->child[0]->name,"StructSpecifier")!=0)
            printf("debug in Specifier: %s\n",node->child[0]->name);
        Type t = StructSpecifier(node->child[0]);
        Type t2 = (Type)malloc(sizeof(Type_));
        t2->kind =STRUCTURE;
        t2->u.structure = t->u.structure;
        return t2;
    }
}

