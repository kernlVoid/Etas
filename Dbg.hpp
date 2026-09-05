#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <fstream>
#include <iostream>  
#include <iomanip>   
#include <atomic>    
#include <mutex>     
#include <ctime>

namespace Etas {

class Dbg {
public:
    // ERROR is defined as a Windows macro, so use a non-conflicting enumerator.
    enum class LogLevel { INFO, WARNING, ERR };

    // Thread-safe Meyers Singleton (Guaranteed by C++11 standard)
    static Dbg& instance() {
        static Dbg instance;
        return instance;
    }

    // Disables file logging entirely (Thread-safe)
    void disable_file_logging() noexcept {
        should_log_to_file.store(false, std::memory_order_relaxed);
    }

    // Enables file logging (Thread-safe, default behavior)
    void enable_file_logging() noexcept {
        should_log_to_file.store(true, std::memory_order_relaxed);
    }

    // Ultra-fast and ultra-safe logging method
    void log(LogLevel level, std::string_view function_name, std::string_view message) {
        std::string_view level_str;
        switch (level) {
            case LogLevel::INFO:    level_str = "[Info] ";    break;
            case LogLevel::WARNING: level_str = "[Warning] "; break;
            case LogLevel::ERR:     level_str = "[Error] ";   break;
        }

        // Thread-safe timestamp generation
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm time_buf{};
        
#if defined(_WIN32) || defined(_WIN64)
    if (const std::tm* local_time = std::localtime(&in_time_t)) {
        time_buf = *local_time;
    }
#else
        localtime_r(&in_time_t, &time_buf); // POSIX thread-safe variant
#endif

        // Build log line performantly via stringstream (avoids slow string concatenations)
        std::stringstream ss;
        ss << std::put_time(&time_buf, "%Y-%m-%d %H:%M:%S") 
           << " " << level_str 
           << "in " << function_name 
           << ": " << message << "\n";
        
        const std::string log_line = ss.str();

        // Check file logging state with atomic efficiency before locking
        const bool write_to_file = should_log_to_file.load(std::memory_order_relaxed);

        // --- CRITICAL SECTION (Kept as short as possible) ---
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            
            // 1. Output to standard error console
            std::cerr << log_line;

            // 2. Output to pre-opened file stream if enabled
            if (write_to_file && log_file.is_open()) [[likely]] {
                log_file << log_line;
                log_file.flush(); // Ensures data survives immediate application crashes
            }
        }
    }

    ~Dbg() {
        if (log_file.is_open()) {
            log_file.flush(); 
            log_file.close(); 
        }
    }

private:
    std::mutex log_mutex;
    std::ofstream log_file;
    std::atomic<bool> should_log_to_file{true}; 

    // Private Constructor: File descriptor stays open to maximize I/O throughput
    Dbg() {
        // Safe relative path: No folder creation, creates file right next to the executable
        // @attention if it dont works get the full path name.
        std::string logFilePath = "runtime_debug.log";

        // Open the file stream directly in the current working directory
        log_file.open(logFilePath, std::ios::app);
        
        if (!log_file.is_open()) {
            std::cerr << "[INTERNAL LOG ERROR] Failed to open " << logFilePath << "\n";
        }
    }


    // Enforce strict Singleton safety by deleting all copy/move operations
    Dbg(const Dbg&) = delete;
    Dbg& operator=(const Dbg&) = delete;
    Dbg(Dbg&&) = delete;
    Dbg& operator=(Dbg&&) = delete;
};


} // namespace Etas
