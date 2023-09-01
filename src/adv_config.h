#define BATTERY_VOLTAGE_CALIBRATION_OFFSET 0.0f // Offset to add to voltage
#define BATTERY_VOLTAGE_CALIBRATION_SCALE 1.0f  // 1.0f / (R1 / (R1 + R2))
#define VOLTAGE_DIVIDER_EEPROM_ADDRESS 16       // Address in EEPROM to store calibration data

#define ADC_CLOCK_DIV_16 16             //ONLY FOR ESP32