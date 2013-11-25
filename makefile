CC=gcc
CFLAGS=-Wall -lm -lusb
LFLAGS=
SOURCES=wsp.c memory.c utils.c wspusb.c output.c weather.c
DISTFILES=$(SOURCES) makefile LICENSE.TXT
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=wsp
BUILD:=$(shell svn info | grep Revision | cut -f2 -d" ")
VERSION:=1.0

all: $(SOURCES) $(EXECUTABLE)

dist:
	zip wsp-v$(VERSION)-b$(BUILD).zip $(DISTFILES) 

.c.o:
	$(CC) -c $(CFLAGS) $<

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

	