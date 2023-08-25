#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <espnow.h>

#ifndef MAX_CONN
#define MAX_CONN 5
#endif

void esp_on_recv(uint8_t *mac_addr, uint8_t *data, uint8_t len);
inline uint32_t encodeNodeId(const uint8_t *hwaddr);
typedef void (*cb_on_recive_t)(const uint32_t &from, const String &msg);

uint8_t BROADCAST_ADDR[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t dev_node_id{0};
uint32_t parent_id{0};
cb_on_recive_t _cb_on_recieve;
uint32_t connections[MAX_CONN];

namespace wrc {
namespace states {
enum NetworkStatus {
  PAIR_REQUEST = 1,
  PAIR_REQUESTED = 1 << 1,
  PAIR_DONE = 1 << 2,
};

enum PackageStatus {
  PREPARE = 1,
  SEND_WAIT = 1 << 1,
  SENT_OK = 1 << 2,
  SENT_FAULT = 1 << 3,
};
} // namespace states
} // namespace wrc

void esp_on_send(uint8_t *mac_addr, uint8_t status) {
  Serial.printf("Last packet sent status to %d = %d\n", encodeNodeId(mac_addr),
                status);
}




// typedef void (*cb_on_sent_t)(u8 *mac_addr, u8 status);

struct MeshPackage {
  uint32_t target;
  uint32_t source;
  char msg[200];
};

class MeshNetwork {
  String _wifi_ssid;
  String _wifi_pass;
  uint16_t _wifi_port;
  WiFiMode_t _wifi_mode;
  uint8_t _wifi_channel;
  uint8_t _wifi_hidden;
  uint8_t _wifi_maxconn;
  uint8_t _is_root;

protected:
  uint8_t net_state{0};

public:
  void init(String ssid, String password, uint16_t port = 5555,
            WiFiMode_t connectMode = WIFI_AP_STA, uint8_t channel = 1,
            uint8_t hidden = 0, uint8_t maxconn = MAX_CONN) {
    this->_wifi_ssid = ssid;
    this->_wifi_pass = password;
    this->_wifi_port = port;
    this->_wifi_mode = connectMode;
    this->_wifi_channel = channel;
    this->_wifi_hidden = hidden;
    this->_wifi_maxconn = maxconn;
    this->network_init();
  }

  void update() {

  }

  void scan_other_roots() {
    
  }

  uint8_t connectionCount() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CONN; i++) {
      if (connections[i]) {
        count++;
      }
    }
    return count;
  }

  bool sendBroadcast(const String &msg) {
    MeshPackage pkg{.target = 0, .source = dev_node_id};
    memcpy(pkg.msg, msg.c_str(), msg.length());
    esp_now_send(BROADCAST_ADDR, (uint8_t *)&pkg, sizeof(pkg));
    return true;
  }

  bool sendSingle(uint32_t dest, const String &msg) {
    MeshPackage pkg{.target = dest, .source = dev_node_id};
    memcpy(pkg.msg, msg.c_str(), msg.length());
    esp_now_send(BROADCAST_ADDR, (uint8_t *)&pkg, sizeof(pkg));
    return true;
  }

  IPAddress getStationIP() { return WiFi.localIP(); }

  bool isRoot() { return _is_root; }

  static void exec_on_receive(uint8_t *mac_addr, uint8_t *data, uint8_t len) {
    MeshPackage curr_mess;
    memcpy(&curr_mess, data, len);
    uint32_t dest = curr_mess.target;
    uint32_t from = curr_mess.source;
    if (dest != 0 || dest != getNodeId()) {
      // ignore not apply
      return;
    }

    String message = (char *)curr_mess.msg;

    if (message == "CONN") {
      Serial.printf("New connection %d", from);
      for (uint8_t i = 0; i < MAX_CONN; i++) {
        if (!connections[i]) {
          connections[i] = from;
          MeshPackage send_package{.target = from, .source = getNodeId()};
          memcpy(send_package.msg, "HELLO", 5);
          esp_now_send(BROADCAST_ADDR, (uint8_t *)&send_package,
                       sizeof(send_package));
          return;
        }
      }
    }

    if (message == "HELLO") {
      if (!parent_id) {
        parent_id = from;
      }
    }

    _cb_on_recieve(curr_mess.source, message);
  }

  static void onReceive(cb_on_recive_t cb) { _cb_on_recieve = cb; }

  static uint32_t getNodeId() {
    if (dev_node_id) {
      return dev_node_id;
    }
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    dev_node_id = encodeNodeId(mac);
    return dev_node_id;
  }

private:
  void esp_init() {
    if (esp_now_init() != 0) {
      Serial.println("Error initializing ESP-NOW");
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(esp_on_send);
    esp_now_register_recv_cb(esp_on_recv);
  }

  void network_init() {
    uint8_t sflags{0};
    uint64_t last_ts{0}, curr_ts{0};
    MeshPackage msg{.target = 0, .source = getNodeId()};
    uint8_t pair_channel = 1;
    // Set device as a Wi-Fi Station
    WiFi.mode(_wifi_mode);

    dev_node_id = getNodeId();
    WiFi.disconnect();

    esp_init();
    net_state = wrc::states::PAIR_REQUEST;
    while (net_state != wrc::states::PAIR_DONE) {
      if (parent_id) {
        net_state = wrc::states::PAIR_DONE;
      }
      switch (net_state) {
      case wrc::states::PAIR_REQUEST:
        Serial.printf("Pairing request on channel %d", pair_channel);

        // clean esp now
        esp_now_deinit();
        WiFi.mode(WIFI_STA);
        // set WiFi channel
        wifi_promiscuous_enable(1);
        wifi_set_channel(pair_channel);
        wifi_promiscuous_enable(0);
        // WiFi.printDiag(Serial);
        WiFi.disconnect();

        // Init ESP-NOW
        esp_init();
        memcpy(msg.msg, "CONN", 4);
        last_ts = millis();
        esp_now_send(BROADCAST_ADDR, (uint8_t *)&msg, sizeof(msg));
        net_state = wrc::states::PAIR_REQUESTED;
        break;

      case wrc::states::PAIR_REQUESTED:
        // time out to allow receiving response from server
        curr_ts = millis();
        if (curr_ts - last_ts > 100) {
          last_ts = curr_ts;
          // time out expired,  try next channel
          pair_channel++;
          if (pair_channel > 11) {
            pair_channel = 0;
            if (sflags) {
              net_state = wrc::states::PAIR_DONE;
              _is_root = 1;
              break;
            }
            sflags = 1;
          }
          net_state = wrc::states::PAIR_REQUEST;
        }
        break;

      case wrc::states::PAIR_DONE:
        Serial.printf("Node with id %d connected", getNodeId());
        break;
      }
    }

    esp_now_deinit();
    esp_init();

    if (!isRoot()) {
      return;
    }

    WiFi.mode(_wifi_mode);
    // Set device as a Wi-Fi Station
    WiFi.begin(_wifi_ssid, _wifi_pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Setting as a Wi-Fi Station..");
    }

    pair_channel = WiFi.channel();
    Serial.print("Server SOFT AP MAC Address:  ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(pair_channel);
  }
};

void esp_on_recv(uint8_t *mac_addr, uint8_t *data, uint8_t len) {
  MeshNetwork::exec_on_receive(mac_addr, data, len);
}

inline uint32_t encodeNodeId(const uint8_t *hwaddr) {
  uint32_t value = 0;
  value |= hwaddr[2] << 24;
  value |= hwaddr[3] << 16;
  value |= hwaddr[4] << 8;
  value |= hwaddr[5];
  return value;
}
