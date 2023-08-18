#include <Arduino.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>

#include <IPAddress.h>
#include <functions/mswitch.hpp>
#include <painlessMesh.h>

#define MESH_BSSID "wrcnet"
#define MESH_TOKEN "wrcnetpass"
#define MESH_CHANNEL 8
#define MESH_PORT 5555

#define STATION_SSID "Adrian SSID"
#define STATION_TOKEN "parola1234"

#define PARAM_INPUT_RELAY "relay"
#define PARAM_INPUT_STATE "state"

#define CAPTURE_FRAME 1
#define PIN_RELAY 0
#define PIN_RELAY_PULL_UP false
#define MAX_MANAGED_DEV 5
#define DELAY_TIME 30000
#define SERIAL_BOUND 115200

// before complile and uploading run the command
// python .\scripts\data_sync.py cls src/main.cc -i data/index.html:index_html
// -i data/index.js:index_js -i data/style.css:index_css
const char index_html[] PROGMEM = R"rawliteral(
)rawliteral";

const char index_css[] PROGMEM = R"rawliteral(
)rawliteral";

const char index_js[] PROGMEM = R"rawliteral(
)rawliteral";

using painlessmesh::wifi::Mesh;

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
void setup_mesh();
void setup_server();
void rutine_send_event_stream();
void rutine_elevate_root();
bool mesh_single_send(uint32_t dest, String msg);
bool hasfusebis(uint32_t mask);
void setfusebis(uint32_t bit);
bool server_process_update(String state, String swid = "");

uint32_t sflags{wrc::flags::EVENT_STEAM};
Mesh mesh{};
MeshSwitchFunction msf{MAX_MANAGED_DEV};
AsyncWebServer server(80);
AsyncEventSource events("/events");
BaseFunction *activeFunction[]{&msf, nullptr};

void setup() {
  Serial.begin(SERIAL_BOUND);

  setup_mesh();

  if (sflags & wrc::flags::MESH_ROOT) {
    setup_server();
  }

  LocalSwitch *lsf = new LocalSwitch{PIN_RELAY, !PIN_RELAY_PULL_UP};
  msf.setup_send_proc(&mesh_single_send);
  msf.setup_local_switch(mesh.getNodeId(), lsf);
  mesh.sendBroadcast(msf.name() + "::query::registration");
}

void loop() {
  mesh.update();
  rutine_send_event_stream();
  rutine_elevate_root();
}

void setup_mesh() {
  Serial.println("Running 'setup_mesh' ...");
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_BSSID, MESH_TOKEN, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.onReceive(&callback_receive_setup);

  mesh.setContainsRoot(true);
  mesh.onReceive(&callback_receive_worker);
}

void rutine_elevate_root() {
  if (hasfusebis(wrc::flags::MESH_ROOT)) {
    return;
  }

  Serial.println("Running rutine 'rutine_elevate_root' ...");
  // configuration for root
  if (mesh.getNodeList().size() == 0)
    Serial.println("Elevate current node as root ...");
  mesh.setHostname("WRC_BRIDGE");
  mesh.stationManual(STATION_SSID, STATION_TOKEN);
  mesh.setRoot(true);
  setfusebis(wrc::flags::MESH_ROOT);
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
  events.send("ping", NULL, millis());
  auto msf_iter = msf.iter_states();
  while (!msf_iter.done()) {
    events.send(String(msf_iter.id() + 1).c_str(),
                msf_iter.state() ? "relay_on" : "relay_off", millis());
    msf_iter.next();
  }
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
    if (request->hasParam(PARAM_INPUT_RELAY) &
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
    StaticJsonDocument<200> doc;
    String respone;
    auto msf_iter = msf.iter_states();
    while (!msf_iter.done()) {
      doc[String(msf_iter.id() + 1)] = msf_iter.state() ? "on" : "off";
      msf_iter.next();
    }
    serializeJson(doc, respone);
    request->send(200, "application/json", respone);
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

void callback_receive_worker(const uint32_t &from, const String &msg) {
  Serial.printf("incoming message from %u : %s\n", from, msg.c_str());
  for (BaseFunction **fct = activeFunction; fct != nullptr; fct++) {
    (*fct)->run(msg, from);
  }
}

bool hasfusebis(uint32_t mask) { return (sflags & mask) == mask; }
void setfusebis(uint32_t bit) { sflags |= bit; }
bool mesh_single_send(uint32_t dest, String msg) {
  Serial.printf("outgoing message to %u : %s\n", dest, msg.c_str());
  return mesh.sendSingle(dest, msg);
}

bool server_process_update(String state, String swid) {
  String event_name{""};
  bool action_result{false};

  if (swid == "") {
    auto msf_iter = msf.iter_states();
    while (!msf_iter.done()) {
      server_process_update(state, String(msf_iter.id() + 1));
      msf_iter.next();
    }
    return true;
  }

  if (state.equals(String("on"))) {
    action_result = msf.on(swid.toInt() - 1);
    event_name = "relay_on";
  }

  if (state.equals(String("off"))) {
    action_result = msf.off(swid.toInt() - 1);
    event_name = "relay_off";
  }

  events.send(swid.c_str(), event_name.c_str(), millis());
  Serial.println("Update event was sent ...");
  return action_result;
}