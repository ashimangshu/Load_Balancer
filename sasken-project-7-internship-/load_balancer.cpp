// g++ -std=c++17 load_balancer.cpp -o load_balancer -pthread
#include <iostream>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <climits>
#include <functional>
#include <atomic>

using namespace std;

enum class LBAlgorithm { ROUND_ROBIN, LEAST_CONNECTIONS, IP_HASH };

class BackendManager {
public:
    vector<pair<string, int>> backend_servers = {
        {"127.0.0.1", 9001},
        {"127.0.0.1", 9002},
        {"127.0.0.1", 9003}
    };

    vector<bool> backend_health;
    vector<int> request_count;
    vector<int> active_connections;
    mutex health_mutex, active_mutex, request_mutex, log_mutex;

    BackendManager() {
        size_t n = backend_servers.size();
        backend_health.resize(n, true);
        request_count.resize(n, 0);
        active_connections.resize(n, 0);
    }

    vector<int> get_healthy_indices() {
        lock_guard<mutex> lock(health_mutex);
        vector<int> indices;
        for (size_t i = 0; i < backend_health.size(); ++i)
            if (backend_health[i]) indices.push_back((int)i);
        return indices;
    }

    void set_health(int index, bool healthy) {
        lock_guard<mutex> lock(health_mutex);
        backend_health[index] = healthy;
    }

    void increment_requests(int index) {
        lock_guard<mutex> lock(request_mutex);
        request_count[index]++;
    }

    void increment_active(int index) {
        lock_guard<mutex> lock(active_mutex);
        active_connections[index]++;
    }

    void decrement_active(int index) {
        lock_guard<mutex> lock(active_mutex);
        active_connections[index]--;
    }

    int get_least_connection_backend(const vector<int>& healthy) {
        lock_guard<mutex> lock(active_mutex);
        int min_conn = INT_MAX, selected = -1;
        for (int idx : healthy) {
            if (active_connections[idx] < min_conn) {
                min_conn = active_connections[idx];
                selected = idx;
            }
        }
        return selected;
    }

    void log_status(ofstream& out) {
        for (size_t i = 0; i < backend_servers.size(); ++i) {
            const auto& [ip, port] = backend_servers[i];
            string status = backend_health[i] ? "healthy" : "unhealthy";
            out << ip << ":" << port << " [" << status << "] Requests: "
                << request_count[i] << " Active: " << active_connections[i] << "\n";
        }
    }
};

static bool set_timeouts(int fd, int sec=2) {
    timeval tv{sec, 0};
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

class HealthChecker {
    BackendManager* manager;
    atomic<bool> running{true};
    thread worker;

    static bool send_all(int fd, const char* buf, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = ::send(fd, buf + sent, len - sent, 0);
            if (n <= 0) return false;
            sent += (size_t)n;
        }
        return true;
    }

    static bool read_some(int fd, string& out) {
        char buf[1024];
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        out.append(buf, buf + n);
        return true;
    }

public:
    HealthChecker(BackendManager* mgr): manager(mgr) {}

    void start() {
        worker = thread([this]() {
            while (running) {
                this_thread::sleep_for(chrono::seconds(5));
                for (size_t i = 0; i < manager->backend_servers.size(); ++i) {
                    const auto& [ip, port] = manager->backend_servers[i];
                    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
                    if (fd == -1) { manager->set_health((int)i, false); continue; }

                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(port);
                    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

                    set_timeouts(fd, 2);
                    bool alive = false;

                    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) != -1) {
                        // Real HTTP health check
                        string req = "GET /health HTTP/1.1\r\nHost: " + ip + "\r\nConnection: close\r\n\r\n";
                        if (send_all(fd, req.c_str(), req.size())) {
                            string resp;
                            // read just some bytes; we only need the status line
                            if (read_some(fd, resp)) {
                                // Check for "HTTP/1.1 200"
                                if (resp.find("HTTP/1.1 200") != string::npos ||
                                    resp.find("HTTP/1.0 200") != string::npos) {
                                    alive = true;
                                }
                            }
                        }
                    }
                    ::close(fd);
                    manager->set_health((int)i, alive);
                }

                ofstream out("status.txt");
                out << "Health Status:\n";
                manager->log_status(out);
            }
        });
    }

    void stop() {
        running = false;
        if (worker.joinable()) worker.join();
    }
};

class LoadBalancer {
    BackendManager* manager;
    LBAlgorithm algorithm;
    mutex rr_mutex;
    size_t rr_index = 0;

public:
    LoadBalancer(BackendManager* mgr, LBAlgorithm algo = LBAlgorithm::ROUND_ROBIN)
        : manager(mgr), algorithm(algo) {}

    int select_backend(const string& client_ip) {
        auto healthy = manager->get_healthy_indices();
        if (healthy.empty()) return -1;

        if (algorithm == LBAlgorithm::ROUND_ROBIN) {
            lock_guard<mutex> lock(rr_mutex);
            int index = healthy[rr_index % healthy.size()];
            rr_index++;
            return index;
        } else if (algorithm == LBAlgorithm::LEAST_CONNECTIONS) {
            return manager->get_least_connection_backend(healthy);
        } else { // IP_HASH
            hash<string> hasher;
            return healthy[hasher(client_ip) % healthy.size()];
        }
    }
};

class ClientHandler {
    BackendManager* manager;
    LoadBalancer* balancer;

    static bool send_all(int fd, const char* buf, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = ::send(fd, buf + sent, len - sent, 0);
            if (n <= 0) return false;
            sent += (size_t)n;
        }
        return true;
    }

    int create_connection(const string& ip, int port) {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) return -1;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
            ::close(fd);
            return -1;
        }
        set_timeouts(fd, 10);
        return fd;
    }

    // Simple HTTP request-response forwarding with timeouts.
    static bool forward_once(int src_fd, int dst_fd, bool& any_bytes) {
        char buffer[8192];
        ssize_t n = ::recv(src_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;
        any_bytes = true;
        return send_all(dst_fd, buffer, (size_t)n);
    }

public:
    ClientHandler(BackendManager* mgr, LoadBalancer* lb)
        : manager(mgr), balancer(lb) {}

    void handle(int client_fd, const string& client_ip) {
        int backend_index = balancer->select_backend(client_ip);
        if (backend_index == -1) {
            string msg = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send_all(client_fd, msg.c_str(), msg.size());
            ::close(client_fd);
            return;
        }

        const auto& [ip, port] = manager->backend_servers[backend_index];
        int backend_fd = create_connection(ip, port);
        if (backend_fd == -1) {
            string msg = "HTTP/1.1 503 Backend Connection Failed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send_all(client_fd, msg.c_str(), msg.size());
            ::close(client_fd);
            return;
        }

        manager->increment_active(backend_index);

        // Forward one HTTP request (whatever client sends right now)
        bool got_req = false;
        if (!forward_once(client_fd, backend_fd, got_req)) {
            ::close(backend_fd);
            ::close(client_fd);
            manager->decrement_active(backend_index);
            return;
        }

        // Read backend response and forward back
        bool got_resp = false;
        if (!forward_once(backend_fd, client_fd, got_resp)) {
            // if backend sends nothing, try to be graceful
        }

        manager->increment_requests(backend_index);
        manager->decrement_active(backend_index);

        ::close(backend_fd);
        ::close(client_fd);
    }
};

int main(int argc, char* argv[]) {
    LBAlgorithm algo = LBAlgorithm::ROUND_ROBIN;
    if (argc > 1) {
        string a = argv[1];
        transform(a.begin(), a.end(), a.begin(), ::tolower);
        if (a == "least") algo = LBAlgorithm::LEAST_CONNECTIONS;
        else if (a == "iphash") algo = LBAlgorithm::IP_HASH;
    }

    BackendManager backendManager;
    LoadBalancer balancer(&backendManager, algo);
    ClientHandler clientHandler(&backendManager, &balancer);
    HealthChecker checker(&backendManager);

    checker.start();

    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(server_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        ::close(server_fd);
        return 1;
    }
    if (::listen(server_fd, 64) == -1) {
        perror("listen failed");
        ::close(server_fd);
        return 1;
    }
    cout << "Load balancer running on port 8080...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = ::accept(server_fd, (sockaddr*)&client_addr, &len);
        if (client_fd == -1) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        std::thread(&ClientHandler::handle, &clientHandler, client_fd, string(client_ip)).detach();
    }

    ::close(server_fd);
    checker.stop();
    return 0;
}
