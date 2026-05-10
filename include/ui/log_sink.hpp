#pragma once
#include <spdlog/sinks/base_sink.h>
#include <deque>
#include <mutex>
#include <string>
#include <chrono>

namespace sip {

struct LogEntry {
    std::string        timestamp;   // HH:MM:SS
    spdlog::level::level_enum level;
    std::string        logger;      // e.g. "Thermal"
    std::string        message;
};

// Thread-safe ring buffer sink — captures last N log lines in memory.
class MemSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit MemSink(size_t capacity = 120) : capacity_(capacity) {}

    // Called from any thread — grabs last `n` entries (newest last).
    std::vector<LogEntry> tail(size_t n = 3) const {
        std::lock_guard lock(entries_mu_);
        size_t start = entries_.size() > n ? entries_.size() - n : 0;
        return {entries_.begin() + start, entries_.end()};
    }

    size_t size() const {
        std::lock_guard lock(entries_mu_);
        return entries_.size();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format timestamp HH:MM:SS
        auto tt  = std::chrono::system_clock::to_time_t(msg.time);
        std::tm  tm{}; localtime_r(&tt, &tm);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                      tm.tm_hour, tm.tm_min, tm.tm_sec);

        // Extract bracketed logger token from the message body, e.g. "[Thermal]"
        std::string raw(msg.payload.data(), msg.payload.size());
        std::string logger_token;
        if (raw.size() > 1 && raw[0] == '[') {
            auto end = raw.find(']');
            if (end != std::string::npos) {
                logger_token = raw.substr(1, end - 1);
                raw = raw.substr(end + 2); // skip "] "
            }
        }

        LogEntry e;
        e.timestamp = buf;
        e.level     = msg.level;
        e.logger    = logger_token.empty() ? std::string(msg.logger_name.data(), msg.logger_name.size()) : logger_token;
        e.message   = raw;

        std::lock_guard lock(entries_mu_);
        entries_.push_back(std::move(e));
        if (entries_.size() > capacity_) entries_.pop_front();
    }

    void flush_() override {}

private:
    size_t                       capacity_;
    mutable std::mutex           entries_mu_;
    std::deque<LogEntry>         entries_;
};

} // namespace sip
