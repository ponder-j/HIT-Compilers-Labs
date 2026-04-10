%{
#include "tree.h"
#include "syntax.tab.h"

extern int error;
int yycolumn = 1;

// 定义 YY_USER_ACTION 宏，更新位置信息
// YY_USER_ACTION 在每次匹配成功后先于后续代码块执行
// 实验其实并没有要求实现行内列号
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

%option yylineno

/* 正则表达式定义 */
digit       [0-9]
letter      [_a-zA-Z]

/* 整数：支持十进制，八进制，十六进制 */
dec_int     0|[1-9][0-9]*
oct_int     0[0-7]+
hex_int     0[xX][0-9a-fA-F]+
/* 错误的八进制和十六进制 (包含非法的字母或数字) */
err_oct     0[0-7]*[89][0-9a-zA-Z]*
err_hex     0[xX][0-9a-fA-F]*[g-zG-Z][0-9a-zA-Z]*

/* 浮点数：支持普通格式和指数格式 */
float_norm  [0-9]+\.[0-9]+
float_exp   [0-9]+\.[0-9]+[eE][+-]?[0-9]+
/* 错误的指数浮点数 */
err_float   [0-9]+\.[0-9]+[eE][+-]?

id          {letter}({letter}|{digit})*

%%

"int"|"float" { yylval.node = createNode("TYPE", yytext, yylineno, 1); return TYPE; }
"struct"      { yylval.node = createNode("STRUCT", yytext, yylineno, 1); return STRUCT; }
"return"      { yylval.node = createNode("RETURN", yytext, yylineno, 1); return RETURN; }
"if"          { yylval.node = createNode("IF", yytext, yylineno, 1); return IF; }
"else"        { yylval.node = createNode("ELSE", yytext, yylineno, 1); return ELSE; }
"while"       { yylval.node = createNode("WHILE", yytext, yylineno, 1); return WHILE; }

{dec_int}|{oct_int}|{hex_int} { yylval.node = createNode("INT", yytext, yylineno, 1); return INT; }
{err_oct}   { error++; printf("Error type A at Line %d: Illegal octal number \'%s\'\n", yylineno, yytext); return INT; }
{err_hex}   { error++; printf("Error type A at Line %d: Illegal hexadecimal number \'%s\'\n", yylineno, yytext); return INT; }

{float_norm}|{float_exp} { yylval.node = createNode("FLOAT", yytext, yylineno, 1); return FLOAT; }
{err_float} { error++; printf("Error type A at Line %d: Illegal floating point number \'%s\'\n", yylineno, yytext); return FLOAT; }

{id}        { yylval.node = createNode("ID", yytext, yylineno, 1); return ID; }

";"         { yylval.node = createNode("SEMI", yytext, yylineno, 1); return SEMI; }
","         { yylval.node = createNode("COMMA", yytext, yylineno, 1); return COMMA; }
"="         { yylval.node = createNode("ASSIGNOP", yytext, yylineno, 1); return ASSIGNOP; }
">"|"<"|">="|"<="|"=="|"!=" { yylval.node = createNode("RELOP", yytext, yylineno, 1); return RELOP; }
"+"         { yylval.node = createNode("PLUS", yytext, yylineno, 1); return PLUS; }
"-"         { yylval.node = createNode("MINUS", yytext, yylineno, 1); return MINUS; }
"*"         { yylval.node = createNode("STAR", yytext, yylineno, 1); return STAR; }
"/"         { yylval.node = createNode("DIV", yytext, yylineno, 1); return DIV; }
"&&"        { yylval.node = createNode("AND", yytext, yylineno, 1); return AND; }
"||"        { yylval.node = createNode("OR", yytext, yylineno, 1); return OR; }
"."         { yylval.node = createNode("DOT", yytext, yylineno, 1); return DOT; }
"!"         { yylval.node = createNode("NOT", yytext, yylineno, 1); return NOT; }
"("         { yylval.node = createNode("LP", yytext, yylineno, 1); return LP; }
")"         { yylval.node = createNode("RP", yytext, yylineno, 1); return RP; }
"["         { yylval.node = createNode("LB", yytext, yylineno, 1); return LB; }
"]"         { yylval.node = createNode("RB", yytext, yylineno, 1); return RB; }
"{"         { yylval.node = createNode("LC", yytext, yylineno, 1); return LC; }
"}"         { yylval.node = createNode("RC", yytext, yylineno, 1); return RC; }


"/*"        {
                char c, prev = 0;
                while ((c = input()) != 0) {
                    if (c == '\n') yycolumn = 1;
                    if (c == '/' && prev == '*') break;
                    prev = c;
                }
            }
"//"        {
                char c;
                while ((c = input()) != '\n' && c != 0);
                yycolumn = 1;
            }

[ \t\r]+    { /* 空白字符略过 */ }
\n          { yycolumn = 1; }

.           { error++; printf("Error type A at Line %d: Mysterious characters \'%s\'\n", yylineno, yytext); }

%%