CXX = g++
CXXFLAGS = -Wall -pthread -std=c++11

# Targets
all: backend_server load_balancer

backend_server: backend_server.cpp
	$(CXX) $(CXXFLAGS) backend_server.cpp -o backend_server

load_balancer: load_balancer.cpp
	$(CXX) $(CXXFLAGS) load_balancer.cpp -o load_balancer

# Cleanup
clean:
	rm -f backend_server load_balancer
