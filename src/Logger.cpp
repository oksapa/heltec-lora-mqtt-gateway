#include "Logger.h"
#include <time.h>

// Define the global logger object
LoggerClass Logger;

LoggerClass::LoggerClass() {
}

void LoggerClass::addOutput(Print* output) {
    _outputs.push_back(output);
}

size_t LoggerClass::write(uint8_t c) {
    if (_startOfLine && c != '\n' && c != '\r') {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "[%H:%M:%S] ", &timeinfo);
            for (auto output : _outputs) {
                output->print(timeStr);
            }
        } else {
             // Optional: Print a placeholder if time isn't set yet
             // for (auto output : _outputs) output->print("[??:??:??] ");
        }
        _startOfLine = false;
    }

    if (c == '\n') {
        _startOfLine = true;
    }

    for (auto output : _outputs) {
        output->write(c);
    }
    return 1;
}

size_t LoggerClass::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; i++) {
        n += write(buffer[i]);
    }
    return n;
}