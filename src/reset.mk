OBJECTS += $(OBJ_DIR)/reset.o

$(OBJ_DIR)/reset.o: $(SOURCE_DIR)/reset.c \
                    $(INCLUDE_DIR)/reset.h \
                    $(INCLUDE_DIR)/common.h