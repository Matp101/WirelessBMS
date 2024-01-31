// TODO:
//  Add on connection, set cell number (save node_id to eeprom, save calibration)
//  SDcard logging
//  https://randomnerdtutorials.com/esp32-microsd-card-arduino/

#ifdef PARENT
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <ACS712.h>
#include <ESPAsyncWebServer.h>
#include <fuelgauge.hpp>

#include <SPI.h>
#include <SD.h>

#include "batteries.h"
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

// Parent 
struct Parent
{
  float voltage;
  float voltage_nodes;
  float current;
  float soc;
  float tt;
};

// Prototypes
void sendMessage();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void delayReceivedCallback(uint32_t from, int32_t delay);
float getVoltageNodes();
float getVoltageRead();
float getCurrent();
float getSOC();
float getTT();
int calibrateCurrent();
void updateValues();
void webserver();

Parent parent;

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

IPAddress getlocalIP();
AsyncWebServer server(80);
IPAddress myAPIP(IP_ADDRESS);

ACS712 current_sensor(CURRENT_SENSOR_TYPE, CURRENT_SENSOR_PIN, 3.3, 4095);
FuelGauge fuel_gauge(BATTERY_NOMINAL_CAPACITY, BATTERY_INTERNAL_RESISTANCE, BATTERY_MAX_VOLTAGE, BATTERY_LOAD_CURRENT, 4);

std::map<uint32_t, String> nodeData; // NodeID -> JSON data

bool fuel_gauge_initialized = false;

#ifdef DEBUG_LED
// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;
#endif

void setup()
{
  Serial.begin(115200);
#ifdef DEBUG_LED
  pinMode(LED, OUTPUT);
#endif

  // setup wifi mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP | DEBUG);
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
  PRINTF("My API IP is %d.%d.%d.%d\n", myAPIP[0], myAPIP[1], myAPIP[2], myAPIP[3]);

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


static unsigned long lastUpdate = 0;
void loop()
{
  // mesh connected
  mesh.update();

  if(!fuel_gauge_initialized && getCurrent() == 0.0f){
    PRINTF("Initializing Fuel Gauge\n");
    fuel_gauge.initialize(getCurrent(), getVoltageNodes(), &parent.soc, &parent.tt);
    fuel_gauge_initialized = true;
  }

  //update current, voltage, soc, tt
  if (millis() - lastUpdate > 1000)
  {
    lastUpdate = millis();
    updateValues();
  }

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
  parent.voltage_nodes = 0.0f;
  for (auto &node : nodeData){
    //deserialize json and get voltage and add them all
    DynamicJsonDocument doc(256);
    deserializeJson(doc, node.second);
    parent.voltage_nodes += doc["cell"]["v"].as<float>();
  }
  return parent.voltage_nodes;
}

//TODO
float getVoltageRead(){
  parent.voltage = 0.0f;
  return parent.voltage;
}

float getCurrent(){
  parent.current = current_sensor.getDC();
  return parent.current;
}

float getSOC(){
  parent.soc = fuel_gauge.getSOC();
  return parent.soc;
}

float getTT(){
  parent.tt = fuel_gauge.getTTS();
  return parent.tt;
}

int calibrateCurrent(){
  fuel_gauge.initialize(0.0f, getVoltageNodes(), &parent.soc, &parent.tt);
  fuel_gauge_initialized = true;
  return current_sensor.autoCalibrate();
}

void updateValues(){
  parent.voltage = getVoltageRead();
  parent.voltage_nodes = getVoltageNodes();
  parent.current = getCurrent();
  if(fuel_gauge_initialized){
    //negate current
    float currentdraw = -parent.current;
    fuel_gauge.update(currentdraw, &parent.soc, &parent.tt);
  }
  //String out = "V:" + String(parent.voltage) + " VN:" + String(parent.voltage_nodes) + " C:" + String(parent.current) + " SOC:" + String(parent.soc) + " TT:" + String(parent.tt) + "\n";
  //PRINTF(out.c_str())
}

//==========================================================================================

const PROGMEM char* WEBSITE_HTML = R"html(
<html>
<head>
    <title>Wireless BMS</title>
    <style>
        table,
        th,
        td {
            border: 1px solid black;
            border-collapse: collapse;
        }

        p {
            margin: 0px;
        }
    </style>
</head>

<body>
    <div id="sd-color" style="width: 100%; height: 20px; background-color: red;">
        <p style="text-align: center; justify-content: center;">Not currently writing to sd output</p>
    </div>
    <div>
        <br>
        <label for="interval">Update interval (seconds):</label>
        <input type="number" id="interval" min="1" value="5">
    </div>
    <div id="parent">
        <h2>Parent</h2>
        <table cellpadding="5">
            <thead>
                <tr>
                    <th><strong>Voltage (V)</strong></th>
                    <th><strong>Voltage Nodes (V)</strong></th>
                    <th><strong>Discharge Current (A)</strong></th>
                    <th><strong>SOC (%)</strong></th>
                    <th><strong>TTS (h)</strong></th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td id="p_v">0</td>
                        <td id="p_vn">0</td>
                        <td id="p_c">0</td>
                        <td id="p_s">0</td>
                        <td id="p_tt">0</td>
                    </tr>
                </tbody>
            </table>
            <br>

            <div id="calibrate" style="display: block ruby;">
                <button id="calibratebutton" class="button" onclick="calibrateCurrent()">Calibrate</button>
                <p id="calibrate-status">Calibrate Current. When calibrating, make sure <strong>NO</strong> load is outputing</p>
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
                        <th>Balance State</th>
                        <th>Calibration Offset</th>
                        <th>Calibration Scale</th>
                        <th>Free Heap Memory</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody id="nodes-table"></tbody>
            </table>
        </div>
    

            <script>
                let isEditing = false;

                function updateNodesTable(nodes) {
                    if (isEditing) return;

                    nodes.sort((a, b) => a.info.cell - b.info.cell);
                    let tableContent = '';
                    nodes.forEach(node => {
                        tableContent += `<tr id="row${node.info.id}">
		            <td>
						<p id="cell-${node.info.id}">${node.info.cell}</p>
		            	<input type="number" value="${node.info.cell}" id="cell-edit-${node.info.id}" style="display:none"/>
					</td>
		            <td>${node.info.id}</td>
		            <td>${node.info.sn}</td>
		            <td>${node.cell.v}</td>
		            <td>${node.cell.v_raw}</td>
		            <td>${node.bal.state}</td>
		            <td>
						<p id="offset-${node.info.id}">${node.cell.cal_offset}</p>
						<input type="number" value="${node.cell.cal_offset}" id="offset-edit-${node.info.id}" style="display:none"/>
					</td>
		            <td>
						<p id="scale-${node.info.id}">${node.cell.cal_scale}</p>
						<input type="number" value="${node.cell.cal_scale}" id="scale-edit-${node.info.id}" style="display:none"/>
					</td>
		            <td>${node.info.free_mem}</td>
					<td>
                        <button id="edit-${node.info.id}" onclick="enableEditing(${node.info.id})">Edit</button>
                        <button id="submit-${node.info.id}" onclick="submitChanges(${node.info.id})" style="display:none;">Submit</button>
                        <button id="cancel-${node.info.id}" onclick="cancelEdit(${node.info.id})" style="display:none;">Cancel</button>
                    </td>
                </tr>`;
                    });
                    document.getElementById('nodes-table').innerHTML = tableContent;

                    updateParent();
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

                // Fetch nodes data on page load
                window.onload = fetchNodesData;

                let currentInterval;

                document.getElementById('interval').addEventListener('change', function () {
                    // Clear the existing interval
                    if (currentInterval) {
                        clearInterval(currentInterval);
                    }

                    // Set the new interval based on the input value
                    const milliseconds = this.value * 1000;
                    currentInterval = setInterval(fetchNodesData, milliseconds);
                    console.log('changed to ' + document.getElementById('interval').value * 1000)
                });

                // Set the initial interval when the page loads
                currentInterval = setInterval(fetchNodesData, document.getElementById('interval').value * 1000);

                function submitChanges(nodeId) {
                    // 1. Grab the current values of the editable cells for a given node.
                    let cellValue = document.getElementById(`cell-edit-${nodeId}`).value;
                    let offsetValue = document.getElementById(`offset-edit-${nodeId}`).value;
                    let scaleValue = document.getElementById(`scale-edit-${nodeId}`).value;

                    // Construct the data payload to send
                    let data = {
                        set: {
                            node_id: nodeId,
                            cell: parseInt(cellValue),
                            callibration_offset: parseFloat(offsetValue),
                            callibration_scale: parseFloat(scaleValue)
                        }
                    };

                    // 2. Send those values to the server
                    fetch('/updateNode', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json',
                        },
                        body: JSON.stringify(data),
                    })
                        .then(response => response.json())
                        .then(data => {
                            // Handle server response
                            console.log(data);
                        })
                        .catch((error) => {
                            console.error('Error:', error);
                        });

                    isEditing = false;
                    fetchNodesData();
                }


                function enableEditing(nodeId) {
                    isEditing = true;
                    let rowToEdit = document.getElementById(`row${nodeId}`);
                    let cells = rowToEdit.querySelectorAll("td");

                    document.getElementById(`edit-${nodeId}`).style.display = 'none';
                    document.getElementById(`submit-${nodeId}`).style.display = 'inline-block';
                    document.getElementById(`cancel-${nodeId}`).style.display = 'inline-block';

                    document.getElementById(`cell-edit-${nodeId}`).style.display = 'inline-block';
                    document.getElementById(`offset-edit-${nodeId}`).style.display = 'inline-block';
                    document.getElementById(`scale-edit-${nodeId}`).style.display = 'inline-block';
                    document.getElementById(`cell-${nodeId}`).style.display = 'none';
                    document.getElementById(`offset-${nodeId}`).style.display = 'none';
                    document.getElementById(`scale-${nodeId}`).style.display = 'none';
                }

                function cancelEdit(nodeId) {
                    isEditing = false;
                    let rowToEdit = document.getElementById(`row${nodeId}`);
                    let cells = rowToEdit.querySelectorAll("td");
                    fetchNodesData();  // Reloads original data
                    document.getElementById(`edit-${nodeId}`).style.display = 'inline-block';
                    document.getElementById(`submit-${nodeId}`).style.display = 'none';
                    document.getElementById(`cancel-${nodeId}`).style.display = 'none';
                }

                function updateParent() {
                    fetch('/parent')
                        .then(response => response.json())
                        .then(data => {
                            // Structure is {v: voltage, c: current, s: soc, vn: voltageNodes}
                            document.getElementById('p_v').textContent = data.v;
                            document.getElementById('p_vn').textContent = data.vn;
                            document.getElementById('p_c').textContent = data.c;
                            document.getElementById('p_s').textContent = data.s*100;
                            document.getElementById('p_tt').textContent = data.tt;
                        });
                }

                function calibrateCurrent() {
                    fetch('/calibrateCurrent');
                }
            </script>
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
        String response = String("{\"v\":") + parent.voltage + ",\"c\":" + parent.current + ",\"s\":" + parent.soc + ",\"tt\":" + parent.tt + ",\"vn\":" + parent.voltage_nodes + "}";
        request->send(200, "application/json", response);
    });

    server.on("/calibrateCurrent", [](AsyncWebServerRequest *request) {
      String response = String(calibrateCurrent());
      request->send(200, "application/json", response);
    });

    server.on(
		"/updateNode", HTTP_POST, [](AsyncWebServerRequest *request)
		{
		},
		NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
		{
    // Only handling the data when the entire payload has been received
    if(index + len == total){
      	// Convert the received data to a readable string.
      	String reqBody = String((char*)data).substring(index, index+len);
        PRINTF("DATA: %s\n", reqBody.c_str());

		//send single message
		//{"set":{"node_id":434116965,"cell":1,"callibration_offset":0,"callibration_scale":1}}
		StaticJsonDocument<256> rec_doc;
		DeserializationError error = deserializeJson(rec_doc, reqBody.c_str());
		if (error) {
			#if DEBUG_SERIAL >= 1
    		PRINTF("deserializeJson() failed: %s\n", error.c_str());
			#endif
    		return;
 	 	}

		DynamicJsonDocument send_doc(256);
		send_doc["info"]["cell"] = 0;
		send_doc["info"]["id"] = mesh.getNodeId();
		send_doc["set"]["id"] = rec_doc["set"]["node_id"];
		send_doc["set"]["cal_offset"] = rec_doc["set"]["callibration_offset"];
		send_doc["set"]["cal_scale"] = rec_doc["set"]["callibration_scale"];
		send_doc["set"]["cell"] = rec_doc["set"]["cell"];
		send_doc["set"]["balance"] = false;
		send_doc["set"]["led"] = false;

		String jsonString;
		serializeJson(send_doc, jsonString);
		uint32_t nodeId = send_doc["set"]["id"].as<uint32_t>();
		mesh.sendSingle(nodeId, jsonString);

		PRINTF("SENDING (%d): %s\n", nodeId, jsonString)
		PRINTF("~~~%s\n", send_doc)
        
    } });

    server.begin();
}


String toJson() {
    // Create a larger JSON document to handle all data
    DynamicJsonDocument doc(4096);

    // Create an object for parent data
    JsonObject parentJson = doc.createNestedObject("parent");
    parentJson["voltage"] = parent.voltage;
    parentJson["current"] = parent.current;
    parentJson["soc"] = parent.soc;
    parentJson["tt"] = parent.tt;
    parentJson["voltage_nodes"] = parent.voltage_nodes;

    // Create an array for nodes data
    JsonArray nodesArray = doc.createNestedArray("nodes");
    for (const auto& pair : nodeData) {
        DynamicJsonDocument nodeDoc(256);
        DeserializationError error = deserializeJson(nodeDoc, pair.second);
        if (!error) {
            nodesArray.add(nodeDoc.as<JsonObject>());
        }
    }

    // Serialize the JSON document to a string
    String response;
    serializeJson(doc, response);
    return response;
}


#endif // MASTER