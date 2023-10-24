// Debug
#define DEBUG_SERIAL 2              // 0: off, 1: info, 2: debug
// LED
#define DEBUG_LED                   // Comment out to disable LED blinking
#define BLINK_PERIOD    3000        // milliseconds until cycle repeat
#define BLINK_DURATION  100         // milliseconds LED is on for

#define SERIAL_NUMBER 0x00000000    // 32-bit unsigned integer, 0x00000000 to 0xFFFFFFFF

// MASTER
#define MESH_SSID       "bms"       // Mesh network SSID
#define MESH_PASSWORD   "bmslab123" // Must be at least 8 characters long
#define MESH_PORT       5555        // Mesh port to use
#define CHANNEL         6           // Mesh channel to use
#define   STATION_SSID     "bmslab"
#define   STATION_PASSWORD "bmslabtester"
#define HOSTNAME "HTTP_BRIDGE"
#define IP_ADDRESS 4,3,2,1
// Current Sensor
#define CURRENT_SENSOR_PIN 36            // Pin to read the current sensor from
#define CURRENT_SENSOR_TYPE ACS712_30A // Type of current sensor, ACS712_05B, ACS712_20A, ACS712_30A
// SD Card (SD CARD must be in FAT32 format)
#define SD_CS_PIN 5                // Pin to use for SD card chip select
#define SD_MOSI_PIN 23              // Pin to use for SD card MOSI
#define SD_CLK_PIN 18               // Pin to use for SD card CLK
#define SD_MISO_PIN 19              // Pin to use for SD card MISO

// SLAVE
#define BALANCER_PIN 2              // Pin to control the balancer
#define BALANCER_INVERTED false     // Set to true if the balancer is active low
// Indvidual cell voltage 
#define BATTERY_VOLTAGE_PIN 0       // Pin to read the battery voltage from
#define BATTERY_VOLTAGE_MAX 4.2f    // Maximum voltage of the battery
#define BATTERY_MAX_THRESHOLD 0.05f // Battery has to fall below BATTERY_VOLTAGE_MAX minus this to turn off balancer
// Update interval
#define UPDATE_INTERVAL 1           // seconds, how often to send data
#define READ_VOLTAGE_INTERVAL 1     // seconds, how often to update the voltage
#define VOLTAGE_SETTLE_DELAY 5000   // milliseconds, how long to wait for voltage to settle once balancer is turned off 
// EEPROM
#define EEPROM_SIZE 128
#define EEPROM_START 0
// Information
int cell_num = 0;                   // cell number, 0-indexed