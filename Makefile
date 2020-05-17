all: dyninstBB_extent

dyninstBB_extent: dyninstBB_extent.cpp blocks.pb.cc
	c++ -O2 -std=c++11 -L/usr/local/lib -o dyninstBB_extent dyninstBB_extent.cpp blocks.pb.cc -ldyninstAPI -lparseAPI -linstructionAPI -lsymtabAPI -lglog -lgflags -lprotobuf -lboost_system -lpthread -lcapstone
