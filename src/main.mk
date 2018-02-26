OBJECTS += $(OBJ_DIR)/main.o

$(OBJ_DIR)/main.o: $(SOURCE_DIR)/main.c \
                 $(INCLUDE_DIR)/activate.h \
                 $(INCLUDE_DIR)/add.h \
                 $(INCLUDE_DIR)/autoextend.h \
                 $(INCLUDE_DIR)/common.h \
                 $(INCLUDE_DIR)/crontab.h \
                 $(INCLUDE_DIR)/lock.h \
                 $(INCLUDE_DIR)/reset.h \
                 $(INCLUDE_DIR)/schedule.h \
                 $(INCLUDE_DIR)/set.h \
                 $(INCLUDE_DIR)/terminate.h \
                 $(INCLUDE_DIR)/unlock.h \
                 $(INCLUDE_DIR)/unoccupied.h