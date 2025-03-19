CXX = g++
CXXFLAGS = -std=c++17 -Wall -mavx -O3 -pthread
LDFLAGS = -pthread

SRCS_COMMON = NetworkMessage.cpp
SRCS_MASTER = master.cpp $(SRCS_COMMON)
SRCS_CLIENT = client.cpp $(SRCS_COMMON)

OBJS_COMMON = $(SRCS_COMMON:.cpp=.o)
OBJS_MASTER = $(SRCS_MASTER:.cpp=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.cpp=.o)

all: master client testbench

master: $(OBJS_MASTER) master_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

client: $(OBJS_CLIENT) client_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

testbench: testbench.o $(OBJS_MASTER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o master client testbench

.PHONY: all clean