#ifndef ASKTUX_SCOPED_TIMER_H
#define ASKTUX_SCOPED_TIMER_H

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

/**
 * ScopedTimer — logs elapsed time (in milliseconds) to stderr when
 * it goes out of scope.  Does nothing unless enabled is set to true.
 *
 * Usage:
 *   ScopedTimer::enabled = true;   // enable all timers
 *   {
 *       ScopedTimer t("my label");
 *       // ... work ...
 *       t.lap("phase 2");           // intermediate report
 *   }                              // auto-report on scope exit
 *
 * When disabled the constructor/destructor are trivial (just a bool
 * check + string move), so it's fine to leave them in production code.
 */
struct ScopedTimer {
    inline static std::atomic<bool> enabled{false};

    std::string label;
    std::chrono::steady_clock::time_point start;
    bool active;

    ScopedTimer(std::string label)
        : label(std::move(label))
        , start(enabled.load() ? std::chrono::steady_clock::now()
                               : std::chrono::steady_clock::time_point{})
        , active(enabled.load()) {}

    ~ScopedTimer() {
        if (!active) return;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        std::cerr << "[AskTux TIMER] " << label << ": " << elapsed << " ms"
                  << std::endl;
    }

    /** Manually report elapsed so far and reset the timer. */
    void lap(const std::string& lap_label) {
        if (!active) return;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - start).count();
        std::cerr << "[AskTux TIMER] " << label << " — " << lap_label << ": "
                  << elapsed << " ms" << std::endl;
        start = now;
    }
};

#endif // ASKTUX_SCOPED_TIMER_H
