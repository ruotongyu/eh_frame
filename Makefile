all: dyninstBB_extent

CXXFLAGS = -O2 -std=c++11

InstructionCache.o: InstructionCache.cc
	g++ -c $(CXXFLAGS) -o $@ $^ 

livenessAnaEhframe.o: livenessAnaEhframe.cc
	g++ -c $(CXXFLAGS) -o $@ $^ 

dyninstBB_extent: dyninstBB_extent.cpp blocks.pb.cc refInf.pb.cc utils.cpp livenessAnaEhframe.o InstructionCache.o
	g++ $(CXXFLAGS) -o dyninstBB_extent dyninstBB_extent.cpp blocks.pb.cc refInf.pb.cc utils.cpp loadInfo.cpp livenessAnaEhframe.o InstructionCache.o -L/usr/local/lib -lcommon -ldyninstAPI -linstructionAPI -lparseAPI -lsymtabAPI -lglog -lgflags -lprotobuf -lboost_system -lpthread -lcapstone

