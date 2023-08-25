#include <Arduino.h>

#define MNET 1
#define PAINLESS 2
#define USEIMPL MNET

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>

#include <IPAddress.h>
#include <functions/mswitch.hpp>

#if USEIMPL == MNET
#include <mnet.hpp>
#endif
#if USEIMPL == PAINLESS
#include <painlessMesh.h>
#endif

#define MESH_BSSID "wrcnet"
#define MESH_TOKEN "wrcnetpass"
// #define MESH_BSSID "Adrian SSID"
// #define MESH_TOKEN "parola1234"
#define MESH_CHANNEL 8
#define MESH_PORT 5555

#define STATION_SSID "Adrian SSID"
#define STATION_TOKEN "parola1234"

#define PARAM_INPUT_RELAY "relay"
#define PARAM_INPUT_STATE "state"

#define CAPTURE_FRAME 1
#define PIN_RELAY 0
#define PIN_RELAY_PULL_UP true
#define MAX_MANAGED_DEV 5
#define DELAY_TIME 30000
#define SERIAL_BOUND 115200

// before complile and uploading run the command
// python .\scripts\data_sync.py cls src/main.cc -i data/index.html:index_html
// -i data/index.js:index_js -i data/style.css:index_css
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>LOVIMAR DASHBOARD</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
        integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous" />
    <link rel="icon" href="data:," />
    <link href="/style.css" rel="stylesheet">
    <script defer="defer" src="/index.js"></script>
</head>
<body>
    <div class="topnav">
        <h1>LOVIMAR DASHBOARD</h1>
    </div>
    <div class="content">
        <div class="cards">
            <div class="card temperature" id="concomitent-card">
                <p class="card-title">
                    <i class="fas fa-broadcast-tower"></i> Control - CONCOMITENT
                </p>
                <div class="card-body">
                    <label class="switch">
                        <input type="checkbox" onchange="toggleCheckboxAll(this)" id="concomitent-btn">
                        <span class="slider"></span>
                    </label>
                </div>
                <p class="timestamp">Last Update: <span id="concomitent-lts">2023/07/28 12:43:23</span></p>
            </div>
            <div class="card temperature">
                <p class="card-title">
                    <i class="fab fa-buromobelexperte"></i> Control - INDIVIDUAL
                </p>
                <div class="card-body" id="individual-pannel"></div>
                <p class="timestamp">Last Reading: <span id="individual-lts">2023/07/28 12:43:23</span></p>
            </div>
        </div>
    </div>
</body>
</html>

)rawliteral";

const char index_css[] PROGMEM = R"rawliteral(
html {
    font-family: Arial;
    display: inline-block;
    text-align: center;
}

h1 {
    font-size: 2rem;
}

body {
    margin: 0;
}

.topnav {
    overflow: hidden;
    background-color: #2f4468;
    color: white;
    font-size: 1.7rem;
}

.content {
    padding: 20px;
}

.card {
    background-color: white;
    box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, 0.5);
}

.cards {
    max-width: 700px;
    margin: 0 auto;
    display: grid;
    grid-gap: 2rem;
    grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
}

.reading {
    font-size: 2.8rem;
}

.timestamp {
    color: #bebebe;
    font-size: 1rem;
}

.card-title {
    font-size: 1.2rem;
    font-weight: bold;
}

.card.temperature {
    color: #b10f2e;
}

.card.humidity {
    color: #50b8b4;
}

h2 {
    font-size: 3rem;
}

p {
    font-size: 3rem;
}

body {
    max-width: 600px;
    margin: 0px auto;
    padding-bottom: 25px;
}

.card-body {
    display: block;
    padding-bottom: 8px;
}

.switch {
    position: relative;
    display: inline-block;
    width: 120px;
    height: 68px;
    margin: 8px;
}

.switch input {
    display: none;
}

.switch.offline {
    color: #2f4468;
}
.switch.offline::after {
    color: #2f4468;
    content: "~off";
    pointer-events: none;
}
.switch.offline input {
    pointer-events: none;
}

.switch span.name {
    position: relative;
    display: inline-block;
    margin-top: 70px;
    font-size: 1rem;
}

.slider {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    /* z-index: 999; */
    border-radius: 34px;
}

.slider:before {
    position: absolute;
    content: "";
    height: 52px;
    width: 52px;
    left: 8px;
    bottom: 8px;
    background-color: #fff;
    -webkit-transition: 0.4s;
    transition: 0.4s;
    border-radius: 68px;
}

.switch.offline .slider {
    background-color: #6e6868;
}

.switch.offline input:checked+.slider {
    background-color: #703a3a;
}

input:checked+.slider {
    background-color: #2196f3;
}

input:checked+.slider:before {
    -webkit-transform: translateX(52px);
    -ms-transform: translateX(52px);
    transform: translateX(52px);
}

)rawliteral";

const char index_js[] PROGMEM = R"rawliteral(
var btn_states = {};

function getDateTime() {
    return new Date().toISOString();
}

function toggleCheckbox(event) {
    element = event.target;
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false : true;
    let id = element.id.split("individual-btn-")[1];
    let url = `update?relay=${id}&state=${element.checked ? 'off' : 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function toggleCheckboxAll(element) {
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false : true;
    let url = `update?state=${element.checked ? 'off' : 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function create_relay_btn(container, r_index, r_state) {
    let root = document.createElement('label');
    root.classList = ['switch'];
    container.appendChild(root);

    let input = document.createElement('input');
    input.type = "checkbox"
    input.onchange = toggleCheckbox;
    input.id = `individual-btn-${r_index}`;
    root.appendChild(input);

    let slider = document.createElement('span');
    slider.classList = ["slider"];
    root.appendChild(slider);

    let title = document.createElement('span');
    title.classList = ['name'];
    title.textContent = `ready #${r_index}`;
    root.appendChild(title);

    relay_btn_state_update(r_index, r_state);
}

function paint_btns(data) {
    let container = document.getElementById("individual-pannel");
    container.innerHTML = "";

    for (const [key, value] of Object.entries(data)) {
        create_relay_btn(container, key, value);
    }
}

function relay_btn_state_update(dev_id, state) {
    let ts_now = getDateTime();
    let ts_ind = document.getElementById("individual-lts");
    let ts_com = document.getElementById("concomitent-lts");
    let btn_ind = document.getElementById(`individual-btn-${dev_id}`);
    let valid_state = ["on", "off"].includes(state);

    if (btn_ind == null) {
        sync_relays();
        return;
    }

    btn_ind.checked = state == "on" ? true : false;
    btn_states[dev_id] = btn_ind.checked;
    btn_ind.disabled = !valid_state;
    btn_ind.parentElement.classList = valid_state ? ["switch"] : ["switch", "offline"];

    let tb_com_state = true;
    for (const [key, value] of Object.entries(btn_states)) {
        if ( value == false) {
            tb_com_state = false;
        }
    }

    document.getElementById("concomitent-btn").checked = tb_com_state;

    ts_com.innerHTML = ts_now;
    ts_ind.innerHTML = ts_now;
}

function sync_relays() {
    fetch('/relays').then(r => r.json()).then(paint_btns);
}

function main() {
    sync_relays();
}

document.addEventListener('DOMContentLoaded', main);

if (!!window.EventSource) {
    var source = new EventSource("/events");

    source.addEventListener(
        "open",
        (e) => {
            console.log("Events Connected");
        },
        false
    );
    source.addEventListener(
        "error",
        (e) => {
            if (e.target.readyState != EventSource.OPEN) {
                console.log("Events Disconnected");
            }
        },
        false
    );

    source.addEventListener(
        "message",
        (e) => {
            console.log("message", e.data);
        },
        false
    );

    source.addEventListener(
        "relay_on",
        (e) => {
            relay_btn_state_update(e.data, "on");
        },
        false
    );

    source.addEventListener(
        "relay_off",
        (e) => {
            relay_btn_state_update(e.data, "off");
        },
        false
    );
}

)rawliteral";

#if USEIMPL == MNET
using Mesh = MeshNetwork;
#endif
#if USEIMPL == PAINLESS
using painlessmesh::wifi::Mesh;
#endif

namespace wrc {
namespace flags {
enum FuseBits {
  EVENT_STEAM = 0b1,
  MESH_ROOT = 0b10,
  SERVER_HOST = 0b100,
};
} // namespace flags
} // namespace wrc

void callback_receive_setup(const uint32_t &from, const String &msg);
void callback_receive_worker(const uint32_t &from, const String &msg);
void callback_sw_state_changed(SwitchState &swd);
void setup_mesh();
void setup_server();
void rutine_send_event_stream();
void rutine_elevate_root();
void rutine_query_state();
bool mesh_single_send(uint32_t dest, String msg);
bool hasfusebis(uint32_t mask);
void setfusebis(uint32_t bit);
bool server_process_update(String state, String swid = "");
void proc_event_send(const char *message, const char *event = NULL,
                     uint32_t id = 0, uint32_t reconnect = 0);
String sjson_sw(MeshSwitchFunction *sw);

// uint32_t sflags{wrc::flags::EVENT_STEAM};
uint32_t sflags{0};
Mesh mesh{};
SwitchState *sw_state = new SwitchState();
MeshSwitchFunction msf{MAX_MANAGED_DEV};
AsyncWebServer server(80);
AsyncEventSource events("/events");
BaseFunction *activeFunction[]{&msf, nullptr};

IPAddress myIP(0, 0, 0, 0);
IPAddress myAPIP(0, 0, 0, 0);

IPAddress getlocalIP() { return IPAddress(mesh.getStationIP()); }

void setup() {
  Serial.begin(SERIAL_BOUND);
  pinMode(LED_BUILTIN, OUTPUT);

  delay(1);
  Serial.println("DEVICE START");

  setup_mesh();

  if (sflags & wrc::flags::MESH_ROOT) {
    setup_server();
  }

  LocalSwitch *lsf = new LocalSwitch{PIN_RELAY, PIN_RELAY_PULL_UP};
  msf.on_change(callback_sw_state_changed);
  msf.setup_send_proc(&mesh_single_send);
  msf.setup_local_switch(mesh.getNodeId(), lsf);
  mesh.sendBroadcast(msf.name() + "::query::registration");
}

void loop() {
  mesh.update();
  rutine_send_event_stream();
  rutine_elevate_root();
  rutine_query_state();

  if (myIP != getlocalIP()) {
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }
}

#if USEIMPL == MNET
void setup_mesh() {
  Serial.println("Running 'setup_mesh' ...");

  mesh.init(STATION_SSID, STATION_TOKEN, MESH_PORT, WIFI_STA);
  mesh.onReceive(&callback_receive_worker);
}

void rutine_elevate_root() {
  if (hasfusebis(wrc::flags::MESH_ROOT)) {
    return;
  }

  Serial.println("Running rutine 'rutine_elevate_root' ...");
  Serial.println(String(sflags));
  Serial.println(String(mesh.connectionCount()));
  // configuration for root
  if (mesh.connectionCount() <= 1) {

    Serial.println("Elevate current node as root ...");
    // mesh.stationManual(STATION_SSID, STATION_TOKEN);
    // mesh.setHostname("WRC_BRIDGE");
    // mesh.setRoot(true);
    // mesh.setContainsRoot(true);
    setfusebis(wrc::flags::MESH_ROOT);
    setup_server();
  }
}
#endif
#if USEIMPL == PAINLESS
void setup_mesh() {
  Serial.println("Running 'setup_mesh' ...");
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_BSSID, MESH_TOKEN, MESH_PORT, WIFI_STA);
  mesh.onReceive(&callback_receive_worker);

  mesh.stationManual(STATION_SSID, STATION_TOKEN);
  mesh.setHostname("WRC_BRIDGE");
  mesh.setRoot(true);

  mesh.setContainsRoot(true);

  myAPIP = IPAddress(mesh.getAPIP());
  Serial.println("My AP IP is " + myAPIP.toString());
}

void rutine_elevate_root() {
  if (hasfusebis(wrc::flags::MESH_ROOT)) {
    return;
  }

  Serial.println("Running rutine 'rutine_elevate_root' ...");
  Serial.println(String(sflags));
  Serial.println(String(mesh.getNodeList().size()));
  // configuration for root
  if (mesh.getNodeList().size() == 0) {

    Serial.println("Elevate current node as root ...");
    mesh.stationManual(STATION_SSID, STATION_TOKEN);
    mesh.setHostname("WRC_BRIDGE");
    mesh.setRoot(true);
    mesh.setContainsRoot(true);
    setfusebis(wrc::flags::MESH_ROOT);
    setup_server();
  }
}
#endif

void rutine_query_state() {
  static uint64_t last_ts{0};
  if ((sflags & wrc::flags::SERVER_HOST) == 0x0) {
    return;
  }

  if ((millis() - last_ts) <= 5000) {
    return;
  }
  last_ts = millis();

  msf.query_state_all();
}

void rutine_send_event_stream() {
  static uint64_t last_ts;
  if ((sflags & wrc::flags::EVENT_STEAM) == 0x0) {
    return;
  }
  if ((sflags & wrc::flags::SERVER_HOST) == 0x0) {
    return;
  }

  if ((millis() - last_ts) <= DELAY_TIME) {
    return;
  }

  Serial.println("Event update sending ...");
  proc_event_send("ping", NULL, millis());
  String nfo{};

  msf.iter(nfo, [](String &data, SwitchState &swd) {
    proc_event_send(String(swd.store_id + 1).c_str(),
                    swd.state ? "relay_on" : "relay_off", millis());
  });
  last_ts = millis();
}

void setup_server() {
  Serial.println("Setting up the server");
  setfusebis(wrc::flags::SERVER_HOST);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP][GET] /");
    request->send(200, "text/html", index_html);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP][GET] /style.css");
    request->send(200, "text/css", index_css);
  });

  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP][GET] /index.js");
    request->send(200, "text/javascript", index_js);
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print("[HTTP][GET] ");
    Serial.println(request->url());
    if (request->hasParam(PARAM_INPUT_RELAY) &&
        request->hasParam(PARAM_INPUT_STATE)) {
      server_process_update(request->getParam(PARAM_INPUT_STATE)->value(),
                            request->getParam(PARAM_INPUT_RELAY)->value());
    }

    else if (request->hasParam(PARAM_INPUT_STATE)) {
      server_process_update(request->getParam(PARAM_INPUT_STATE)->value());
    }

    request->send(200, "text/plain", "OK");
  });

  server.on("/relays", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP][GET] /relays");
    request->send(200, "application/json", sjson_sw(&msf));
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n",
                    client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.addHandler(&events);
  server.begin();
  Serial.println("Server ready ... ");
}

void callback_receive_setup(const uint32_t &from, const String &msg) {
  Serial.printf("incoming message from %u : %s\n", from, msg.c_str());
}

void callback_sw_state_changed(SwitchState &swd) {
  proc_event_send(String(swd.store_id + 1).c_str(),
                  swd.state ? "relay_on" : "relay_off", millis());
}

void callback_receive_worker(const uint32_t &from, const String &msg) {
  Serial.printf("Incoming message from %u : %s\n", from, msg.c_str());
  for (uint8_t ind = 0;; ind++) {
    BaseFunction *fct = activeFunction[ind];
    if (fct == nullptr) {
      break;
    }
    Serial.printf("Handling message with %s\n", fct->name().c_str());
    fct->run(msg, from);
    Serial.printf("-----\n");
  }
}

bool hasfusebis(uint32_t mask) { return (sflags & mask) == mask; }
void setfusebis(uint32_t bit) { sflags = sflags | bit; }
bool mesh_single_send(uint32_t dest, String msg) {
  Serial.printf("outgoing message to %u : %s\n", dest, msg.c_str());
  return mesh.sendSingle(dest, msg);
}

bool server_process_update(String state, String swid) {
  String event_name{""};

  Serial.printf("RUN server_process_update <%s, %s>\n", state.c_str(),
                swid.c_str());

  if (swid == "") {
    mesh.sendBroadcast(msf.name() + "::update::" + state);
    server_process_update(state, String(msf.index_lsf() + 1));
    return true;
  }

  if (!msf.node_id(swid.toInt() - 1)) {
    return false;
  }

  if (state.equals(String("on"))) {
    return msf.on(swid.toInt() - 1);
  }

  if (state.equals(String("off"))) {
    return msf.off(swid.toInt() - 1);
  }

  return false;
}

void proc_event_send(const char *message, const char *event, uint32_t id,
                     uint32_t reconnect) {
  String logMsg = "Event message was sent " + String(message);
  if (event) {
    logMsg += " on event " + String(event);
  }

  if (id) {
    logMsg += " with id " + String(id);
  }

  Serial.println(logMsg);
  events.send(message, event, id, reconnect);
}

#define JFMT_ITEM_DS "\"%d\":\"%s\""

String sjson_sw(MeshSwitchFunction *sw) {
  String result = "{\n  ";

  sw->iter(
      result,
      [](String &result, SwitchState &swd) {
        char tmp[20]{0};
        sprintf(tmp, JFMT_ITEM_DS, swd.store_id + 1, swd.state ? "on" : "off");
        result += tmp;
      },
      1);
  sw->iter(
      result,
      [](String &result, SwitchState &swd) {
        char tmp[20]{0};
        sprintf(tmp, JFMT_ITEM_DS, swd.store_id + 1, swd.state ? "on" : "off");
        result += ",\n  ";
        result += tmp;
      },
      10, 1);

  result += "\n}";
  return result;
}
