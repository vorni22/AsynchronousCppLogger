#include <iostream>

#include "Logger.h"

void segfault() {
    int *a = 0;
    *a = 1;
    *a = 2;
    *a = 3;
}

int main() {
    Logger logger("test.txt", -1);
    logger.start_logger();

    logger.ping_logger();

    std::string line;
    while (std::getline(std::cin, line)) {
        logger.push_logger(INFO, line.c_str());

        if (line == "exit")
            break;

        if (line == "segfault") {
            segfault();
        }
    }

    return 0;
}