CC=gcc
CFLAGS=-Wall -lm -lusb
SOURCES=wsp.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=wsp

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
.c.o:
	$(CC) $(CFLAGS) $< -o $@

