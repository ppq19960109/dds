CROSS_COMPILE = #arm-rockchip-linux-gnueabihf-
CC = @echo "GCC $@"; $(CROSS_COMPILE)gcc
CXX = @echo "G++ $@"; $(CROSS_COMPILE)g++
RM = rm -rf
AR = ar -rcs
CP = cp -r
MKDIR = mkdir -p

TOPDIR = ./

SRC_DIRS := $(shell find src -maxdepth 3 -type d)

LINKTOOL_PATH:=liblinktool
ALI_PATH:=libalink
LOG_PATH:=libzlog
HISTORY_PATH:=libhistory

CFLAGS += $(addprefix -I , $(SRC_DIRS))
CFLAGS += -I$(TOPDIR)
CFLAGS += -Wall #-Werror
ifdef DEBUG
CFLAGS += -g -lmcheck -DDEBUG
endif

LDFLAGS += -L$(TOPDIR) -Wl,-rpath,./

LIBS += -Wl,--start-group	\
		-Wl,-Bstatic  \
		-Wl,-Bdynamic -ldl -lm -lpthread -lrt -ldds -lasound \
		-Wl,--end-group

SRC := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
INC:=$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.h))

OBJ += $(SRC:%.c=%.o)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

TARGET := ddsapp
.PHONY : all clean

all: $(TARGET)

$(TARGET) : $(OBJ)
	$(CXX) $^  $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean :
	$(RM) $(TARGET)
	$(RM) $(OBJ)
