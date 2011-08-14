include  ../../config.mk
TARGET   = likwid-msrD

CC        = gcc
DEFINES   = -D_GNU_SOURCE
CFLAGS   = -O2 -Wall -ansi -pedantic -Wextra -std=c99
CPPFLAGS = $(DEFINES)
INCLUDES = -I../includes
LIBS     =

all: $(TARGET)

$(TARGET): msr-daemon.c
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o ../../$(TARGET) msr-daemon.c

