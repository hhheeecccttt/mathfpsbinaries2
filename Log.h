#ifndef LOG_CLASS
#define LOG_CLASS

#include <iostream>
#include <streambuf>
#include <string>
#include <deque>
#include <mutex>
#include <algorithm>

// Tee for std::cout: every completed line is kept in a ring buffer so the game can
// draw the console on screen, while still going to the real console.
class LogStreamBuf : public std::streambuf {
public:
    explicit LogStreamBuf(std::streambuf* base) : base(base) {}

    std::deque<std::string> lines;
    std::mutex              mutex;

    // The newest 'max' completed lines, joined with '\n' (oldest first).
    std::string Tail(int max) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string out;
        const int n = (int)lines.size();
        for (int i = std::max(0, n - max); i < n; i++) {
            if (!out.empty()) out += '\n';
            out += lines[i];
        }
        return out;
    }

protected:
    int_type overflow(int_type c) override {
        if (c != traits_type::eof()) {
            const char ch = traits_type::to_char_type(c);
            { std::lock_guard<std::mutex> lock(mutex); Append(ch); }
            base->sputc(ch);
        }
        return c;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (std::streamsize i = 0; i < n; i++) Append(s[i]);
        }
        base->sputn(s, n);
        return n;
    }

private:
    void Append(char c) {
        if (c == '\n') {
            if (!pending.empty()) {
                lines.push_back(pending);
                pending.clear();
                if (lines.size() > 100) lines.pop_front();
            }
        }
        else if (c != '\r') pending += c;
    }

    std::streambuf* base;
    std::string      pending;
};

inline LogStreamBuf* g_logbuf = nullptr;

// Install once, before anything is printed.
inline void InstallConsoleLog() {
    static LogStreamBuf* tee = new LogStreamBuf(std::cout.rdbuf());
    std::cout.rdbuf(tee);
    g_logbuf = tee;
}

#endif
