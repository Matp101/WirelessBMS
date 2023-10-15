#ifndef ACS712_h
#define ACS712_h

#include <Arduino.h>

#define ADC_SCALE 1023.0
#define VREF 5.0
#define DEFAULT_FREQUENCY 60

enum ACS712_type
{
	ACS712_05B,
	ACS712_20A,
	ACS712_30A
};

class ACS712
{
private:
	int _zero = 512;
	float _sensitivity;
	uint8_t _pin;

public:
	// Setup sensor, type of sensor and pin
	// Sensitivity is mV/A for 20A and 30A module, mV/A for 5A module
	// Type: ACS712_05B, ACS712_20A, ACS712_30A
	ACS712(ACS712_type type, uint8_t pin)
	{
		_pin = pin;

		switch (type)
		{
		case ACS712_05B:
			_sensitivity = 185.0;
			break;
		case ACS712_20A:
			_sensitivity = 100.0;
			break;
		case ACS712_30A:
			_sensitivity = 66.0; 
			break;
		}
	};

	// Setup the sensor
	void begin()
	{
		pinMode(_pin, INPUT);
	};

	// Calibrate zero value
	int calibrate()
	{
		uint16_t acc = 0;
		for (int i = 0; i < 10; i++)
		{
			acc += analogRead(_pin);
		}
		_zero = acc / 10;
		return _zero;
	};

	// Set zero point in ADC counts
	void setZeroPoint(int zeroPoint)
	{
		_zero = zeroPoint;
	};

	// Set sensitivity (mV/A)
	void setSensitivity(float sens)
	{
		_sensitivity = sens;
	};

	// Get Current DC value in mA
	float getCurrentDC()
	{
		int16_t acc = 0;
		for (int i = 0; i < 10; i++)
		{
			acc += analogRead(_pin) - _zero;
		}
		float I = ((float)acc / 10.0) / ADC_SCALE * VREF / _sensitivity;
		return I;
	};

	// Get Current for AC voltage in mA
	float getCurrentAC(uint16_t frequency = DEFAULT_FREQUENCY)
	{
		uint32_t period = 1000000 / frequency;
		uint32_t t_start = micros();

		uint32_t Isum = 0, measurements_count = 0;
		int32_t Inow;

		while (micros() - t_start < period)
		{
			Inow = analogRead(_pin) - _zero;
			Isum += Inow * Inow;
			measurements_count++;
		}

		float Irms = sqrt(Isum / measurements_count) / ADC_SCALE * VREF / _sensitivity;
		return Irms;
	};

};

#endif
