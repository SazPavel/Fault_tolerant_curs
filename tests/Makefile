# compiler
CC=gcc
# directories
IDIR=../include
LDIR=../lib
SRCDIR=.
# flags
NCFLAGS=`ncursesw5-config --cflags`
NCLIBS=`ncursesw5-config --libs`
CFLAGS=$(NCFLAGS) -I$(IDIR)
LDFLAGS= -L$(LDIR) -lcards -lpthread $(NCLIBS)
# target vars
SOURCES=client_test.c server_test.c
TARGET=client_test server_test
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=$(LDIR)/lib$(TARGET).a
# rules
.PHONY: all run clean
default: all run clean
all: $(TARGET)

$(SOURCES):
	@$(CC) $(CFLAGS) -c $@
%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
$(TARGET): %: %.o $(OBJECTS)
	@$(CC) $< $(LDFLAGS) -o $@

run:
	@./client_test
	@./server_test
clean:
	@rm -f client_test
	@rm -f server_test
	@rm -f *.o
