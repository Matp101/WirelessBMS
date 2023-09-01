
#define DEBUG_SERIAL 2              // 0: off, 1: info, 2: debug
#define DEBUG_LED                   // Comment out to disable LED blinking
#define BLINK_PERIOD    3000        // milliseconds until cycle repeat
#define BLINK_DURATION  100         // milliseconds LED is on for

#define MESH_SSID       "bms"       // Mesh network SSID
#define MESH_PASSWORD   "bmslab123" // Must be at least 8 characters long
#define MESH_PORT       5555        // Mesh port to use

#define BATTERY_VOLTAGE_PIN 0       // Pin to read the battery voltage from
#define BATTERY_VOLTAGE_MAX 4.2f    // Maximum voltage of the battery

#define BALANCER_PIN 2              // Pin to control the balancer
#define BALANCER_INVERTED false     // Set to true if the balancer is active low

#define SERIAL_NUMBER 0x00000000    // 32-bit unsigned integer, 0x00000000 to 0xFFFFFFFF
int cell_num = 0;                   // cell number, 0-indexed

#define UPDATE_INTERVAL 5           // seconds, how often to send data
#define READ_VOLTAGE_INTERVAL 5     // seconds, how often to update the voltage
#define VOLTAGE_SETTLE_DELAY 5000   // milliseconds, how long to wait for voltage to settle once balancer is turned off
