CXX=g++
CXXFLAGS=-g -O2

all: EhframeParser.o

EhframeParser.o: EhframeParser.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $^ 

clean:
	rm *.o
