#ifndef IO_H_
#define IO_H_
#include <Arduino.h>
#include <ArduinoJson.h>

//==============================================================================
// Editable settings

// DEBUG_VD: 0 = off, 1 = basic, 2 = verbose
#ifndef DEBUG_VD
#define DEBUG_VD 2
#endif

//==============================================================================

#ifndef DEBUG_VD
#define DEBUG_VD 0
#endif
#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif

class VoltageDivider
{
private:
	int _pin;
	float _vin; // Input voltage
	float _calibrationOffset;
	float _calibrationScale;
	float _lastVoltage;
#ifdef ESP32
	int _anologClockDivider = 1;
#endif

public:
	// Voltage divider
	// pin: Analog pin to read from
	// vin: Input voltage (default 5.0)
	// calibrationOffset: Offset to add to voltage (default 0.0)
	// calibrationScale: Scale to multiply voltage by (default 1.0)
	VoltageDivider(int pin, float vin = 4.2f, float calibrationOffset = 0.0f, float calibrationScale = 1.0f)
	{
		this->_pin = pin;
		this->_vin = vin;
		this->_calibrationOffset = calibrationOffset;
		this->_calibrationScale = calibrationScale;
	};

	void begin(float calibrationOffset = 0.0f, float calibrationScale = 1.0f) { 
		if (calibrationOffset != this->_calibrationOffset)
			this->_calibrationOffset = calibrationOffset;
		if (calibrationScale != this->_calibrationScale)
			this->_calibrationScale = calibrationScale;
	pinMode(this->_pin, INPUT); 
#if DEBUG_VD > 0
	PRINTF("\n=== VoltageDivider ===\npin: %d\nvin: %f\ncalibrationOffset: %f\ncalibrationScale: %f\n", _pin, _vin, _calibrationOffset, _calibrationScale);
#endif
#if DEBUG_VD > 0
	PRINTF("=== End VoltageDivider ===\n");
#endif
	};

	void setAnalogClockDivider(int divider)
	{
#ifdef ESP32
		this->_anologClockDivider = divider;
		analogSetClockDiv(divider);
#endif
	};

	void clearCalibration()
	{
		this->_calibrationOffset = 0.0;
		this->_calibrationScale = 1.0;
	};

	// Get calibration offset
	float getCalibrationOffset() { return this->_calibrationOffset; };

	// Get calibration scale
	float getCalibrationScale() { return this->_calibrationScale; };

	// Read raw ADC value
	int readRaw() { return analogRead(this->_pin); };

	// Calculate and return the voltage
	float readVoltage()
	{
		int raw = readRaw();
		float scaledVoltage = (float)raw / 1023.0 * _vin;
		float calibratedVoltage = (scaledVoltage + _calibrationOffset) * _calibrationScale;
#if DEBUG_VD >= 2
		PRINTF("READ: Raw: %d, Scaled: %f, Calibrated: %f\n", raw, scaledVoltage, calibratedVoltage);
#endif
		_lastVoltage = calibratedVoltage;
		return calibratedVoltage;
	};

	// Get last voltage
	float getLastVoltage() { return _lastVoltage; };

	// Setter for calibration offset
	bool setCalibrationOffset(float offset)
	{
		if(offset != this->_calibrationOffset){
			this->_calibrationOffset = offset;
			return true;
		}
		return false;
	};

	// Setter for calibration scale
	bool setCalibrationScale(float scale)
	{
		if(scale != this->_calibrationScale){
			this->_calibrationScale = scale;
			return true;
		}
		return false;
	};
};

#endif