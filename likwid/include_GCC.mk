CC  = gcc
FC  = gfortran
#FC  = ifort
AS  = as
AR  = ar
PAS = ./perl/AsmGen.pl 
GEN_PAS = ./perl/generatePas.pl 
GEN_GROUPS = ./perl/generateGroups.pl 

ANSI_CFLAGS  = -ansi
ANSI_CFLAGS += -std=c99
#ANSI_CFLAGS += -pedantic
#ANSI_CFLAGS += -Wextra
#ANSI_CFLAGS += -Wall

CFLAGS   =  -O1 -Wno-format
FCFLAGS  = -J ./  -fsyntax-only
#FCFLAGS  = -module ./ 
ASFLAGS  = 
CPPFLAGS =
LFLAGS   =  -pthread

SHARED_CFLAGS = -fpic
SHARED_LFLAGS = -shared

DEFINES  = -D_GNU_SOURCE
DEFINES  += -DMAX_NUM_THREADS=128
DEFINES  += -DPAGE_ALIGNMENT=4096
#enable this option to build likwid-bench with marker API for likwid-perfCtr
#DEFINES  += -DPERFMON

INCLUDES =
LIBS     =


