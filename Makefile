# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Iinclude

# Linker flags for OpenSSL
LDFLAGS = -lssl -lcrypto

# Source files
SRCS = src/main.cpp \
       src/RateLimiter.cpp \
       src/ReverseProxy.cpp \
       src/Server.cpp \
       src/SSLClientHandler.cpp \
       src/SSLHandler.cpp \
       src/ThreadPool.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target executable
TARGET = server

# Build rule
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
