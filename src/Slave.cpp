//TOOD: EEPROM for calibration values and cell number

//\#define SLAVE
#ifdef SLAVE
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#include "config.h"
#include "adv_config.h"
#include "voltage_divider.hpp"
#include "balance.hpp"


#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif
#if defined(DEBUG_LED) && defined(LED_BUILTIN)
#define LED LED_BUILTIN
#endif



// Prototypes
void sendMessage(); 
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);
void readVoltageAsync();

Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

//Task taskReadVoltage(TASK_SECOND * 1, TASK_FOREVER, &readVoltageAsync);

#ifdef DEBUG_LED
// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;
#endif

VoltageDivider cell_voltage(BATTERY_VOLTAGE_PIN, BATTERY_VOLTAGE_MAX, BATTERY_VOLTAGE_CALIBRATION_OFFSET, BATTERY_VOLTAGE_CALIBRATION_SCALE, VOLTAGE_DIVIDER_EEPROM_ADDRESS);
Balancer balancer(BALANCER_PIN, BALANCER_INVERTED);

uint32_t main_node = 0;

void setup() {
  Serial.begin(115200);
  
  cell_voltage.begin();
  cell_voltage.setAnalogClockDivider(ADC_CLOCK_DIV_16); //ONLY FOR ESP32

  balancer.begin();
  
#ifdef DEBUG_LED
  pinMode(LED, OUTPUT);
#endif

  //setup wifi mesh
  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages
	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, CHANNEL);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);
  mesh.setContainsRoot(false);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

#ifdef DEBUG_LED
  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
      // If on, switch off, else switch on
      if (onFlag)
        onFlag = false;
      else
        onFlag = true;
      blinkNoNodes.delay(BLINK_DURATION);

      if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
        // Calculate delay based on current mesh time and BLINK_PERIOD
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(BLINK_PERIOD - 
            (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
      }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();
#endif
}

void loop() {
  // mesh connected
  mesh.update();

  // Read the battery voltage and turn on balancer if needed
  readVoltageAsync();

  //Blink LED if mesh is connected
#ifdef DEBUG_LED
  digitalWrite(LED, !onFlag);
#endif

}

bool isConnectedToAnyNode() {
    return !mesh.getNodeList().empty();
}

// Send back to main node all the details about itself, if main node yet then broadcast
void sendMessage() {
  if (!isConnectedToAnyNode() ) {  // Check if the device is connected
    return;  // If not connected, exit the function without sending
  }
  DynamicJsonDocument doc(256);
  doc["info"]["cell"] = cell_num;
  doc["info"]["id"] = mesh.getNodeId();
  doc["info"]["sn"] = SERIAL_NUMBER;
  doc["cell"]["v"] = cell_voltage.getLastVoltage();   // make sure to get last voltage rather than update current due to passive balancing
  doc["cell"]["v_raw"] = cell_voltage.readRaw();
  doc["bal"]["state"] = balancer.getState();
  doc["cell"]["cal_offset"] = cell_voltage.getCalibrationOffset();
  doc["cell"]["cal_scale"] = cell_voltage.getCalibrationScale();
  doc["info"]["free_mem"] = ESP.getFreeHeap();  
  String jsonString;
  serializeJson(doc, jsonString);  

  if (main_node == 0)
    mesh.sendBroadcast(jsonString);
  else
    mesh.sendSingle(main_node, jsonString);
  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }

#if DEBUG_SERIAL >= 1
  PRINTF("Sending message: %s\n", jsonString.c_str());
#endif
}

// If main esp is talking to this node
//  info:cell == 0
//  info:node_id == NodeId
//  set:cal_offset == int
//  set:cal_scale == int
//  set:cell == int
//  set:balance == bool
void receivedCallback(uint32_t from, String & msg) {

#if DEBUG_SERIAL >= 2
  PRINTF("Received from %u msg=%s\n", from, msg.c_str());
#endif

  if (from != main_node && main_node != 0)
    return;

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
#if DEBUG_SERIAL >= 1
    PRINTF("deserializeJson() failed: %s\n", error.c_str());
#endif
    return;
  }

  if (main_node != 0){
    //Only talk to main esp and make sure values are present
    if (!doc.containsKey("info") && !doc["info"].containsKey("cell"))
      return;
    if (doc["info"]["cell"] != 0)
        return;
  
    main_node = from;
  }
  
  //if main esp is talking to this node
  if (!doc.containsKey("set")){
    PRINTF("ERROR: No set key\n")
    return;
  }
  if (!doc["set"].containsKey("id")) {
    PRINTF("ERROR: No id key\n")
    return;
  }
  if (doc["set"]["id"] != mesh.getNodeId()){
    PRINTF("ERROR: Not my id\n")
    return;
  }
  PRINTF("=== Changed Settings ===")
  
  //if main esp is talking to this node //set:cell:voltage_calibration_offset
  if (doc["set"].containsKey("cal_offset")){
    float offset = doc["set"]["cal_offset"].as<float>();
    cell_voltage.setCalibrationOffset(offset);
#if DEBUG_SERIAL >= 2
    PRINTF("set cal offset: %f\n", offset)
#endif
  }
  if (doc["set"].containsKey("cal_scale")){
    float scale = doc["set"]["cal_scale"].as<float>();
    cell_voltage.setCalibrationScale(scale);
#if DEBUG_SERIAL >= 2
    PRINTF("set cal scale: %f\n", scale)
#endif
  }
  if (doc["set"].containsKey("cell")){
    cell_num = doc["set"]["cell"].as<int>();
#if DEBUG_SERIAL >= 2
    PRINTF("set cell: %d\n", cell_num);
#endif
  }
  if (doc["set"].containsKey("balance")){
    bool balance = !(doc["set"]["balance"] == "false");
    balancer.setState(balance);
#if DEBUG_SERIAL >= 2
    PRINTF("set balance: %d\n", balance);
#endif
  }
  PRINTF("========================")
  return;
}


void newConnectionCallback(uint32_t nodeId) {
#ifdef DEBUG_LED
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
#endif

#if DEBUG_SERIAL >= 1
  PRINTF("=================\nNew Connection\n");
  PRINTF("nodeId = %u\n", nodeId);
  PRINTF("%s\n", mesh.subConnectionJson(true).c_str());
  PRINTF("=================\n");
#endif
}

void changedConnectionCallback() {
  
  nodes = mesh.getNodeList();

#ifdef DEBUG_LED
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
#endif

#if DEBUG_SERIAL >= 1
  // List all nodes
  PRINTF("=================\nChanged connections\n");
  PRINTF("Num nodes: %d\n", nodes.size());
  PRINTF("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    PRINTF(" %u", *node);
    node++;
  }
  PRINTF("\n=================\n");
#endif

  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
#if DEBUG_SERIAL >= 1  
  PRINTF("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
#endif
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
#if DEBUG_SERIAL >= 1
  PRINTF("Delay to node %u is %d us\n", from, delay);
#endif
}



// ==================== Voltage Read Async ====================

enum VoltageReadState {
  INITIAL,
  DELAY,
  READ,
  RESTORE,
  WAIT
};

VoltageReadState readState = INITIAL;
unsigned long lastTime;
bool balancer_state;
bool threshold_flag;
float voltage = 0.0f;

void readVoltageAsync() {
  unsigned long currentTime = millis();

  switch (readState) {
    // Initial state, set the balancer to off and wait for voltage to settle
    case INITIAL:
      balancer_state = balancer.getState();
      if (balancer_state == true){
        readState = DELAY;
        balancer.setState(false);
      }else {
        readState = READ;
      }
      lastTime = currentTime;
      break;

    // Wait for voltage to settle if balancer was on
    case DELAY:
      if (currentTime - lastTime >= VOLTAGE_SETTLE_DELAY) {
        readState = READ;
      }
      break;

    // Read the voltage and if it is above max or below threshold, turn on balancer
    case READ:
      voltage = cell_voltage.readVoltage();
      if (voltage >= BATTERY_VOLTAGE_MAX || threshold_flag == true){
        if (voltage <= (BATTERY_MAX_THRESHOLD - BATTERY_VOLTAGE_MAX) ) {
          threshold_flag = false;
          balancer_state = false;
        }
        else{
          threshold_flag = true;
          balancer_state = true;
        }
      }
      readState = RESTORE;
      break;

    // Restore the balancer state and wait for the next read
    case RESTORE:
      balancer.setState(balancer_state);
      readState = WAIT;
      break;

    // Wait for the next read
    case WAIT:
      if (currentTime - lastTime >= READ_VOLTAGE_INTERVAL * 1000) {
        readState = INITIAL;
      }
      break;
  }
}
#endif // SLAVE