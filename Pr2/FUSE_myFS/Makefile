OBJDIR := obj
C_FILES := $(wildcard src/*.c)
OBJ_FILES := $(addprefix $(OBJDIR)/,$(notdir $(C_FILES:.c=.o)))

CFLAGS := -g -Wall $(shell pkg-config fuse --cflags) -m64
#LDFLAGS := -lreadline $(shell pkg-config fuse --libs)
LDFLAGS := $(shell pkg-config fuse --libs)
.PHONY: clean test

TARGET = fs-fuse

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) -o $@ $^ $(LDFLAGS)
	
obj/%.o: src/%.c
	mkdir -p $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

test: $(TARGET)
	./test1.sh -m

clean:
	rm -f ./obj/*.o
	rm -f $(TARGET)
