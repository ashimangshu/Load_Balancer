// g++ -std=c++17 backend_server.cpp -o backend_server -pthread
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

static bool set_timeouts(int fd, int sec=5) {
    timeval tv{sec, 0};
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

static bool send_all(int fd, const char* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static bool read_n(int fd, string& out, size_t n) {
    out.reserve(out.size() + n);
    vector<char> buf(4096);
    size_t total = 0;
    while (total < n) {
        size_t want = min(buf.size(), n - total);
        ssize_t r = ::recv(fd, buf.data(), want, 0);
        if (r <= 0) return false;
        out.append(buf.data(), buf.data() + r);
        total += (size_t)r;
    }
    return true;
}

static bool read_until_headers(int fd, string& headers) {
    // Read until we find \r\n\r\n
    string acc;
    vector<char> buf(1024);
    while (true) {
        ssize_t r = ::recv(fd, buf.data(), buf.size(), 0);
        if (r <= 0) return false;
        acc.append(buf.data(), buf.data() + r);
        size_t pos = acc.find("\r\n\r\n");
        if (pos != string::npos) {
            headers = acc.substr(0, pos + 4);
            // put the rest (possible body already received) into headers' tail marker using a delimiter
            // instead of extra buffer, just keep acc beyond headers in headers (we'll parse length and account for it)
            // Better approach: return both, but to keep code short, we'll track remaining separately.
            // We'll store the remainder length in a global static (not ideal) — instead do this:
            // We'll pass remainder back via reference param:
            // Simpler: stash remainder in a global static? Not nice.
            // We'll modify function to return the full acc; caller splits.
            // To avoid complexity, just return acc and let caller split.
            headers = acc; // return full acc; caller splits
            return true;
        }
        if (acc.size() > 64 * 1024) return false; // avoid header abuse
    }
}

struct Request {
    string method;
    string path;
    string headers_raw;
    string body;
    int content_length = 0;
    bool keep_alive = false;
};

static bool parse_request_from_buffer(const string& buf, Request& req, size_t& consumed) {
    size_t hdr_end = buf.find("\r\n\r\n");
    if (hdr_end == string::npos) return false;

    string start_line;
    {
        size_t eol = buf.find("\r\n");
        if (eol == string::npos) return false;
        start_line = buf.substr(0, eol);
        req.headers_raw = buf.substr(0, hdr_end + 4);
    }

    // Parse start line: METHOD SP PATH SP HTTP/1.1
    {
        size_t sp1 = start_line.find(' ');
        size_t sp2 = (sp1 == string::npos) ? string::npos : start_line.find(' ', sp1 + 1);
        if (sp1 == string::npos || sp2 == string::npos) return false;
        req.method = start_line.substr(0, sp1);
        req.path = start_line.substr(sp1 + 1, sp2 - (sp1 + 1));
    }

    // Headers
    string lower = req.headers_raw;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Content-Length
    req.content_length = 0;
    {
        string key = "\r\ncontent-length:";
        size_t p = lower.find(key);
        if (p != string::npos) {
            size_t p2 = lower.find("\r\n", p + 2);
            string val = lower.substr(p + key.size(), p2 - (p + key.size()));
            // trim
            auto l = val.find_first_not_of(" \t");
            auto r = val.find_last_not_of(" \t");
            if (l != string::npos) val = val.substr(l, r - l + 1);
            req.content_length = stoi(val);
        }
    }

    // Connection keep-alive?
    req.keep_alive = (lower.find("\r\nconnection: keep-alive") != string::npos);

    // Body that might already be present in buf
    size_t body_start = hdr_end + 4;
    size_t already = (buf.size() > body_start) ? (buf.size() - body_start) : 0;
    if ((int)already >= req.content_length) {
        req.body.assign(buf.data() + (long)body_start, req.content_length);
        consumed = body_start + req.content_length;
        return true;
    } else {
        // Need more bytes; caller will read the rest
        req.body.assign(buf.data() + (long)body_start, already);
        consumed = body_start + already;
        return false; // indicates more body needed
    }
}

static void handle_client(int client_fd, int listen_port) {
    set_timeouts(client_fd, 5);

    bool done = false;
    while (!done) {
        // Read headers (and possibly some body) into a buffer
        string buf;
        if (!read_until_headers(client_fd, buf)) break;

        Request req;
        size_t consumed = 0;
        bool complete = parse_request_from_buffer(buf, req, consumed);
        if (!complete) {
            // Need to read the remainder of the body
            size_t need = (req.content_length > 0) ? (req.content_length - req.body.size()) : 0;
            if (need > 0) {
                string more;
                if (!read_n(client_fd, more, need)) break;
                req.body += more;
                consumed += need;
            }
        }

        // Route: /health
        if (req.method == "GET" && req.path == "/health") {
            const string body = "OK";
            string resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + body;
            send_all(client_fd, resp.c_str(), resp.size());
            break; // close on health
        }

        // Echo: respond with the body content and the port info
        string echo = "Echo from port: " + to_string(listen_port) + ":\n" + req.body;
        string resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + to_string(echo.size()) + "\r\n" +
            (req.keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n") +
            "\r\n" + echo;

        if (!send_all(client_fd, resp.c_str(), resp.size())) break;

        if (!req.keep_alive) {
            done = true;
        }
    }

    ::close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "usage: ./backend_server <port>\n";
        return 1;
    }
    int port = stoi(argv[1]);

    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(server_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        ::close(server_fd);
        return 1;
    }

    if (::listen(server_fd, 64) == -1) {
        perror("listen");
        ::close(server_fd);
        return 1;
    }

    cout << "Backend server listening on port: " << port << endl;

    while (true) {
        sockaddr_in caddr{};
        socklen_t clen = sizeof(caddr);
        int cfd = ::accept(server_fd, (sockaddr*)&caddr, &clen);
        if (cfd == -1) {
            perror("accept");
            continue;
        }
        std::thread(handle_client, cfd, port).detach();
    }
}
