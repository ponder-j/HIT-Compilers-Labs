# GNU make手册：http://www.gnu.org/software/make/manual/make.html
# ************ 遇到不明白的地方请google以及阅读手册 *************

# 编译器设定和编译选项
CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -std=c99

# 编译目标：当前目录下的编译器源文件，排除备用入口 test.c 和自动生成文件
CFILES = $(filter-out ./test.c ./lex.yy.c ./syntax.tab.c,$(shell find ./ -maxdepth 1 -name "*.c"))
OBJS = $(CFILES:.c=.o)
LFILE = $(shell find ./ -name "*.l")
YFILE = $(shell find ./ -name "*.y")
LFC = $(shell find ./ -name "*.l" | sed s/[^/]*\\.l/lex.yy.c/)
YFC = $(shell find ./ -name "*.y" | sed s/[^/]*\\.y/syntax.tab.c/)
LFO = $(LFC:.c=.o)
YFO = $(YFC:.c=.o)
Testlist := $(wildcard Test/*.cmm)

parser: syntax $(OBJS)
	$(CC) -o parser $(OBJS) $(YFO) -lfl 

syntax: lexical syntax-c
	$(CC) $(CFLAGS) -c $(YFC) -o $(YFO)

lexical: $(LFILE)
	$(FLEX) -o $(LFC) $(LFILE)

syntax-c: $(YFILE)
	$(BISON) -o $(YFC) -d -v $(YFILE)

-include $(patsubst %.o, %.d, $(OBJS))

# 定义的一些伪目标
.PHONY: clean test
# test:
# 	 for file in $(Testlist); do \
#         	./parser $$file >> result.txt; \
#         	echo "" >> result.txt; \
#     	 done
test:
	mkdir -p Result
	for file in $(Testlist); do \
		name=$$(basename $$file .cmm); \
		./parser $$file Result/$$name.ir; \
	done
clean:
	rm -f parser lex.yy.c syntax.tab.c syntax.tab.h syntax.output result.txt
	rm -f $(OBJS) $(OBJS:.o=.d)
	rm -f $(YFO)
	rm -f $(LFC) $(YFC) $(YFC:.c=.h)
	rm -f *~
	rm -rf Result
