#ifdef MASTER
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>


#include "config.h"
#include "adv_config.h"
#include "voltage_divider.hpp"
#include "balance.hpp"


#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

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

void sendMessage() ; // Prototype
Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

//Task taskReadVoltage(TASK_SECOND * 1, TASK_FOREVER, &readVoltageAsync);

#ifdef DEBUG_LED
// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;
#endif

VoltageDivider cell_voltage(BATTERY_VOLTAGE_PIN, BATTERY_VOLTAGE_MAX, BATTERY_VOLTAGE_CALIBRATION_OFFSET, BATTERY_VOLTAGE_CALIBRATION_SCALE, VOLTAGE_DIVIDER_EEPROM_ADDRESS);
Balancer balancer(BALANCER_PIN, BALANCER_INVERTED);

void setup() {
  Serial.begin(115200);
  
  cell_voltage.begin();
  cell_voltage.setAnalogClockDivider(ADC_CLOCK_DIV_16); //ONLY FOR ESP32

  balancer.begin();
  
#ifdef DEBUG_LED
  pinMode(LED, OUTPUT);
#endif

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

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
  mesh.update();
  readVoltageAsync();
#ifdef DEBUG_LED
  digitalWrite(LED, !onFlag);
#endif
}

void sendMessage() {
  DynamicJsonDocument doc(256);
  doc["info"]["serial_number"] = SERIAL_NUMBER;
  doc["info"]["cell"] = cell_num;
  doc["info"]["node_id"] = mesh.getNodeId();
  doc["info"]["free_mem"] = ESP.getFreeHeap();  
  doc["cell"]["voltage"] = cell_voltage.getLastVoltage();   // make sure to get last voltage rather than update current due to passive balancing
  doc["cell"]["voltage_raw"] = cell_voltage.readRaw();
  doc["cell"]["voltage_calibration_offset"] = cell_voltage.getCalibrationOffset();
  doc["cell"]["voltage_calibration_scale"] = cell_voltage.getCalibrationScale();
  doc["balance"]["state"] = balancer.getState();
  String jsonString;
  serializeJson(doc, jsonString);  

  mesh.sendBroadcast(jsonString);
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
//  set:callibration_offset == int
//  set:callibration_scale == int
//  set:cell == int
//  set:balance == bool
void receivedCallback(uint32_t from, String & msg) {
#if DEBUG_SERIAL >= 2
  PRINTF("startHere: Received from %u msg=%s\n", from, msg.c_str());
#endif

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
#if DEBUG_SERIAL >= 1
    PRINTF("deserializeJson() failed: %s\n", error.c_str());
#endif
    return;
  }

  //Only talk to main esp and make sure values are present
  if (!doc.containsKey("info") && !doc["info"].containsKey("cell"))
    return;
  if (doc["info"]["cell"] != 0)
      return;
  
  //if main esp is talking to this node
  if (!doc.containsKey("set"))
    return;
  if (!doc["set"].containsKey("node_id"))
    return;
  if (doc["set"]["node_id"] != mesh.getNodeId())
    return;
  
  //if main esp is talking to this node //set:cell:voltage_calibration_offset
  if (doc["set"].containsKey("callibration_offset")){
    cell_voltage.setCalibrationOffset(doc["set"]["callibration_offset"]);
  }
  if (doc["set"].containsKey("callibration_scale")){
    cell_voltage.setCalibrationScale(doc["set"]["callibration_scale"]);
  }
  if (doc["set"].containsKey("cell")){
    cell_num = doc["set"]["cell"];
  }
  if (doc["set"].containsKey("balance")){
    balancer.setState(doc["set"]["balance"]);
  }
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
  PRINTF("--> startHere: New Connection, nodeId = %u\n", nodeId);
  PRINTF("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
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
  PRINTF("Changed connections\n");
  PRINTF("Num nodes: %d\n", nodes.size());
  PRINTF("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    PRINTF(" %u", *node);
    node++;
  }
  PRINTF("\n");
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

void readVoltageAsync() {
  unsigned long currentTime = millis();

  switch (readState) {
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

    case DELAY:
      if (currentTime - lastTime >= VOLTAGE_SETTLE_DELAY) {
        readState = READ;
      }
      break;

    case READ:
      cell_voltage.readVoltage();
      readState = RESTORE;
      break;

    case RESTORE:
      balancer.setState(balancer_state);
      readState = INITIAL;
      break;
    case WAIT:
      if (currentTime - lastTime >= READ_VOLTAGE_INTERVAL * 1000) {
        readState = INITIAL;
      }
      break;
  }
}
#endif // MASTER