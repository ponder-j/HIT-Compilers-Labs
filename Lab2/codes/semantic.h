#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

#define HASH_SIZE 0x3fff


//类形表示数据结构声明
typedef struct Type_ *Type;
typedef struct FieldList_ *FieldList;

typedef struct Type_ {
	enum {BASIC, ARRAY, STRUCTURE, FUNCTION,STR_SPE} kind;//区分结构类型和结构类型名
    union{
		//基本类型
		int basic;
		//数组类型信息包括元素类型与数组大小构成 
		struct {
			Type elem;
            int size;
		}array;
		// 结构体类型信息是一个链表
		FieldList structure;
		//函数
		struct{
			FieldList params;//参数
			Type funcType;//返回值类型
			int paramNum;//参数数量
		}function;
	}u;
}Type_;

//FieldList = 变量
typedef struct FieldList_ {
	char *name;//域的名字
	Type type;//域的类型
	FieldList tail;//下一个域
}FieldList_;

//符号表, 哈希表实现
unsigned int hash_pjw(char *name); //哈希函数求值
void initHashtable();  //初始化符号表
int insert(FieldList f); //插入符号表 返回0表示失败（重定义）
FieldList search(char *name);  //在符号表中查找变量名 返回NULL表示未找到
int TypeEqual(Type type1,Type type2); //类型等价比较

//树节点解析函数声明
void Program(Node *root);
void ExtDefList(Node *node);
void ExtDef(Node *node);

Type Specifier(Node *node);  //此符号Specifier负责生成声明语句中的数据类型，对应节点的解析函数需要返回该节点的综合属性即此非终结符生成的数据类型
void ExtDecList(Node *node,Type spec);//此符号ExtDecList负责处理变量声明中的变量列表，对应节点的解析函数需要传入该节点的继承属性即变量的数据类型，以便在获得变量名字之后，将变量名字和类型加入符号表项
void FunDec(Node *node,Type spec);
void CompSt(Node *node,Type ftype);
FieldList VarDec(Node *type,Type spec);
Type StructSpecifier(Node *node);
void OptTag(Node *node,Type spec);
void DefList(Node *node);
void Stmt(Node *node,Type ftype);
Type Exp(Node *root);
void Def(Node *node);
void DecList(Node *node,Type spec);
void Dec(Node *node,Type spec);

