CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

SRC = src/Server.cpp src/ThreadPool.cpp
TARGET = server

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

tsan: CXXFLAGS = -std=c++17 -Wall -Wextra -g -fsanitize=thread -pthread
tsan: clean $(TARGET)

asan: CXXFLAGS = -std=c++17 -Wall -Wextra -g -fsanitize=address -pthread
asan: clean $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean tsan asan