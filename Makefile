CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.

SRCS_CORE = integration/network_simulator.cpp \
            Dijkstra/dijkstra.cpp \
            Member2_Trie/trie.cpp \
            Member3_PacketHeap/packet_heap.cpp \
            Member4_RouterQueue_DSU/router_queue.cpp \
            Member4_RouterQueue_DSU/dsu.cpp

OBJS_CORE = $(SRCS_CORE:.cpp=.o)

.PHONY: all clean test demo run

all: simulator demo

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

simulator: main.o $(OBJS_CORE)
	$(CXX) $(CXXFLAGS) $^ -o $@

demo: demo.o $(OBJS_CORE)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: test_integration
	./test_integration

test_integration: tests/test_integration.o $(OBJS_CORE)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: simulator
	./simulator

clean:
	rm -f $(OBJS_CORE) main.o demo.o tests/test_integration.o simulator demo test_integration Member4_RouterQueue_DSU/test_member4
