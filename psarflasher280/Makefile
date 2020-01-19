TARGET = updateflasher
OBJS = copy.o main.o libpsardumper/main.o extract_psp/main.o extract_psp/decryptprx/main.o

INCDIR = ./libpasdumper
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR = ./ ./extract_psp/decryptprx ./libpsardumper
LDFLAGS =
LIBS = -lpspreg -lpspvshbridge_2 -lpspipl_update -lpsppower_2 -lpspsemaphore -lpspmesgd_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

