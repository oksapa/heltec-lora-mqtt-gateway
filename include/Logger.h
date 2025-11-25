#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>

class LoggerClass : public Print {
public:
    LoggerClass();
    void addOutput(Print* output);
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

private:
    std::vector<Print*> _outputs;
    bool _startOfLine = true;
};

// Declare the global logger object
extern LoggerClass Logger;

#endif // LOGGER_H
