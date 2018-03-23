OBJECTS += $(OBJ_DIR)/autoextend.o

$(OBJ_DIR)/autoextend.o: $(SOURCE_DIR)/autoextend.c \
                         $(INCLUDE_DIR)/autoextend.h \
                         $(INCLUDE_DIR)/activate.h \
                         $(INCLUDE_DIR)/common.h \
                         $(INCLUDE_DIR)/lock.h \
                         $(INCLUDE_DIR)/unlock.h