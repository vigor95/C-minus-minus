CXX := clang++-3.5
CXXFLAGS := -O3 -std=c++11

PROG := cmm
SRCS := $(wildcard ./*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
LIBS := -lm $(shell --system-libs --ldflags --libs all)

$(PROG): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ -rdynamic $(OBJS)
