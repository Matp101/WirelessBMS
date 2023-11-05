
#define MOLLY_BATTERY
#ifdef MOLLY_BATTERY
  #define BATTERY_NOMINAL_VOLTAGE 3.6f        // V
  #define BATTERY_NOMINAL_CAPACITY 4.2f       // Ah

  #define BATTERY_MAX_VOLTAGE 4.2f            // V
  #define BATTERY_MIN_VOLTAGE 3.0f            // V

  #define BATTERY_INTERNAL_RESISTANCE 0.001f  // Ohm
  #define BATTERY_LOAD_CURRENT 0.250f         // A
#endif