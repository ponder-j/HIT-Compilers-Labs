%{
    #include <stdio.h>
    #include <stdlib.h>
    #include "tree.h"
    #include "lex.yy.c"
    
    extern int yylineno;
    extern Node* Root;
    extern int error;

    void yyerror(char const *msg);
%}

%locations

// union 定义节点类型
%union {
    struct Node* node;
}

/* 终结符定义，使用 union 中的 node 类型 */
%token <node> INT FLOAT ID SEMI COMMA ASSIGNOP RELOP PLUS MINUS STAR DIV AND OR DOT NOT TYPE LP RP LB RB LC RC STRUCT RETURN IF ELSE WHILE

/* 结合性和优先级 */
/* Bison 中，越靠下，优先级越高 */
/* left 左结合；right 右结合；nonassoc 非结合 */
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT
%left LP RP LB RB DOT

/* 通过指定优先级的办法，避免 Bison 报告冲突，解决 if-else 悬空问题 */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* 非终结符定义 */
%type <node> Program ExtDefList ExtDef ExtDecList Specifier StructSpecifier OptTag Tag VarDec FunDec VarList ParamDec CompSt StmtList Stmt DefList Def DecList Dec Exp Args

%%

/* High-level Definitions */
/* $$ 代表父节点，$1, $2, ... 代表子节点 */
Program     : ExtDefList { $$ = createNode("Program", "", @$.first_line, 0); addNode(1, $$, $1); Root = $$; }
            ;

ExtDefList  : ExtDef ExtDefList { $$ = createNode("ExtDefList", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | /* empty */       { $$ = NULL; }
            ;

ExtDef      : Specifier ExtDecList SEMI { $$ = createNode("ExtDef", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Specifier SEMI            { $$ = createNode("ExtDef", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | Specifier FunDec CompSt   { $$ = createNode("ExtDef", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | error SEMI                { error++; }
            ;

ExtDecList  : VarDec { $$ = createNode("ExtDecList", "", @$.first_line, 0); addNode(1, $$, $1); }
            | VarDec COMMA ExtDecList { $$ = createNode("ExtDecList", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            ;

/* Specifiers */
Specifier   : TYPE { $$ = createNode("Specifier", "", @$.first_line, 0); addNode(1, $$, $1); }
            | StructSpecifier { $$ = createNode("Specifier", "", @$.first_line, 0); addNode(1, $$, $1); }
            ;

StructSpecifier : STRUCT OptTag LC DefList RC { $$ = createNode("StructSpecifier", "", @$.first_line, 0); addNode(5, $$, $1, $2, $3, $4, $5); }
                | STRUCT Tag { $$ = createNode("StructSpecifier", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
                ;

OptTag      : ID { $$ = createNode("OptTag", "", @$.first_line, 0); addNode(1, $$, $1); }
            | /* empty */ { $$ = NULL; }
            ;

Tag         : ID { $$ = createNode("Tag", "", @$.first_line, 0); addNode(1, $$, $1); }
            ;

/* Declarators */
VarDec      : ID { $$ = createNode("VarDec", "", @$.first_line, 0); addNode(1, $$, $1); }
            | VarDec LB INT RB { $$ = createNode("VarDec", "", @$.first_line, 0); addNode(4, $$, $1, $2, $3, $4); }
            ;

FunDec      : ID LP VarList RP { $$ = createNode("FunDec", "", @$.first_line, 0); addNode(4, $$, $1, $2, $3, $4); }
            | ID LP RP { $$ = createNode("FunDec", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            ;

VarList     : ParamDec COMMA VarList { $$ = createNode("VarList", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | ParamDec { $$ = createNode("VarList", "", @$.first_line, 0); addNode(1, $$, $1); }
            ;

ParamDec    : Specifier VarDec { $$ = createNode("ParamDec", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            ;

/* Statements */
CompSt      : LC DefList StmtList RC { $$ = createNode("CompSt", "", @$.first_line, 0); addNode(4, $$, $1, $2, $3, $4); }
            | error RC { error++; }
            ;

StmtList    : Stmt StmtList { $$ = createNode("StmtList", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | /* empty */   { $$ = NULL; }
            ;

Stmt        : Exp SEMI { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | CompSt   { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(1, $$, $1); }
            | RETURN Exp SEMI { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            // %prec 强制篡改当前这条规则的优先级
            | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(5, $$, $1, $2, $3, $4, $5); }
            | IF LP Exp RP Stmt ELSE Stmt { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(7, $$, $1, $2, $3, $4, $5, $6, $7); }
            | WHILE LP Exp RP Stmt { $$ = createNode("Stmt", "", @$.first_line, 0); addNode(5, $$, $1, $2, $3, $4, $5); }
            | error SEMI { error++; yyerror("Syntax error."); }
            ;

/* Local Definitions */
DefList     : Def DefList { $$ = createNode("DefList", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | /* empty */ { $$ = NULL; }
            ;

Def         : Specifier DecList SEMI { $$ = createNode("Def", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | error SEMI { error++; }
            ;

DecList     : Dec { $$ = createNode("DecList", "", @$.first_line, 0); addNode(1, $$, $1); }
            | Dec COMMA DecList { $$ = createNode("DecList", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            ;

Dec         : VarDec { $$ = createNode("Dec", "", @$.first_line, 0); addNode(1, $$, $1); }
            | VarDec ASSIGNOP Exp { $$ = createNode("Dec", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            ;

/* Expressions */
Exp         : Exp ASSIGNOP Exp { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp AND Exp      { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp OR Exp       { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp RELOP Exp    { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp PLUS Exp     { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp MINUS Exp    { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp STAR Exp     { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp DIV Exp      { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | LP Exp RP        { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | MINUS Exp        { $$ = createNode("Exp", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | NOT Exp          { $$ = createNode("Exp", "", @$.first_line, 0); addNode(2, $$, $1, $2); }
            | ID LP Args RP    { $$ = createNode("Exp", "", @$.first_line, 0); addNode(4, $$, $1, $2, $3, $4); }
            | ID LP RP         { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp LB Exp RB    { $$ = createNode("Exp", "", @$.first_line, 0); addNode(4, $$, $1, $2, $3, $4); }
            | Exp DOT ID       { $$ = createNode("Exp", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | ID               { $$ = createNode("Exp", "", @$.first_line, 0); addNode(1, $$, $1); }
            | INT              { $$ = createNode("Exp", "", @$.first_line, 0); addNode(1, $$, $1); }
            | FLOAT            { $$ = createNode("Exp", "", @$.first_line, 0); addNode(1, $$, $1); }
            | Exp LB error RB  { error++; }
            ;

Args        : Exp COMMA Args { $$ = createNode("Args", "", @$.first_line, 0); addNode(3, $$, $1, $2, $3); }
            | Exp            { $$ = createNode("Args", "", @$.first_line, 0); addNode(1, $$, $1); }
            ;

%%

void yyerror(char const *msg) {
    // 根据要求，确保只输出特定格式。
    // 如果 msg 不是自己手动调用的内容，则统一打出 "Syntax error." 格式。
    if (!strcmp(msg, "syntax error")) {
        printf("Error type B at Line %d: Syntax error.\n", yylineno);
    } else {
        printf("Error type B at Line %d: %s\n", yylineno, msg);
    }
}