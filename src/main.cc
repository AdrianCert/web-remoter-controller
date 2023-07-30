#include <Arduino.h>

// #include <Esp.h>

// #include <FS.h>
// #include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <espnow.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>

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
    content: "off";
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
function getDateTime() {
    return new Date().toISOString();
}

function toggleCheckbox(event) {
    element = event.target;
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false: true;
    let id = element.id.split("individual-btn-")[1];
    let url = `update?relay=${id}&state=${element.checked? 'off': 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function toggleCheckboxAll(element) {
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false: true;
    let url = `update?state=${element.checked? 'off': 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function create_relay_btn(container, r_index, r_state) {
    let root = document.createElement('label');
    root.classList = [ 'switch'];
    container.appendChild(root);

    let input = document.createElement('input');
    input.type = "checkbox"
    input.onchange = toggleCheckbox;
    input.id = `individual-btn-${r_index}`;
    input.checked = r_state == "on" ? true: false;
    root.appendChild(input);

    let slider = document.createElement('span');
    slider.classList = ["slider"];
    root.appendChild(slider);

    let title = document.createElement('span');
    title.classList = ['name'];
    title.textContent = `ready #${r_index}`;
    root.appendChild(title);
}

function paint_btns(data) {
    let container = document.getElementById("individual-pannel");
    // return;
    container.innerHTML = "";
    for (const [key, value] of Object.entries(data)) {
        console.log(`${key}: ${value}`);
        create_relay_btn(container, key, value);
    }
}

function main() {
    fetch('/relays').then( r => r.json()).then(paint_btns);
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
            console.log("relay_on", e.data);
            let obj = JSON.parse(e.data);
            let ts_now = getDateTime();
            let ts_ind = document.getElementById("individual-lts");
            let ts_com = document.getElementById("concomitent-lts");
            console.log(`individual-btn-${obj}`)
            console.log(`individual-btn-${e.data}`)
            let btn_ind = document.getElementById(`individual-btn-${obj}`);
            btn_ind.checked = true;
            ts_com.innerHTML = ts_now;
            ts_ind.innerHTML = ts_now;
        },
        false
    );

    source.addEventListener(
        "relay_off",
        (e) => {
            console.log("relay_off", e.data);
            let obj = JSON.parse(e.data);
            let ts_now = getDateTime();
            let ts_ind = document.getElementById("individual-lts");
            let ts_com = document.getElementById("concomitent-lts");
            console.log(`individual-btn-${obj}`)
            console.log(`individual-btn-${e.data}`)
            let btn_ind = document.getElementById(`individual-btn-${obj}`);
            btn_ind.checked = false;
            ts_com.innerHTML = ts_now;
            ts_ind.innerHTML = ts_now;
        },
        false
    );
}

)rawliteral";

#define CAPTURE_FRAME 1
#define PIN_RELAY 0
#define MAX_MANAGED_DEV 5
#define LOCAL_MANAGED_DEV 0xa
#define DELAY_TIME 30000
#define SERIAL_BOUND 115200

enum PairingStatus
{
    PAIR_REQUEST,
    PAIR_REQUESTED,
    PAIR_PAIRED,
    PAIR_PRIMARY
};

enum MessageTypes
{
    MSG_PAIR_TX,
    MSG_PAIR_RX,
    MSG_DATA,
    MSG_PING,
    MSG_PONG
};


enum ActionsValues
{
    ACT_RELAY_ON,
    ACT_RELAY_OFF,
    ACT_RELAY_TGL,
};

typedef struct MessagePayload
{
    uint8_t type;
    uint8_t id;
    uint8_t action;
    uint8_t value;
} MessagePayload;

typedef struct PairDevice
{
    uint8_t type;
    uint8_t id;
    uint8_t addr[6];
    uint8_t channel;
} PairDevice;

enum DeviceState
{
    START_UP,
    PAIR_SL,
    PAIR_MS,
    GRANTED_MS,
    START_MS,
    READY_MS,
    READY_SL,
};

const char *PARAM_INPUT_RELAY = "relay";
const char *PARAM_INPUT_STATE = "state";

PairingStatus pair_status = PAIR_REQUEST;
DeviceState dev_state = START_UP;
PairDevice pair_dev;
MessagePayload msg_recv;
MessagePayload msg_send;
uint8_t pair_channel = 1;
uint8_t pair_id = 0;
int _status = 0;
static int local_r_state = LOW;
const char *ssid = "Adrian SSID";
const char *password = "parola1234";
unsigned long ts_last, ts_curr;

PairDevice recv_pair_dev;
PairDevice *managed_devices[] = {0x0, 0x0, 0x0, 0x0, 0x0};
uint8_t status_devices[] = {0x0, 0x0, 0x0, 0x0, 0x0};
uint8_t broadcast_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

AsyncWebServer server(80);
AsyncEventSource events("/events");

void relay_tongle()
{
    Serial.println("Device action: relay_tongle");
    local_r_state = local_r_state == LOW ? HIGH : LOW;
    digitalWrite(PIN_RELAY, local_r_state);
    digitalWrite(LED_BUILTIN, local_r_state);
}

void relay_on()
{
    Serial.println("Device action: relay_on");
    local_r_state = LOW;
    digitalWrite(PIN_RELAY, local_r_state);
    digitalWrite(LED_BUILTIN, local_r_state);
}

void relay_off()
{
    Serial.println("Device action: relay_off");
    local_r_state = HIGH;
    digitalWrite(PIN_RELAY, local_r_state);
    digitalWrite(LED_BUILTIN, local_r_state);
}

void handle_msg_pair_rx(uint8_t *mac_addr, PairDevice *data)
{
    // processing response from server (slave will do it)
    if (PAIR_SL != dev_state)
        return;
    if (PAIR_REQUESTED != pair_status)
        return;

    memcpy(&pair_dev, data, sizeof(PairDevice));
    memcpy(&pair_dev.addr, mac_addr, 6);

    pair_id = pair_dev.id;
    pair_dev.id = 1;
}

void handle_msg_pair_tx(uint8_t *mac_addr, PairDevice *data)
{
    // processing request from server (server will do it)
    // if for bord will be allocated and stored

    // if (esp_now_is_peer_exist(mac_addr)) {
    //     // Slave already paired.
    //     Serial.println("Already Paired");
    //     return;
    // }

    uint8_t dev_id = 0;
    while (dev_id < MAX_MANAGED_DEV)
    {
        if (managed_devices[dev_id] == 0)
        {
            break;
        }
        dev_id++;
    }
    if (MAX_MANAGED_DEV == dev_id)
        return;
    managed_devices[dev_id] = (PairDevice *)malloc(sizeof(PairDevice));
    managed_devices[dev_id]->type = MSG_PAIR_RX;
    managed_devices[dev_id]->id = dev_id + 1;
    managed_devices[dev_id]->channel = pair_channel;
    memcpy(managed_devices[dev_id]->addr, mac_addr, 6);
    events.send(String(dev_id + 1).c_str(), "relay_ready", millis());

    // esp_now_add_peer(mac_addr);
    esp_now_send(mac_addr, (uint8_t *)managed_devices[dev_id], sizeof(PairDevice));
}

void handle_msg_data(uint8_t *mac_addr, MessagePayload *data)
{
    if (READY_SL != dev_state) {
        Serial.println("Device not ready to receive commands ...");
        return;
    }
    if (data->id != pair_id) {
        Serial.print("Device id is not matched local_id=");
        Serial.print(pair_id);
        Serial.print(" targer_id=");
        Serial.println(data->id);
        return;
    }
    if (ACT_RELAY_ON == data->action)
    {
        relay_on();
    }
    if (ACT_RELAY_OFF == data->action)
    {
        relay_off();
    }
    if (ACT_RELAY_TGL == data->action)
    {
        relay_tongle();
    }
}

void print_mac(const uint8_t *mac_addr)
{
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print(mac_str);
}

void esp_frame_trace(uint8_t direction, uint8_t *mac_addr, uint8_t *data, uint8_t len) {
    Serial.print(direction ? "frame <= ": "frame => ");
    print_mac(mac_addr);
    Serial.print(" [ ");
    Serial.print(len);
    Serial.println(" ]");
    for(int i=0; i<len; i++) {
        Serial.print(*(data+i));
        Serial.print(" ");
    }
    Serial.println("");

}

void esp_on_send(uint8_t *mac_addr, uint8_t status)
{
    Serial.print("Last packet sent status: ");
    print_mac(mac_addr);
    if (status == 0)
    {
        Serial.println(" success");
    }
    else
    {
        Serial.println(" fail");
    }
    Serial.println();
}

void esp_on_recv(uint8_t *mac_addr, uint8_t *data, uint8_t len)
{
    #ifdef CAPTURE_FRAME
    esp_frame_trace(1, mac_addr, data, len);
    #endif

    uint8_t d_type = data[0];

    switch (d_type)
    {
    case MSG_PAIR_RX:
        memcpy(&recv_pair_dev, data, len);
        handle_msg_pair_rx(mac_addr, &recv_pair_dev);
    case MSG_PAIR_TX:
        // request for pairing
        memcpy(&recv_pair_dev, data, len);
        handle_msg_pair_tx(mac_addr, &recv_pair_dev);
        break;
    case MSG_DATA:
        MessagePayload recv_msg;
        memcpy(&recv_msg, data, len);
        handle_msg_data(mac_addr, &recv_msg);
        break;
    default:
        break;
    }
}

void esp_to_setup()
{
    if (esp_now_init() != 0)
    {
        Serial.println("Error initializing ESP-NOW");
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    // set callback routines
    esp_now_register_send_cb(esp_on_send);
    esp_now_register_recv_cb(esp_on_recv);
}

PairingStatus auto_pairing()
{
    switch (pair_status)
    {
    case PAIR_REQUEST:
        Serial.print("Pairing request on channel ");
        Serial.println(pair_channel);

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
        esp_to_setup();

        pair_dev.id = 0;
        pair_dev.type = MSG_PAIR_TX;
        pair_dev.channel = pair_channel;

        // add peer and send request
        ts_last = millis();
        _status = esp_now_send(broadcast_address, (uint8_t *)&pair_dev, sizeof(pair_dev));
        Serial.println(_status);
        pair_status = PAIR_REQUESTED;
        break;

    case PAIR_REQUESTED:
        // time out to allow receiving response from server
        ts_curr = millis();
        if (ts_curr - ts_last > 100)
        {
            ts_last = ts_curr;
            // time out expired,  try next channel
            pair_channel++;
            if (pair_channel > 11)
            {
                pair_status = PAIR_PRIMARY;
                pair_channel = 0;
                break;
            }
            pair_status = PAIR_REQUEST;
        }
        break;

    case PAIR_PAIRED:
        break;
    case PAIR_PRIMARY:
        break;
    }
    return pair_status;
}

void network_init()
{
    dev_state = PAIR_SL;
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    Serial.println(WiFi.macAddress());
    WiFi.disconnect();

    esp_to_setup();
    pair_status = PAIR_REQUEST;
    while (1)
    {
        auto_pairing();
        if (PAIR_PRIMARY == pair_status)
        {
            pair_id = 1;
            break;
        }
        if (PAIR_PAIRED == pair_status)
        {
            dev_state = READY_SL;
            break;
        }
    }

    if (READY_SL == dev_state)
    {
        return;
    }

    dev_state = PAIR_MS;
    esp_now_deinit();
    esp_to_setup();

    Serial.println("Device granted for master");
    Serial.print("Server mac address: ");
    Serial.println(WiFi.macAddress());

    WiFi.mode(WIFI_AP_STA);
    // Set device as a Wi-Fi Station
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
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

    dev_state = GRANTED_MS;
}

void process_relay_action(int relay, const String &state)
{
    int8_t state_val = 3;
    Serial.print("Apply action relay ");
    Serial.print(state);
    Serial.print(" on #");
    Serial.println(relay);

    if(state.equals(String("on"))) {
        state_val = 1;
    }

    if(state.equals(String("off"))) {
        state_val = 0;
    }

    if(state_val == 3) {
        return;
    }
    if(managed_devices[relay] == 0x0) {
        return;
    }

    status_devices[relay] = state_val ? ACT_RELAY_ON: ACT_RELAY_OFF;

    // local input
    if (relay == pair_id - 1) {
        Serial.println("Apply action relay localy");
        events.send(String(relay + 1).c_str(), state_val ? "relay_on" : "relay_off", millis());
        Serial.println("Update event was sent ...");
        return state_val ? relay_on() : relay_off();
    }

    MessagePayload send_msg;

    send_msg.type = MSG_DATA;
    send_msg.id = relay;
    send_msg.action = state_val? ACT_RELAY_ON: ACT_RELAY_OFF;

    _status = esp_now_send(managed_devices[relay]->addr, (uint8_t*)&send_msg, sizeof(MessagePayload));
    events.send(String(relay + 1).c_str(), state_val ? "relay_on" : "relay_off", millis());
    Serial.println("Update event was sent ...");
}

void server_init()
{
    Serial.print("Current device state ");
    Serial.println(dev_state);
    if(GRANTED_MS != dev_state)
        return;
    Serial.println("Setting up the server");

    managed_devices[pair_id - 1] = (PairDevice*)LOCAL_MANAGED_DEV;

    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println("[HTTP][GET] /");
            request->send(200, "text/html", index_html);
            // request->send(LittleFS, "/index.html", "text/html");
        });

    server.on(
        "/style.css",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println("[HTTP][GET] /style.css");
            request->send(200, "text/css", index_css);
            // request->send(LittleFS, "/style.css", "text/css");
        });

    server.on(
        "/index.js",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println("[HTTP][GET] /index.js");
            request->send(200, "text/javascript", index_js);
            // request->send(LittleFS, "/index.js", "text/javascript");
        });

    server.on(
        "/update",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.print("[HTTP][GET] ");
            Serial.println(request->url());
            if(request->hasParam(PARAM_INPUT_RELAY) & request->hasParam(PARAM_INPUT_STATE)) {
                process_relay_action(
                    request->getParam(PARAM_INPUT_RELAY)->value().toInt()-1,
                    request->getParam(PARAM_INPUT_STATE)->value()
                );
            }

            else if(request->hasParam(PARAM_INPUT_STATE)) {
                for(uint8_t dev_id = 0; dev_id < MAX_MANAGED_DEV; dev_id++) {
                    if(managed_devices[dev_id] != 0x0) continue;
                    process_relay_action(
                        dev_id,
                        request->getParam(PARAM_INPUT_STATE)->value()
                    );
                }
            }

            request->send(200, "text/plain", "OK");
        });

    server.on(
        "/relays",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            StaticJsonDocument<200> doc;
            String respone;
            for(uint8_t dev_id = 0; dev_id < MAX_MANAGED_DEV; dev_id++) {
                if(managed_devices[dev_id] != 0x0) {
                    doc[String(dev_id + 1)] = status_devices[dev_id] ? "on": "off";
                }
            }
            serializeJson(doc, respone);
            request->send(200, "application/json", respone);
        });

    events.onConnect([](AsyncEventSourceClient *client) {
        if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 10000);
    });

    server.addHandler(&events);
    server.begin();

    dev_state = READY_MS;
    Serial.println("Server ready ... ");
}

void setup()
{
    Serial.begin(SERIAL_BOUND);

    Serial.println("Start");

    pinMode(LED_BUILTIN, OUTPUT); // Initialize the LED_BUILTIN pin as an output
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(PIN_RELAY, OUTPUT); // Initialize the PIN_RELAY pin as an output
    network_init();             // Initialize the network
    server_init();              // Initialize the server
}

// the loop function runs over and over again forever
void loop()
{
    if(READY_MS != dev_state) {
        return;
    }

    if ((millis() - ts_last) > DELAY_TIME) {
        // Send Events to the Web Server with the Sensor Readings
        Serial.println("Event update sending ...");
        events.send("ping",NULL,millis());
        uint8_t dev_id = 0;
        for(dev_id = 0; dev_id < MAX_MANAGED_DEV; dev_id++) {
            if(managed_devices[dev_id] != 0x0) {
                events.send(String(dev_id + 1).c_str(), status_devices[dev_id]? "relay_on" : "relay_off", millis());
            }
        }
        ts_last = millis();
    }
}
