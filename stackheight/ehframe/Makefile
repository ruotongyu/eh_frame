CXX=g++
CXXFLAGS=-g -O2

CXXFLAGS= -O2 -std=c++11
LDFLAGS= -L/usr/local/lib -lparseAPI -linstructionAPI -lsymtabAPI -ldyninstAPI -lcommon -lglog -lgflags -lprotobuf -lboost_system -lpthread

all: EhframeParser.o

EhframeParser.o: EhframeParser.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $^ 

clean:
	rm *.o 
