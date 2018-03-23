# リンクしてほしいオブジェクトファイルの絶対パスを追加する。
OBJECTS += $(OBJ_DIR)/set.o

# 依存関係を絶対パスで書く。(依存関係の一番最初は必ずソースファイルにする)
$(OBJ_DIR)/set.o: $(SOURCE_DIR)/set.c \
                  $(INCLUDE_DIR)/set.h \
                  $(INCLUDE_DIR)/add.h \
                  $(INCLUDE_DIR)/activate.h \
                  $(INCLUDE_DIR)/common.h \
                  $(INCLUDE_DIR)/terminate.h