OBJECTS += $(OBJ_DIR)/add.o

$(OBJ_DIR)/add.o: $(SOURCE_DIR)/add.c \
                  $(INCLUDE_DIR)/add.h \
                  $(INCLUDE_DIR)/common.h \
                  $(INCLUDE_DIR)/lock.h \
                  $(INCLUDE_DIR)/unlock.h