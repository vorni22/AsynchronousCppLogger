#pragma once

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#include <string>
#include <sys/eventfd.h>
#include <poll.h>
#include <atomic>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <signal.h>
#include <functional>
#include <sys/prctl.h>
#include <unordered_map>

#define SHARED_MEM_NAME "/vorni_logger"
#define LOGGER_SIZE 1024 * 1024
#define POLL_TIMEOUT 5

enum LoggerMessageType {
    INFO = 0,
    CRITICAL = 1,
    WARNING = 2,
    ERROR = 3,
    PING = 4,
    LOGGER = 5,
};

struct LoggerBuffer {
    std::atomic<uint32_t> write_index{0};
    std::atomic<uint32_t> read_index{0};
    std::atomic<bool> parent_closed{false};
    char buffer[LOGGER_SIZE];
};

class Logger {
public:
    Logger(std::string path, int waiting_time);
    ~Logger();

    void start_logger();

    void push_logger(LoggerMessageType type, const char* format, ...);
    void ping_logger();

private:
    static void print_time(std::ostringstream &oss);
    static void print_time(std::ostream &os);

    static void report_crash(std::ostream &os);
    static void report_exit(std::ostream &os);
    static void report_silence(std::ostream &os, int how_much);

    void print_to_logger(const char* format, ...);
    void logger_code();

    int seconds_to_wait_for_print;
    bool started;
    std::string file_path;
    int efd;
    LoggerBuffer *shm_buff;

    std::unordered_map<LoggerMessageType, std::string> logger_type_to_string;
};