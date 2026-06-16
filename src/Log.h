#ifndef ASKTUX_LOG_H
#define ASKTUX_LOG_H

#include <atomic>
#include <iostream>

/**
 * Log — conditional debug logging.
 *
 * All calls compile to a single flag check when debug is disabled.
 * Usage:
 *   Log::dbg() << "value: " << x << std::endl;
 *   Log::dbg() << "complex: " << some_func() << std::endl;
 *
 * Arguments are still evaluated when debug is off (the bool check
 * is fast and the stream discards everything), so avoid heavy
 * computation in log expressions.
 */
struct DebugStream {
    bool active_;
    explicit DebugStream(bool active) : active_(active) {}

    template <typename T>
    DebugStream& operator<<(T&& val) {
        if (active_) std::cerr << std::forward<T>(val);
        return *this;
    }

    // Handle std::endl and similar manipulators.
    DebugStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        if (active_) manip(std::cerr);
        return *this;
    }
};

struct Log {
    inline static std::atomic<bool> debug{false};

    static void set_debug(bool enabled) { debug.store(enabled); }
    static bool is_debug()              { return debug.load(); }
    static DebugStream dbg()            { return DebugStream(debug.load()); }
};

#endif // ASKTUX_LOG_H
