BIN_DIR = bin
INCLUDE_DIR = include
INSTALL_DIR = /usr/local/bin
OBJ_DIR := obj
SOURCE_DIR = src

VPATH = $(BIN_DIR)

# srcディレクトリ直下のmakefileを読み込み対象とする。
SUBMAKEFILES = $(wildcard $(SOURCE_DIR)/*.mk)

# 以下2つの変数は、それぞれのmkファイルから値が追加される。
LIBS :=
OBJECTS :=

.PHONY: all
all: tm

# それぞれのルールをincludeする。
include $(SUBMAKEFILES)

# それぞれのルールにコマンドを追加する。
$(OBJ_DIR)/%.o:
#	$(CC) $(CFLAGS) -c -o $@ $<
	$(COMPILE.c) $(OUTPUT_OPTION) $<

# 一番最後に書かないと、includeされない。
tm: $(OBJECTS)
#	$(eval OBJ_DIR = hoge)
#	$(COMPILE.c) $(sort $(LIBS)) -o $(BIN_DIR)/$@ $^
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(sort $(LIBS)) -o $(BIN_DIR)/$@ $^
#	$(CC) $(CFLAGS) -lrt -lpthread $^ -o $@

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/tm $(OBJ_DIR)/*.o

.PHONY: install
install:
	cp $(BIN_DIR)/tm $(INSTALL_DIR)/tm

.PHONY: uninstall
uninstall:
	rm $(INSTALL_DIR)/tm