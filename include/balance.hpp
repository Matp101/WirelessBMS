#ifndef BALANCE_H_
#define BALANCE_H_
#include <Arduino.h>

//==============================================================================
// Editable settings

// DEBUG_VD: 0 = off, 1 = basic, 2 = verbose
#ifndef DEBUG_VD
#define DEBUG_VD 2
#endif

//==============================================================================



#if defined(ESP8266) || defined(ESP32)
#include <EEPROM.h>
#endif
#ifndef DEBUG_VD
#define DEBUG_VD 0
#endif
#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif

class Balancer
{
private:
    int _pin;
    bool _inverted;
    bool _lastState;

public:
    Balancer(int pin, bool inverted = false)
    {
        this->_pin = pin;
        this->_inverted = inverted;
        this->_lastState = false;
    };

    void begin() { pinMode(this->_pin, OUTPUT); };

    // Turn on the balancer
    void on() { digitalWrite(this->_pin, this->_inverted ? LOW : HIGH); _lastState = true; };

    // Turn off the balancer
    void off() { digitalWrite(this->_pin, this->_inverted ? HIGH : LOW); _lastState = false; };

    // Toggle the balancer
    void toggle() { if (_lastState) off(); else on(); };

    // Get the state of the balancer
    bool getState() { return _lastState; };

    // Set the state of the balancer
    void setState(bool state) { if (state) on(); else off(); };
};

#endif // BALANCE_H_