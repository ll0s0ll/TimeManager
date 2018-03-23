OBJECTS += $(OBJ_DIR)/schedule.o

$(OBJ_DIR)/schedule.o: $(SOURCE_DIR)/schedule.c \
                       $(INCLUDE_DIR)/schedule.h \
                       $(INCLUDE_DIR)/common.h