MMUTILPATH = ../../MMUtil+
EXE = .exe
O = .o

CFLAGS = -I$(MMUTILPATH) -Wall
LDFLAGS = -lstdc++ -Zcrtdll -Zomf -s

buffer2$(EXE) : fifo.cpp buffer2.cpp $(MMUTILPATH)/FastMutex.cpp $(MMUTILPATH)/Mutex.cpp $(MMUTILPATH)/Event.cpp $(MMUTILPATH)/MutexBase.cpp $(MMUTILPATH)/string.cpp $(MMUTILPATH)/exception.cpp
	gcc $(CFLAGS) -o $@ $+ $(LDFLAGS)
