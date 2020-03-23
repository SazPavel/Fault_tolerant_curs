# compiler
CC=gcc
# directories
IDIR=./include
LDIR=./lib
SRCDIR=./src
# flags
NCFLAGS=`ncursesw5-config --cflags`
NCLIBS=`ncursesw5-config --libs`
CFLAGS=-Wall $(NCFLAGS) -I$(IDIR)
LDFLAGS= $(NCLIBS) -lpthread
# target vars
SOURCES=cards.c cards_server.c cards_deck.c cards_client.c cards_io.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=$(LDIR)/lib$(TARGET).a
TARGET=cards
# rules
.PHONY: all clean $(TARGET) test
default: $(TARGET) lib clean
all: default

$(SOURCES):
	$(CC) $(CFLAGS) -c $(SRCDIR)/$@
%.o: %.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/$< -o $@
$(TARGET): %: %.o $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
lib: $(OBJECTS)
	ar -rc $(LIBRARY) cards_server.o cards_deck.o cards_client.o cards_io.o
clean:
	rm -f *.o
test:
	@cd tests && $(MAKE)