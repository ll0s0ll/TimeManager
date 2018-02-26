# リンクしてほしいオブジェクトファイルの絶対パスを追加する。
OBJECTS += $(OBJ_DIR)/terminate.o

# 依存関係を絶対パスで書く。(依存関係の一番最初は必ずソースファイルにする)
$(OBJ_DIR)/terminate.o: $(SOURCE_DIR)/terminate.c \
                        $(INCLUDE_DIR)/terminate.h \
                        $(INCLUDE_DIR)/common.h