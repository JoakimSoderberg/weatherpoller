CC=gcc
CFLAGS=-Wall -lm -lusb
SOURCES=wsp.c
DISTFILES=$(SOURCES) makefile LICENSE.TXT
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=wsp
BUILD:=$(shell svn info | grep Revision | cut -f2 -d" ")
VERSION:=1.0

all: $(SOURCES) $(EXECUTABLE)

dist:
	zip wsp-v$(VERSION)-b$(BUILD).zip $(DISTFILES) 

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
.c.o:
	$(CC) $(CFLAGS) $< -o $@

