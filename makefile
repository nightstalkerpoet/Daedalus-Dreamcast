TARGET = main.elf

SOURCES = . src/ src/math/matrix src/math/plane src/math/vector

# Create a list of all files that need to be compiled
ASSRC = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.s))
CSRC = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
CPPSRC = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))

# Create a list of the object files created during compilation
ASOBJ = $(patsubst %.s, %.o, $(ASSRC))
COBJ = $(patsubst %.c, %.o, $(CSRC))
CPPOBJ = $(patsubst %.cpp, %.o, $(CPPSRC))

# You need to put all your object files that should be compiled here. All the
# voodoo work up above should hopefully fill in all of the variables used here
# and make a complete list here... All of that stuff was used directly from your
# old Makefile, for reference.
OBJS = $(ASOBJ) $(COBJ) $(CPPOBJ)

# This shouldn't be needed, unless you're including things out of the src
# directory with #include <file.h> instead of #include "file.h". But, since you
# had this here before effectively, it shouldn't hurt to include it.
KOS_CFLAGS += -I.

# This list of libraries taken directly from your old Makefile, with ones you
# definitely don't need removed from it. I obviously can't comment on whatever
# you have left here.
LIBS = -lparallax -lconio -lm

all: $(TARGET)
 
$(TARGET): $(OBJS)
	kos-c++ -o $(TARGET) $(OBJS) $(LIBS)

clean:
	-rm -f $(OBJS) $(TARGET)

.PHONY: clean all

# This includes all the KOS magic for you, including rules to build .o files out
# of .c, .cpp, and .s files.
include $(KOS_BASE)/Makefile.rules