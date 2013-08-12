MMUTILPATH = ../MMUtil+
#BOOSTPATH = -I/Sprachen/boost
EXE = 
O = .o

CFLAGS = -I$(MMUTILPATH)/include $(BOOSTPATH) -Wall
#LDFLAGS = -lstdc++ -s 
LDFLAGS = -lstdc++ -s -lpthread -lrt

-include Makefile.sub
