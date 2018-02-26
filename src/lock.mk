OBJECTS += $(OBJ_DIR)/lock.o

$(OBJ_DIR)/lock.o: $(SOURCE_DIR)/lock.c \
                   $(INCLUDE_DIR)/lock.h \
                   $(INCLUDE_DIR)/common.h