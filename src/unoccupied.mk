OBJECTS += $(OBJ_DIR)/unoccupied.o

$(OBJ_DIR)/unoccupied.o: $(SOURCE_DIR)/unoccupied.c \
                         $(INCLUDE_DIR)/unoccupied.h \
                         $(INCLUDE_DIR)/common.h