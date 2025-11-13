# 🚀 Advanced Load Balancer System

A high-performance, multi-threaded load balancer implementation in C++17 with intelligent health monitoring, multiple load balancing algorithms, and real-time status tracking.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg)

## ✨ Features

- **🔀 Multiple Load Balancing Algorithms**
  - Round Robin
  - Least Connections
  - IP Hash-based distribution

- **🏥 Intelligent Health Monitoring**
  - Real-time backend server health checks
  - Automatic failover for unhealthy servers
  - Configurable health check intervals

- **⚡ High Performance**
  - Multi-threaded client handling
  - Non-blocking socket operations
  - Efficient connection pooling

- **📊 Real-time Monitoring**
  - Live status tracking via `status.txt`
  - Request count monitoring
  - Active connection tracking

- **🛡️ Robust Error Handling**
  - Graceful degradation
  - Proper HTTP error responses
  - Connection timeout management

## 🏗️ Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Client        │    │   Load Balancer  │    │   Backend       │
│   (Browser/     │───▶│   (Port 8080)    │───▶│   Servers       │
│   Application)  │    │                  │    │   (Ports        │
└─────────────────┘    └──────────────────┘    │   9001-9003)    │
                                               └─────────────────┘
```

## 📁 Project Structure

```
final_project/
├── load_balancer.cpp      # Main load balancer implementation
├── backend_server.cpp      # Backend server implementation
├── Makefile               # Build configuration
├── status.txt             # Real-time status monitoring
└── README.md              # This file
```

## 🚀 Quick Start

### Prerequisites

- **C++17 compatible compiler** (GCC 7+, Clang 5+)
- **POSIX-compliant system** (Linux, macOS)
- **Make** build system

### Build & Run

1. **Clone the repository**
   ```bash
   git clone https://github.com/santu567/sasken-project-7-internship-.git
   cd sasken-project-7-internship-
   ```

2. **Build the project**
   ```bash
   make clean && make
   ```

3. **Start backend servers** (in separate terminals)
   ```bash
   # Terminal 1
   ./backend_server 9001
   
   # Terminal 2  
   ./backend_server 9002
   
   # Terminal 3
   ./backend_server 9003
   ```

4. **Start the load balancer**
   ```bash
   ./load_balancer
   ```

5. **Test the system**
   ```bash
   curl http://localhost:8080/hello
   curl http://localhost:8080/test
   ```

## 🔧 Configuration

### Load Balancing Algorithms

```bash
# Round Robin (default)
./load_balancer

# Least Connections
./load_balancer least

# IP Hash
./load_balancer iphash
```

### Backend Server Configuration

Edit `load_balancer.cpp` to modify backend servers:

```cpp
vector<pair<string, int>> backend_servers = {
    {"127.0.0.1", 9001},
    {"127.0.0.1", 9002},
    {"127.0.0.1", 9003}
};
```

### Health Check Settings

```cpp
// Health check interval (seconds)
this_thread::sleep_for(chrono::seconds(5));

// Connection timeout (seconds)
set_timeouts(fd, 2);
```

## 📊 Monitoring

### Real-time Status

Monitor system health in real-time:

```bash
# View current status
cat status.txt

# Monitor continuously
watch -n 1 cat status.txt
```

### Status File Format

```
Health Status:
127.0.0.1:9001 [healthy] Requests: 15 Active: 2
127.0.0.1:9002 [healthy] Requests: 12 Active: 1
127.0.0.1:9003 [unhealthy] Requests: 8 Active: 0
```

### Process Monitoring

```bash
# Check running processes
ps aux | grep -E "(backend_server|load_balancer)"

# Check listening ports
lsof -i :8080 -i :9001 -i :9002 -i :9003
```

## 🧪 Testing

### Backend Server Testing

Test individual backend servers directly:

```bash
curl http://localhost:9001/hello
curl http://localhost:9002/hello
curl http://localhost:9003/hello
```

### Load Balancer Testing

Test the load balancer:

```bash
# Basic request
curl http://localhost:8080/hello

# Multiple requests to see load distribution
for i in {1..10}; do
    curl -s http://localhost:8080/hello
    echo "Request $i completed"
done
```

### Stress Testing

```bash
# Concurrent requests
for i in {1..50}; do
    curl -s http://localhost:8080/hello &
done
wait
```

## 🔍 Troubleshooting

### Common Issues

1. **Port Already in Use**
   ```bash
   # Kill existing processes
   pkill -f "backend_server"
   pkill -f "load_balancer"
   ```

2. **Connection Reset Errors**
   - Check if all backend servers are running
   - Verify health status in `status.txt`
   - Check firewall settings

3. **Build Errors**
   - Ensure C++17 compiler is installed
   - Check system dependencies

### Debug Mode

Enable debug output by modifying the source code:

```cpp
// Add debug prints in ClientHandler::handle
cout << "Handling request from " << client_ip << endl;
```

## 🏗️ Build System

### Makefile Targets

```bash
make          # Build all targets
make clean    # Clean build artifacts
make backend_server  # Build only backend server
make load_balancer   # Build only load balancer
```

### Compiler Flags

- **C++17 Standard**: `-std=c++17`
- **Optimization**: `-O2`
- **Warnings**: `-Wall -Wextra`
- **Threading**: `-pthread`

## 📈 Performance Characteristics

- **Concurrent Clients**: Supports multiple simultaneous connections
- **Response Time**: Sub-millisecond request routing
- **Throughput**: High-capacity request handling
- **Memory Usage**: Efficient memory management with RAII

## 🔒 Security Considerations

- Input validation and sanitization
- Proper error handling without information leakage
- Connection timeout management
- Resource cleanup on connection termination

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👨‍💻 Author

**Santu Manna**
- GitHub: [@santu567](https://github.com/santu567)
- Project: Sasken Project 7 Internship

## 🙏 Acknowledgments

- C++ Standard Library for robust data structures
- POSIX APIs for cross-platform networking
- Modern C++ features for clean, maintainable code

---

⭐ **Star this repository if you find it helpful!**

🔄 **For updates and issues, check the GitHub repository.**

---

*Built with ❤️ using C++17 and modern software engineering practices.*

