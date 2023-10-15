#ifndef SLAVE_DEVICE_HPP
#define SLAVE_DEVICE_HPP

#include <Arduino.h>
#include <ArduinoJson.h>

//==============================================================================
// Editable settings

// DEBUG_SD: 0 = off, 1 = basic, 2 = verbose
#ifndef DEBUG_SD
#define DEBUG_SD 2
#endif

//==============================================================================



#if defined(ESP8266) || defined(ESP32)
#include <EEPROM.h>
#endif
#ifndef DEBUG_SD
#define DEBUG_SD 0
#endif
#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif


class SlaveDevice
{
public:
    SlaveDevice(uint32_t serial_number, uint32_t node_id, int cell_num){
        this->_serial_number = serial_number;
        this->_node_id = node_id;
        this->_cell_num = cell_num;
    };
    
    void updateJson(StaticJsonDocument<256> &doc)
    {
        this->_voltage = doc["cell"]["voltage"];
        this->_voltage_raw = doc["cell"]["voltage_raw"];
        this->_calibration_offset = doc["cell"]["voltage_calibration_offset"];
        this->_calibration_scale = doc["cell"]["voltage_calibration_scale"];
        this->_free_mem = doc["info"]["free_mem"];
        this->_balance = doc["balance"]["state"];
    };

    void update(float voltage, int voltage_raw, float calibration_offset, float calibration_scale, uint32_t free_mem, bool balance)
    {
        this->_voltage = voltage;
        this->_voltage_raw = voltage_raw;
        this->_calibration_offset = calibration_offset;
        this->_calibration_scale = calibration_scale;
        this->_free_mem = free_mem;
        this->_balance = balance;
    };

    float getVoltage() { return _voltage; }
    float getCalibrationOffset() { return _calibration_offset; };
    float getCalibrationScale() { return _calibration_scale; };
    int getCellNum() { return _cell_num; };
    uint32_t getSerialNumber() { return _serial_number; };
    uint32_t getNodeId() { return _node_id; };
    uint32_t getFreeMem() { return _free_mem; };
    bool getBalance() { return _balance; };

    void setCalibrationOffset(float calibration_offset) { _calibration_offset = calibration_offset; };
    void setCalibrationScale(float calibration_scale) { _calibration_scale = calibration_scale; };
    void setCellNum(int cell_num) { _cell_num = cell_num; };

private:
    int _cell_num;
    uint32_t _serial_number;
    uint32_t _node_id;
    uint32_t _free_mem = 0;
    float _voltage = 0.0f;
    int _voltage_raw = 0;
    float _calibration_offset = 0.0f;
    float _calibration_scale = 1.0f;
    bool _balance = false;
};

#endif