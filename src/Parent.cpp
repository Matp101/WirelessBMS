// TODO:
//  Add on connection, set cell number (save node_id to eeprom)
//  SDcard logging
//  add algorithm for soc

#ifdef PARENT
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <ACS712.h>
#include <ESPAsyncWebServer.h>


#include "config.h"
#include "adv_config.h"

#if defined(DEBUG_SERIAL) && !defined(PRINTF)
#define PRINTF(...)             \
  {                             \
    Serial.printf(__VA_ARGS__); \
  }
#endif
#if defined(DEBUG_LED) && defined(LED_BUILTIN)
#define LED LED_BUILTIN
#endif

// Prototypes
void sendMessage();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void delayReceivedCallback(uint32_t from, int32_t delay);
void readVoltageAsync();
void webserver();

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;
// Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

bool calc_delay = false;
SimpleList<uint32_t> nodes;

IPAddress getlocalIP();
AsyncWebServer server(80);
//IPAddress myIP(0, 0, 0, 0);
IPAddress myAPIP(IP_ADDRESS);

ACS712 current_sensor(CURRENT_SENSOR_TYPE, CURRENT_SENSOR_PIN);

std::map<uint32_t, String> nodeData; // NodeID -> JSON data

// Parent 
float parent_voltage = 0.0f;
float parent_voltage_nodes = 0.0f;
float parent_current = 0.0f;
float parent_soc = 0.0f;


#ifdef DEBUG_LED
// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;
#endif

void setup()
{
  Serial.begin(115200);

  current_sensor.begin();

#ifdef DEBUG_LED
  pinMode(LED, OUTPUT);
#endif

  // setup wifi mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP | DEBUG); // set before init() so that you can see error messages
  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, 6);
  mesh.onReceive(&receivedCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);
  mesh.setRoot(true);
  mesh.setContainsRoot(true);

  myAPIP = IPAddress(mesh.getAPIP());
  Serial.println("My AP IP is " + myAPIP.toString());

  // userScheduler.addTask( taskSendMessage );
  // taskSendMessage.enable();

#ifdef DEBUG_LED
  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []()
                   {
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
      } });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  PRINTF("Starting Webserver")
  webserver();
  server.begin();

#endif
}

void loop()
{
  // mesh connected
  mesh.update();

  // Blink LED if mesh is connected
#ifdef DEBUG_LED
  digitalWrite(LED, !onFlag);
#endif
}

// Send back to main node all the details about itself, if main node yet then broadcast
/*
If main esp is talking to this node
 info:cell == 0
 info:node_id == NodeId
 set:callibration_offset == int
 set:callibration_scale == int
 set:cell == int
 set:balance == bool
*/
void sendMessage()
{
  DynamicJsonDocument doc(256);
  doc["info"]["serial_number"] = SERIAL_NUMBER;
  doc["info"]["cell"] = 0;
  // doc["info"]["node_id"] =

  String jsonString;
  serializeJson(doc, jsonString);

  mesh.sendBroadcast(jsonString);
  // mesh.sendSingle(node, jsonString);
  if (calc_delay)
  {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end())
    {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }

#if DEBUG_SERIAL >= 1
  PRINTF("Sending message: %s\n", jsonString.c_str());
#endif
}

/*
  doc["info"]["serial_number"] = SERIAL_NUMBER;
  doc["info"]["cell"] = cell_num;
  doc["info"]["node_id"] = mesh.getNodeId();
  doc["info"]["free_mem"] = ESP.getFreeHeap();
  doc["cell"]["voltage"] = cell_voltage.getLastVoltage();   // make sure to get last voltage rather than update current due to passive balancing
  doc["cell"]["voltage_raw"] = cell_voltage.readRaw();
  doc["cell"]["voltage_calibration_offset"] = cell_voltage.getCalibrationOffset();
  doc["cell"]["voltage_calibration_scale"] = cell_voltage.getCalibrationScale();
  doc["balance"]["state"] = balancSer.getState();
*/
void receivedCallback(uint32_t from, String &msg)
{

#if DEBUG_SERIAL >= 2
  PRINTF("Received from %u msg=%s\n", from, msg.c_str());
#endif
  nodeData[from] = msg;
}

void newConnectionCallback(uint32_t nodeId)
{
#ifdef DEBUG_LED
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
#endif

#if DEBUG_SERIAL >= 1
  PRINTF("=================\nNew Connection\n");
  PRINTF("nodeId = %u\n", nodeId);
  PRINTF("%s\n", mesh.subConnectionJson(true).c_str());
  PRINTF("=================\n");
#endif
}

void changedConnectionCallback()
{

  nodes = mesh.getNodeList();

#ifdef DEBUG_LED
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
#endif

#if DEBUG_SERIAL >= 1
  // List all nodes
  PRINTF("=================\nChanged connections\n");
  PRINTF("Num nodes: %d\n", nodes.size());
  PRINTF("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end())
  {
    PRINTF(" %u", *node);
    node++;
  }
  PRINTF("\n=================\n");
#endif

  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset)
{
#if DEBUG_SERIAL >= 1
  PRINTF("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
#endif
}

void delayReceivedCallback(uint32_t from, int32_t delay)
{
#if DEBUG_SERIAL >= 1
  PRINTF("Delay to node %u is %d us\n", from, delay);
#endif
}

float getVoltageNodes(){
  parent_voltage_nodes = 0.0f;
  for (auto &node : nodeData){
    //deserialize json and get voltage and add them all
    DynamicJsonDocument doc(256);
    deserializeJson(doc, node.second);
    parent_voltage_nodes += doc["cell"]["v"].as<float>();
  }
  return parent_voltage_nodes;
}

//TODO
float getVoltageRead(){
  parent_voltage = 0.0f;
  return parent_voltage;
}

float getCurrent(){
  parent_current = current_sensor.getCurrentDC();
  return parent_current;
}

//TODO
float getSOC(){
  parent_soc = 0.0f;
  return parent_soc;
}

int calibrateCurrent(){
  return current_sensor.calibrate();
}

//==========================================================================================
const PROGMEM char* WEBSITE_HTML = R"html(
<html>

<head>
    <script>
        function updateNodesTable(nodes) {
            let tableContent = '';
            nodes.forEach(node => {
                tableContent += `<tr>
                    <td>${node.cell}</td>
                    <td>${node.id}</td>
                    <td>${node.sn}</td>
                    <td>${node.voltage}</td>
                    <td>${node.voltageRaw}</td>
                    <td>${node.state}</td>
                    <td>${node.calibrationOffset}</td>
                    <td>${node.calibrationScale}</td>
                    <td>${node.freeHeapMemory}</td>
                </tr>`;
            });
            document.getElementById('nodes-table').innerHTML = tableContent;
        }

        function fetchNodesData() {
            fetch('/getNodes')
                .then(response => response.json())
                .then(data => {
                    updateNodesTable(data.nodes);
                });
        }

        function toggleSDwrite() {
            fetch('/toggleSD')
                .then(response => response.json())
                .then(data => {
                    const sdDiv = document.getElementById('sd-color');
                    if (data.sdStatus) {
                        sdDiv.style.backgroundColor = 'green';
                        sdDiv.innerHTML = '<p style="text-align: center; justify-content: center;">Writing to sd output</p>';
                    } else {
                        sdDiv.style.backgroundColor = 'red';
                        sdDiv.innerHTML = '<p style="text-align: center; justify-content: center;">Not currently writing to sd output</p>';
                    }
                });
          }

          function updateParent(){
              fetch('/parent')
                  .then(response => response.json())
                  .then(data => {
                      // Structure is {v: voltage, c: current, s: soc, vn: voltageNodes}
                      document.getElementById('p_v').textContent = data.v;
                      document.getElementById('p_vn').textContent = data.vn;
                      document.getElementById('p_c').textContent = data.c;
                      document.getElementById('p_s').textContent = data.s;
                  });
          }

          function calibrateCurrent(){
            fetch('/calibrateCurrent');
          }

        // Fetch nodes data on page load
        window.onload = fetchNodesData;
    </script>

    <style>
        table,
        th,
        td {
            border: 1px solid black;
            border-collapse: collapse;
        }
    </style>
</head>

<body>
    <div id="sd-color" style="width: 100%; height: 20px; background-color: red;">
        <p style="text-align: center; justify-content: center;">Not currently writing to sd output</p>
    </div>
    <div id="parent">
        <h2>Parent</h2>
        <table cellpadding="5">
            <thead>
                <tr>
                    <th><strong>Voltage (V)</strong></th>
                    <th><strong>Voltage Nodes(V)</strong></th>
                    <th><strong>Current (mah)</strong></th>
                    <th><strong>SOC</strong></th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <th id="p_v">0</th>
                    <th id="p_vn">0</th>
                    <th id="p_c">0</th>
                    <th id="p_s">0</th>
                </tr>
            </tbody>
        </table>
        <div id="sd" style="display: block ruby;">
            <button id="sdbutton" class="button" onclick="calibrateCurrent()">Calibrate</button>
            <p id="sd-status">Calibrate Current. When calibrating, make sure <strong>NO</strong> load is outputing</p>
        </div>
        <br>
        <div id="sd" style="display: block ruby;">
            <button id="sdbutton" class="button" onclick="toggleSDwrite()">SD Card write</button>
            <p id="sd-status">SD card not found</p>
        </div>
    </div>

    <div id="nodes-info">
        <h2>Nodes</h2>
        <table cellpadding="5">
            <thead>
                <tr>
                    <th>Cell</th>
                    <th>ID</th>
                    <th>SN</th>
                    <th>Voltage (V)</th>
                    <th>Voltage Raw</th>
                    <th>State</th>
                    <th>Calibration Offset</th>
                    <th>Calibration Scale</th>
                    <th>Free Heap Memory</th>
                </tr>
            </thead>
            <tbody id="nodes-table">
                <!-- Filled dynamically via JavaScript -->
            </tbody>
        </table>
    </div>
</body>

</html>
)html";

void webserver() {
    // Endpoint to serve the main website
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", WEBSITE_HTML);
    });

    // Endpoint to return nodes data
    server.on("/getNodes", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray nodesArray = doc.createNestedArray("nodes");

        for (auto &pair : nodeData) {
            DynamicJsonDocument nodeDoc(256);
            deserializeJson(nodeDoc, pair.second);
            nodesArray.add(nodeDoc.as<JsonObject>());
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Endpoint to toggle SD write status (Placeholder for now)
    server.on("/toggleSD", HTTP_GET, [](AsyncWebServerRequest *request) {
        // TODO: Implement actual SD toggling logic here.
        static bool sdStatus = false;
        sdStatus = !sdStatus;
        String response = String("{\"sdStatus\":") + (sdStatus ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    });


    server.on("/parent", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = String("{\"v\":") + getVoltageRead() + ",\"c\":" + getCurrent() + ",\"s\":" + getSOC() + ",\"vn\":" + getVoltageNodes() + "}";
        request->send(200, "application/json", response);
    });

    server.on("/calibrateCurrent", [](AsyncWebServerRequest *request) {
      String response = String(calibrateCurrent());
      request->send(200, "application/json", response);
    });

    server.begin();
}


#endif // MASTER