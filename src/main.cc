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
#include <webfiles.hpp>

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
void rutine_send_local_status();
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
  rutine_send_local_status();

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

void rutine_send_local_status() {
  static uint64_t last_ts{0};

  if ((millis() - last_ts) <= 1000) {
    return;
  }

  last_ts = millis();

  msf.send_local_staus();
}

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
