#ifndef ACS712_h
#define ACS712_h

#include <Arduino.h>

#define ACS712_05B_SENSITIVITY 185 // mV/A
#define ACS712_20A_SENSITIVITY 100 // mV/A
#define ACS712_30A_SENSITIVITY 66  // mV/A

enum ACS712Type
{
	ACS712_05B = 0,
	ACS712_20A = 1,
	ACS712_30A = 2,
	ACS712_N = 3
};

class ACS712
{
public:
	ACS712(ACS712Type _type, uint8_t _pin, float _adc_ref = 5, int _adc_fs = 1023)
	{
		type = _type;
		adc_fs = _adc_fs;

		// Define sensor pin
		ACS712_pin = _pin;

		// Define sensor offset
		offset = _adc_fs / 2.0;

		// Define sensor sensitivity according to the sensor type.
		inv_sensitivity = 1000.0 / float(sensor_sen[_type]); // V/A -> A/V

		results_adjuster = _adc_ref / float(_adc_fs); // V/ADC
		results_adjuster *= inv_sensitivity;				 // V/ADC x A/V -> A/ADC
	}

	float autoCalibrate()
	{
		int _adc = 0, _sample = 0;
		while (_sample < 10)
		{
			_adc += analogRead(ACS712_pin);
			_sample++;
		}

		offset = float(_adc) * 0.1; // average of 10 samples
		return offset;
	}

	float getDC(int _count = 10)
	{
		int _adc = 0, _sample = 0;
		while (_sample < _count)
		{
			_adc += analogRead(ACS712_pin);
			_sample++;
		}

		float _adc_avg = (float(_adc) / float(_count)) - offset; // average of 10 samples and remove offset
		float _current = _adc_avg * results_adjuster;			 // ADC x A/ADC -> A
		return _current;
	}

	float getOffset() { return offset; }

	float getSensitivity() { return 1000.0 / inv_sensitivity; }

	void setOffset(float _offset) { offset = _offset; }

	void setSensitivity(float _sen)
	{
		results_adjuster /= inv_sensitivity; // A/ADC / A/V -> V/ADC
		inv_sensitivity = 1000.0 / _sen;	 // V/A -> A/V
		results_adjuster *= inv_sensitivity; // V/ADC x A/V -> A/ADC
	};

	void reset()
	{
		float _sen = float(sensor_sen[type]);
		setSensitivity(_sen);
		offset = adc_fs / 2.0;
	};

private:
	uint8_t sensor_sen[ACS712_N] = {ACS712_05B_SENSITIVITY,
									  ACS712_20A_SENSITIVITY,
									  ACS712_30A_SENSITIVITY};
	float inv_sensitivity;
	float results_adjuster;
	float offset;
	int adc_fs;
	uint8_t ACS712_pin;
	ACS712Type type;
};

#endif
