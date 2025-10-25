#include "Logger.h"

Logger::Logger(std::string path, int waiting_time) {
    int fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(LoggerBuffer));
    shm_buff = (LoggerBuffer*)mmap(nullptr, sizeof(LoggerBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    efd = eventfd(0, EFD_NONBLOCK);

    seconds_to_wait_for_print = waiting_time;
    started = false;
    file_path = path;

    logger_type_to_string[PING] = " PING: ";
    logger_type_to_string[INFO] = " INFO: ";
    logger_type_to_string[WARNING] = " WARNING: ";
    logger_type_to_string[ERROR] = " ERROR: ";
    logger_type_to_string[CRITICAL] = " CRITICAL: ";
    logger_type_to_string[LOGGER] = " LOGGER: ";
}

Logger::~Logger() {
    shm_buff->parent_closed.store(true);
    shm_unlink(SHARED_MEM_NAME);
}

void Logger::start_logger() {
    pid_t pid = fork();
    started = true;

    if (pid == 0) {
        logger_code();
        exit(0);
    }

    push_logger(LOGGER, "New session =============================");
}

void Logger::push_logger(LoggerMessageType type, const char *format, ...) {
    std::ostringstream oss;
    print_time(oss);
    
    if (logger_type_to_string.count(type)) {
        oss << logger_type_to_string[type] << format;
    } else {
        oss << " UNKNOWN: " << format;
    }

    va_list args;
    va_start(args, format);

    print_to_logger(oss.str().c_str(), args);

    va_end(args);
}

void Logger::ping_logger() {
    push_logger(PING, "ping");
}

void Logger::print_time(std::ostringstream &oss) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);   // Windows
#else
    localtime_r(&now_time, &local_tm);   // POSIX
#endif

    oss << "["
        << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
        << "]";
}

void Logger::print_time(std::ostream &os) {
        auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);   // Windows
#else
    localtime_r(&now_time, &local_tm);   // POSIX
#endif

    os << "["
        << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
        << "]";
}

void Logger::report_crash(std::ostream &os) {
    print_time(os);
    os << " LOGGER: PARENT CRASHED\n" << std::endl;
    os.flush();
}

void Logger::report_exit(std::ostream &os) {
    print_time(os);
    os << " LOGGER: PARENT CLOSED SUCCESSFULLY\n" << std::endl;
    os.flush();
}

void Logger::report_silence(std::ostream &os, int how_much) {
    print_time(os);
    os << " LOGGER: nothing for parrent for " << how_much << " second." << std::endl;
    os.flush();
}

void Logger::print_to_logger(const char *format, ...)
{
    if (!started)
        return;

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    std::vector<char> buf(size + 1);
    std::vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);

    buf[size] = '\n';

    uint32_t w = shm_buff->write_index.load();
    for (size_t j = 0; j < buf.size(); ++j)
        shm_buff->buffer[(w + j) % sizeof(shm_buff->buffer)] = buf[j];

    shm_buff->write_index.store((w + buf.size()) % sizeof(shm_buff->buffer));

    eventfd_write(efd, 1);
}

void Logger::logger_code() {
    pollfd pfd = { efd, POLLIN, 0 };

    std::ofstream file(file_path, std::ios::app | std::ios::binary);
    static std::atomic<bool> parent_dead{false};

    signal(SIGINT, SIG_IGN); 
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    signal(SIGHUP, +[](int) {
        parent_dead.store(true);
    });

    // Handle rare race: parent might die before prctl() call completes
    pid_t ppid = getppid();
    if (getppid() != ppid) {
        parent_dead.store(true);
    }

    int waiting_time = 0;
    int ttl = std::min(POLL_TIMEOUT, seconds_to_wait_for_print);
    if (ttl <= 0)
        ttl = POLL_TIMEOUT;

    while (true) {
        if (parent_dead.load()) {
            if (shm_buff->parent_closed.load() == true) {
                report_exit(file);
            } else {
                report_crash(file);
            }

            break;
        }

        int r = poll(&pfd, 1, 1000 * ttl); // 5s timeout
        if (r > 0) {
            uint64_t val;
            eventfd_read(efd, &val);

            // TO DO: write to file
            uint32_t w = shm_buff->write_index.load();
            uint32_t r = shm_buff->read_index.load();

            if (r != w) {
                while (r != w) {
                    file.put(shm_buff->buffer[r]);
                    r = (r + 1) % sizeof(shm_buff->buffer);
                }

                file.flush();
                shm_buff->read_index.store(r);
            }

            waiting_time = 0;

        } else if (r == 0) {
            waiting_time += ttl;

            if (shm_buff->parent_closed.load() == true) {
                report_exit(file);
                break;
            }

            if (getppid() == 1) {
                report_crash(file);
                break;
            }

            if (seconds_to_wait_for_print > 0 && waiting_time >= seconds_to_wait_for_print) {
                report_silence(file, waiting_time);
                waiting_time = 0;
            } else {
                waiting_time = 0;
            }
        }
    }

    shm_unlink(SHARED_MEM_NAME); 
}
