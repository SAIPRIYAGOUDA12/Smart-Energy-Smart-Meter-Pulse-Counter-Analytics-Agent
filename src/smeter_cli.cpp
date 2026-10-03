// smeter-cli - terminal dashboard client for smeterd (talks HTTP/JSON)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static std::string http_get(const std::string& host, int port, const std::string& path) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &a.sin_addr);
    if (fd < 0 || connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { if (fd >= 0) ::close(fd); return ""; }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    (void)!::send(fd, req.data(), req.size(), 0);
    std::string resp; char buf[4096]; ssize_t n;
    while ((n = ::recv(fd, buf, sizeof buf, 0)) > 0) resp.append(buf, n);
    ::close(fd);
    auto p = resp.find("\r\n\r\n");
    return p == std::string::npos ? "" : resp.substr(p + 4);
}
static double jnum(const std::string& j, const std::string& k) {
    auto p = j.find("\"" + k + "\":");
    return p == std::string::npos ? 0 : std::atof(j.c_str() + p + k.size() + 3);
}
static std::string jstr(const std::string& j, const std::string& k) {
    auto p = j.find("\"" + k + "\":\"");
    if (p == std::string::npos) return "";
    p += k.size() + 4;
    return j.substr(p, j.find('"', p) - p);
}
static std::vector<double> jarr(const std::string& j, const std::string& k) {
    std::vector<double> v;
    auto p = j.find("\"" + k + "\":[");
    if (p == std::string::npos) return v;
    p += k.size() + 4;
    auto e = j.find(']', p);
    const char* s = j.c_str() + p; const char* end = j.c_str() + e;
    while (s < end) { char* nx; double d = std::strtod(s, &nx); if (nx == s) break; v.push_back(d); s = nx; if (*s == ',') ++s; }
    return v;
}
static std::vector<std::string> jevents(const std::string& j) {
    std::vector<std::string> out;
    auto p = j.find("\"events\":[");
    if (p == std::string::npos) return out;
    p += 10;
    auto e = j.find(']', p);
    while (p < e) {
        auto q1 = j.find('"', p); if (q1 == std::string::npos || q1 > e) break;
        auto q2 = j.find('"', q1 + 1);
        out.push_back(j.substr(q1 + 1, q2 - q1 - 1));
        p = q2 + 1;
    }
    return out;
}
static std::string spark(const std::vector<double>& v, size_t width) {
    static const char* blocks[] = {"▁","▂","▃","▄","▅","▆","▇","█"};
    if (v.empty()) return "";
    size_t start = v.size() > width ? v.size() - width : 0;
    double mx = 1; for (size_t i = start; i < v.size(); ++i) mx = std::max(mx, v[i]);
    std::string s;
    for (size_t i = start; i < v.size(); ++i) s += blocks[std::min(7, static_cast<int>(v[i] / mx * 7.999))];
    return s;
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1"; int port = 8099; double interval = 1; bool once = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--host" && i + 1 < argc) host = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (a == "--interval" && i + 1 < argc) interval = std::atof(argv[++i]);
        else if (a == "--once") once = true;
        else { std::cout << "usage: smeter-cli [--host H] [--port P] [--interval S] [--once]\n"; return a == "--help" ? 0 : 2; }
    }
    for (;;) {
        std::string st = http_get(host, port, "/api/status");
        if (st.empty()) { std::cerr << "cannot reach smeterd at " << host << ":" << port << "\n"; return 1; }
        std::string hi = http_get(host, port, "/api/history?n=60");
        if (!once) std::printf("\033[2J\033[H");
        time_t w = static_cast<time_t>(jnum(st, "wall_s")); char tb[32]; std::strftime(tb, sizeof tb, "%H:%M:%S", localtime(&w));
        std::printf("== Smart Meter Agent ==  state: %s   source: %s   meter-time: %s\n",
                    jstr(st, "state").c_str(), jstr(st, "source").c_str(), tb);
        std::printf("Power now : %8.1f W   (smoothed %.1f W, peak %.1f W)\n", jnum(st, "power_w"), jnum(st, "smooth_w"), jnum(st, "peak_w"));
        std::printf("Energy    : %8.3f kWh  (%0.f pulses)\n", jnum(st, "energy_kwh"), jnum(st, "pulses"));
        std::printf("Cost      : %8.2f  @ %.2f /kWh\n", jnum(st, "cost"), jnum(st, "rate"));
        std::printf("Quality   : debounced=%0.f out-of-order=%0.f spikes=%0.f\n", jnum(st, "debounced"), jnum(st, "out_of_order"), jnum(st, "spikes"));
        std::printf("Avg W/min : %s\n", spark(jarr(hi, "avg_w"), 60).c_str());
        std::printf("Events:\n");
        auto ev = jevents(st);
        for (size_t i = ev.size() > 6 ? ev.size() - 6 : 0; i < ev.size(); ++i) std::printf("  %s\n", ev[i].c_str());
        std::fflush(stdout);
        if (once) return 0;
        std::this_thread::sleep_for(std::chrono::duration<double>(interval));
    }
}
