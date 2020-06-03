all: dyninstBB_extent

dyninstBB_extent: dyninstBB_extent.cpp blocks.pb.cc refInf.pb.cc utils.cpp
	c++ -g -O2 -std=c++11 -L/usr/local/lib -o dyninstBB_extent dyninstBB_extent.cpp blocks.pb.cc refInf.pb.cc utils.cpp loadInfo.cpp -linstructionAPI -ldyninstAPI -lparseAPI -lsymtabAPI -lglog -lgflags -lprotobuf -lboost_system -lpthread -lcapstone

