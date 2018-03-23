LIBS += -lpthread

# MacOS Xにはlibrtが存在しない。
ifneq ($(shell uname),Darwin)
  LIBS += -lrt
endif

OBJECTS += $(OBJ_DIR)/common.o

$(OBJ_DIR)/common.o: $(SOURCE_DIR)/common.c \
                     $(INCLUDE_DIR)/common.h