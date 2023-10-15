#ifndef WEBSITE_H_
#define WEBSITE_H_
#include <Arduino.h>
#include <ArduinoJson.h>

//==============================================================================
// Editable settings

// DEBUG_VD: 0 = off, 1 = basic, 2 = verbose
#ifndef DEBUG_VD
#define DEBUG_VD 2
#endif

//==============================================================================


#endif
#ifndef DEBUG_VD
#define DEBUG_VD 0
#endif
#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif

class Webserver
{
private:
	int _pin;
	float _vin; // Input voltage
	float _calibrationOffset;
	float _calibrationScale;
	int _eepromAddress; // Address in EEPROM to store calibration data
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
	// eepromAddress: Address in EEPROM to store calibration data (default NULL)
	VoltageDivider(int pin, float vin = 4.2, float calibrationOffset = 0.0, float calibrationScale = 1.0, int eepromAddress = -1)
	{
		this->_pin = pin;
		this->_vin = vin;
		this->_eepromAddress = eepromAddress; // No EEPROM address set
		this->_calibrationOffset = calibrationOffset;
		this->_calibrationScale = calibrationScale;
#ifdef EEPROM_ENABLED
		if (eepromAddress != -1)
		{
			loadCalibration();
		}
#endif
	};

	void begin() { 
	pinMode(this->_pin, INPUT); 
#if DEBUG_VD > 0
	PRINTF("=== VoltageDivider ===\npin: %d\nvin: %f\ncalibrationOffset: %f\ncalibrationScale: %f\neepromAddress: %s\n=== End VoltageDivider ===\n", _pin, _vin, _calibrationOffset, _calibrationScale, _eepromAddress == -1 ? "NULL" : String(_eepromAddress).c_str());
#endif
	};

	void setAnalogClockDivider(int divider)
	{
#ifdef ESP32
		this->_anologClockDivider = divider;
		analogSetClockDiv(divider);
#endif
	};

#ifdef EEPROM_ENABLED
	// Set EEPROM address to store calibration data
	void setEEPROMAddress(int address)
	{
		this->_eepromAddress = address;
		saveCalibration();
	};

	// Save calibration data to EEPROM
	void saveCalibration()
	{
		EEPROM.put(this->_eepromAddress, this->_calibrationOffset);
		EEPROM.put(this->_eepromAddress + sizeof(float), this->_calibrationScale);
#if DEBUG_VD >= 2
		PRINTF("Saved calibration data to EEPROM address %d\n", this->_eepromAddress);
#endif
	};

	// Load calibration data from EEPROM
	void loadCalibration()
	{
		EEPROM.get(this->_eepromAddress, this->_calibrationOffset);
		EEPROM.get(this->_eepromAddress + sizeof(float), this->_calibrationScale);
#if DEBUG_VD >= 2
		PRINTF("Loaded calibration data from EEPROM address %d\n", this->_eepromAddress);
#endif
	};
#endif

	void clearCalibration()
	{
		this->_calibrationOffset = 0.0;
		this->_calibrationScale = 1.0;
#ifdef EEPROM_ENABLED
		saveCalibration();
#endif
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
		PRINTF("Raw: %d\nScaled: %f\nCalibrated: %f\n", raw, scaledVoltage, calibratedVoltage);
#endif
		_lastVoltage = calibratedVoltage;
		return calibratedVoltage;
	};

	// Get last voltage
	float getLastVoltage() { return _lastVoltage; };

	// Setter for calibration offset
	void setCalibrationOffset(float offset)
	{
		this->_calibrationOffset = offset;
#ifdef EEPROM_ENABLED
		saveCalibration();
#endif
	};

	// Setter for calibration scale
	void setCalibrationScale(float scale)
	{
		this->_calibrationScale = scale;
#ifdef EEPROM_ENABLED
		saveCalibration();
#endif
	};
};

#endif