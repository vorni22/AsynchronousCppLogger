#include <iostream>

#include "Logger.h"

void segfault() {
    int *a = 0;
    *a = 1;
    *a = 2;
    *a = 3;
}

int main() {
    Loggers::logs("logger1")->start_logger();
    Loggers::logs("logger2")->start_logger();

    Loggers::logs("logger1")->ping_logger();
    Loggers::logs("logger2")->ping_logger();

    std::string line;
    while (std::getline(std::cin, line)) {
        Loggers::logs("logger1")->push_logger(INFO, line.c_str());
        Loggers::logs("logger2")->push_logger(INFO, line.c_str());

        if (line == "exit")
            break;

        if (line == "segfault") {
            segfault();
        }
    }

    Loggers::close_logs();

    return 0;
}