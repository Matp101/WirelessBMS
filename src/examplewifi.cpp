// TODO:
//  Add website
//  Add on connection, set cell number (save node_id to eeprom)
//  if device disconnects, when reconnect, send info
//  remove
//#define MASTER_EXAMPLE
#ifdef MASTER_EXAMPLE
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <ACS712.h>
#include <ESPAsyncWebServer.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <set>

#include "config.h"
#include "adv_config.h"

#if defined(DEBUG_SERIAL) && !defined(PRINTF)
#define PRINTF(...)                 \
	{                               \
		Serial.printf(__VA_ARGS__); \
	}
#endif

void webserver();
void newNodeCallback(uint32_t);
void receivedCallback(uint32_t, String &);
void cleanupNodes();

struct NodeInfo
{
	String data;			// This is where we store the JSON data.
	unsigned long lastSeen; // This is to store the last time we received a message from this node.
};

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;
Task cleanupTask(TASK_SECOND * 30, TASK_FOREVER, &cleanupNodes);
// Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

std::map<uint32_t, NodeInfo> nodeData; // NodeID -> NodeInfo

bool calc_delay = false;

IPAddress getlocalIP();
AsyncWebServer server(80);
IPAddress myIP(0, 0, 0, 0);
IPAddress myAPIP(0, 0, 0, 0);

void newNodeCallback(uint32_t nodeId)
{
	// Handle new node connection if needed
	PRINTF("New node connected with ID: %u\n", nodeId);
}

void receivedCallback(uint32_t from, String &msg)
{
	PRINTF("Received message from %u: %s\n", from, msg.c_str());
	NodeInfo &node = nodeData[from];
	node.data = msg;
	node.lastSeen = millis();
}

void cleanupNodes()
{
	const unsigned long timeout = 60000; // e.g., 60 seconds timeout to consider a node as disconnected.
	unsigned long now = millis();

	for (auto it = nodeData.begin(); it != nodeData.end();)
	{
		if (now - it->second.lastSeen > timeout)
		{
			it = nodeData.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void setup()
{
	Serial.begin(115200);

	// setup wifi mesh
	mesh.setDebugMsgTypes(ERROR | STARTUP | DEBUG); // set before init() so that you can see error messages
	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, CHANNEL);
	mesh.onReceive(&receivedCallback);

	mesh.stationManual(STATION_SSID, STATION_PASSWORD);
	mesh.setHostname(HOSTNAME);

	// Setup mesh callbacks
	mesh.onNewConnection(&newNodeCallback);
	mesh.onChangedConnections([]()
							  { Serial.printf("Changed connections\n"); });
	//  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
	//  mesh.onNodeDelayReceived(&delayReceivedCallback);
	// mesh.onNoConnections([]() { Serial.println("No connections!"); });

	mesh.onDroppedConnection([](uint32_t nodeId)
							 { Serial.printf("Node %u dropped!\n", nodeId); });

	mesh.setRoot(true);
	mesh.setContainsRoot(true);

	myAPIP = IPAddress(mesh.getAPIP());
	Serial.println("My AP IP is " + myAPIP.toString());

	// Setup webserver
	PRINTF("Starting Webserver\n")
	webserver();
	server.begin();

	// Add the task to the scheduler for cleaning up nodes if disconnected
	userScheduler.addTask(cleanupTask);
	cleanupTask.enable();

	// Print Wifi network name and Password
	PRINTF("Wifi network credentials: %s, %s\n", MESH_SSID, MESH_PASSWORD);
}

void loop()
{
	mesh.update();
}

//==========================================================================================
const PROGMEM char *WEBSITE_HTML = R"html(
<html>

<head>
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
					<th><strong>Current (mah)</strong></th>
				</tr>
			</thead>
			<tbody>
				<tr>
					<th>0</th>
					<th>0</th>
				</tr>
			</tbody>
		</table>
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
			<tbody id="nodes-table">
				<tr id="row434116965">
					<td>
						<p id="cell-434116965">0</p>
						<p>
							<input type="number" value="0" id="cell-edit-434116965" style="display:none">
						</p>
					</td>
					<td>434116965</td>
					<td>0</td>
					<td>0.057478003</td>
					<td>14</td>
					<td>false</td>
					<td>
						<p id="offset-434116965">0</p>
						<p>
							<input type="number" value="0" id="offset-edit-434116965" style="display:none">
						</p>
					</td>
					<td>
						<p id="scale-434116965">1</p>
						<p>
							<input type="number" value="1" id="scale-edit-434116965" style="display:none">
						</p>
					</td>
					<td>39192</td>
					<td>
						<button id="edit-434116965" onclick="enableEditing(434116965)">Edit</button>
						<button id="submit-434116965" onclick="submitChanges(434116965)"
							style="display:none;">Submit</button>
						<button id="cancel-434116965" onclick="cancelEdit(434116965)"
							style="display:none;">Cancel</button>
					</td>
				</tr>
			</tbody>
			<!-- <tbody id="nodes-table">
			</tbody> -->
		</table>
	</div>

	<script>
		let isEditing = false;

		function updateNodesTable(nodes) {
			if (isEditing) return;
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
						<p id="offset-${node.info.id}">${node.info.cell}</p>
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
.then(response => {
  if (!response.ok) {
    return response.text().then(errorText => {
      throw new Error(`Server error: ${errorText}`);
    });
  }
  return response.json();
})
.then(data => {
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
	</script>
</body>

</html>
)html";

void webserver()
{
	// Endpoint to serve the main website
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(200, "text/html", WEBSITE_HTML); });

	// Endpoint to return nodes data
	server.on("/getNodes", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
	    DynamicJsonDocument doc(1024);
	    JsonArray nodesArray = doc.createNestedArray("nodes");

	    for (auto &pair : nodeData) {
	        DynamicJsonDocument nodeDoc(512);
	        deserializeJson(nodeDoc, pair.second.data);
	        nodesArray.add(nodeDoc.as<JsonObject>());
	    }

	    String response;
	    serializeJson(doc, response);
	    request->send(200, "application/json", response); });

	// Endpoint to toggle SD write status (Placeholder for now)
	server.on("/toggleSD", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
        // TODO: Implement actual SD toggling logic here.
        static bool sdStatus = false;
        sdStatus = !sdStatus;
        String response = String("{\"sdStatus\":") + (sdStatus ? "true" : "false") + "}";
        request->send(200, "application/json", response); });

	server.on(
		"/updateNode", HTTP_POST, [](AsyncWebServerRequest *request)
		{
			// String reqBody;

			// PRINTF("MESSAGE: %s", reqBody);

			// if(request->hasParam("plain", true)){
			//     reqBody = request->getParam("body", true)->value();
			// } else {
			//     request->send(400, "application/json", "{\"status\":\"no data received\"}");
			//     return;
			// }

			// // Broadcast the data to the specific node in the mesh network
			// DynamicJsonDocument doc(256);
			// DeserializationError error = deserializeJson(doc, reqBody);

			// if (error) {
			//     request->send(500, "application/json", "{\"status\":\"json parsing error\"}");
			//     return;
			// }

			// uint32_t nodeId = doc["set"]["node_id"];
			// String jsonString;
			// serializeJson(doc, jsonString);
			// PRINTF("Sending message (%d): %s\n",nodeId, jsonString.c_str());
			// if (mesh.sendSingle(nodeId, jsonString)) {
			//     request->send(200, "application/json", "{\"status\":\"data sent\"}");
			// } else {
			//     request->send(500, "application/json", "{\"status\":\"send failed\"}");
			// }
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

		//Received from 2127948325 msg={"info":{"cell":0,"id":2127948325},"set":{"id":434116965,"cal_offset":0,"cal_scale":1,"cell":1,"balance":false,"led":false}}

		String jsonString;
		serializeJson(send_doc, jsonString);
		uint32_t nodeId = send_doc["set"]["id"].as<uint32_t>();
		mesh.sendSingle(nodeId, jsonString);

		PRINTF("SENDING (%d): %s\n", nodeId, jsonString)
		PRINTF("~~~%s\n", send_doc)
        
        // Your existing code to handle the received JSON data can follow here...
    } });

	server.begin();
}

#endif // MASTER