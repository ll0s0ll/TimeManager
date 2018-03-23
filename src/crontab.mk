# リンクしてほしいオブジェクトファイルの絶対パスを追加する。
OBJECTS += $(addprefix $(OBJ_DIR)/, crontab.o crontab_entry.o crontab_misc.o)

# 依存関係を絶対パスで書く。(依存関係の一番最初は必ずソースファイルにする)
$(OBJ_DIR)/crontab.o: $(SOURCE_DIR)/crontab.c \
                      $(INCLUDE_DIR)/crontab.h \
                      $(INCLUDE_DIR)/common.h \
                      $(INCLUDE_DIR)/crontab_cron.h

$(OBJ_DIR)/crontab_entry.o: $(SOURCE_DIR)/crontab_entry.c \
                            $(INCLUDE_DIR)/crontab_cron.h

$(OBJ_DIR)/crontab_misc.o: $(SOURCE_DIR)/crontab_misc.c \
                           $(INCLUDE_DIR)/crontab_cron.h