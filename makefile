CC=g++
CCFLAGS=-std=c++11 -c -O2
LC=g++
LCFLAGS=-std=c++11 -O2
FSFLAG=-lstdc++fs

archiver: archiver.o LZWcomp.o HC.o
	$(LC) $(LCFLAGS) -o archiver archiver.o LZWcomp.o HC.o $(FSFLAG)
archiver.o: archiver.cpp	
	$(CC) $(CCFLAGS) -o archiver.o archiver.cpp
LZWcomp.o: LZWcomp.cpp
	$(CC) $(CCFLAGS) -o LZWcomp.o LZWcomp.cpp
HC.o: HC.cpp
	$(CC) $(CCFLAGS) -o HC.o HC.cpp

clean: 
	rm -f *.o archiver
