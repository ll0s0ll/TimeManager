OBJECTS += $(OBJ_DIR)/activate.o

$(OBJ_DIR)/activate.o: $(SOURCE_DIR)/activate.c \
                       $(INCLUDE_DIR)/activate.h \
                       $(INCLUDE_DIR)/common.h \
                       $(INCLUDE_DIR)/lock.h \
                       $(INCLUDE_DIR)/unlock.h