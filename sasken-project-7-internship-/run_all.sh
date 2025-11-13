#!/bin/bash

# Usage:
#   ./run.sh [roundrobin|least|iphash]
# Default: roundrobin

ALGO=${1:-roundrobin}

echo "🔧 Building backend servers and load balancer..."
g++ -std=c++17 backend_server.cpp -o backend_server -pthread
g++ -std=c++17 load_balancer.cpp -o load_balancer -pthread

echo "🚀 Starting backend servers..."
./backend_server 9001 &
PID1=$!
./backend_server 9002 &
PID2=$!
./backend_server 9003 &
PID3=$!

sleep 1  # give servers a moment to start

echo "🌐 Starting load balancer on port 8080 with algorithm: $ALGO"
./load_balancer $ALGO &
PID_LB=$!

echo "✅ All services started!"
echo "Load balancer running at: http://127.0.0.1:8080"
echo
echo "Backend PIDs: $PID1, $PID2, $PID3"
echo "Load balancer PID: $PID_LB"
echo
echo "👉 To stop everything, run: kill $PID1 $PID2 $PID3 $PID_LB"
