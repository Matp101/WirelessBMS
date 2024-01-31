#define VERSION 1.0f


#define BATTERY_VOLTAGE_CALIBRATION_OFFSET 0.0f // Offset to add to voltage
#define BATTERY_VOLTAGE_CALIBRATION_SCALE 1.0f  // 1.0f / (R1 / (R1 + R2))

#define ADC_CLOCK_DIV_16 16             //ONLY FOR ESP32
//#define SLAVE //INTELESENSE

#define SD_CARD_ENABLE                  // Comment out to disable SD card writing
#define SD_CARD_CS_PIN 5                // Pin to use for SD card chip select
#define SD_CARD_MOSI_PIN 23             // Pin to use for SD card MOSI
#define SD_CARD_CLK_PIN 18              // Pin to use for SD card CLK
#define SD_CARD_MISO_PIN 19             // Pin to use for SD card MISO
#define SD_CARD_UPDATE_INTERVAL 1       // seconds, how often update data
#define SD_CARD_FILE_PREFIX "data_"      // Prefix for the file name
#define SD_CARD_FILE_SUFFIX ".csv"       // Suffix for the file name
#define SD_RETRY_INTERVAL 1000          // milliseconds, how long to wait before retrying to write to SD card