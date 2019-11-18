#include <Arduino.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <PubSubClient.h>
#include "dsmr.h"
#include "secrets.h"

// project stuff
#define PURPOSE "reading p1 messages and ship them to mqtt in json"
#define VERBOSE 0

// board configuration
#define DEFAULT_BAUDRATE 115200
#define READER_UART 2
#define RX_PIN 16
#define TX_PIN 17
#define RTS_PIN 4
#define COOLDOWN 10 * 1000    // time in msec between reading RX_PIN for messages

#define WIFI_WAIT 10 * 1000   // time in msec to wait before checking wifi
int status = WL_IDLE_STATUS;                        // wifi status
unsigned long check_wifi = WIFI_WAIT;
unsigned long last;                                 // timestamp of last read telegram
const size_t capacity = JSON_OBJECT_SIZE(24) + 660; // json document size

// setup connectivity
WiFiClient espClient; 
PubSubClient client(espClient);

// Create Serial1 connected to UART 1
HardwareSerial Serial_in(2);
P1Reader reader(&Serial_in, RTS_PIN);


// mqtt connectivity
void check_connect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// write_to_mqtt
void write_to_mqtt(String message, String topic=MQTT_TOPIC) {
  int topic_size = topic.length() + 1;
  int message_size = message.length() +1;

  char local_topic[topic_size];
  char local_message[message_size];

  topic.toCharArray(local_topic,topic_size);
  message.toCharArray(local_message,message_size);

  check_connect_mqtt();

  if (VERBOSE) {
    Serial.println((String)"sending msg to " + local_topic + ":");
    Serial.println(message);
  }

  if (  client.publish(local_topic, local_message, message_size) == true) {
    Serial.println("Success sending message");
  } else {
    Serial.print("Error sending message: ");
    Serial.println(client.getWriteError());
  }
}

/**
 * Define the data we're interested in, as well as the datastructure to
 * hold the parsed data. This list shows all supported fields, remove
 * any fields you are not using from the below list to make the parsing
 * and printing code smaller.
 * Each template argument below results in a field of the same name.
 */
using MyData = ParsedData<
  /* String */ identification,
  /* String */ p1_version,
  /* String */ timestamp,
  /* String */ equipment_id,
  /* FixedValue */ energy_delivered_tariff1,
  /* FixedValue */ energy_delivered_tariff2,
  /* FixedValue */ energy_returned_tariff1,
  /* FixedValue */ energy_returned_tariff2,
  /* String */ electricity_tariff,
  /* FixedValue */ power_delivered,
  /* FixedValue */ power_returned,
  /* FixedValue */ electricity_threshold,
  /* uint8_t */ electricity_switch_position,
  /* uint32_t */ electricity_failures,
  /* uint32_t */ electricity_long_failures,
  /* String */ electricity_failure_log,
  /* uint32_t */ electricity_sags_l1,
  /* uint32_t */ electricity_sags_l2,
  /* uint32_t */ electricity_sags_l3,
  /* uint32_t */ electricity_swells_l1,
  /* uint32_t */ electricity_swells_l2,
  /* uint32_t */ electricity_swells_l3,
  /* String */ message_short,
  /* String */ message_long,
  /* FixedValue */ voltage_l1,
  /* FixedValue */ voltage_l2,
  /* FixedValue */ voltage_l3,
  /* FixedValue */ current_l1,
  /* FixedValue */ current_l2,
  /* FixedValue */ current_l3,
  /* FixedValue */ power_delivered_l1,
  /* FixedValue */ power_delivered_l2,
  /* FixedValue */ power_delivered_l3,
  /* FixedValue */ power_returned_l1,
  /* FixedValue */ power_returned_l2,
  /* FixedValue */ power_returned_l3,
  /* uint16_t */ gas_device_type,
  /* String */ gas_equipment_id,
  /* uint8_t */ gas_valve_position,
  /* TimestampedFixedValue */ gas_delivered,
  /* uint16_t */ thermal_device_type,
  /* String */ thermal_equipment_id,
  /* uint8_t */ thermal_valve_position,
  /* TimestampedFixedValue */ thermal_delivered,
  /* uint16_t */ water_device_type,
  /* String */ water_equipment_id,
  /* uint8_t */ water_valve_position,
  /* TimestampedFixedValue */ water_delivered,
  /* uint16_t */ slave_device_type,
  /* String */ slave_equipment_id,
  /* uint8_t */ slave_valve_position,
  /* TimestampedFixedValue */ slave_delivered
>;

/**
 * This illustrates looping over all parsed fields using the
 * ParsedData::applyEach method.
 *
 * When passed an instance of this Printer object, applyEach will loop
 * over each field and call Printer::apply, passing a reference to each
 * field in turn. This passes the actual field object, not the field
 * value, so each call to Printer::apply will have a differently typed
 * parameter.
 *
 * For this reason, Printer::apply is a template, resulting in one
 * distinct apply method for each field used. This allows looking up
 * things like Item::name, which is different for every field type,
 * without having to resort to virtual method calls (which result in
 * extra storage usage). The tradeoff is here that there is more code
 * generated (but due to compiler inlining, it's pretty much the same as
 * if you just manually printed all field names and values (with no
 * cost at all if you don't use the Printer).
 */
struct Printer {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
      Serial.print(Item::name);
      Serial.print(F(": "));
      Serial.print(i.val());
      Serial.print(Item::unit());
      Serial.println();
    }
  }
};

void processData(MyData DSMR_data) {
  StaticJsonDocument <capacity> doc;

  if ( DSMR_data.identification_present) {
    doc["identification"] = DSMR_data.identification;
  }
  if ( DSMR_data.p1_version_present) {
    doc["p1_version"] = DSMR_data.p1_version;


  }
  if ( DSMR_data.timestamp_present) {
    doc["timestamp"] = DSMR_data.timestamp;
  }
  if ( DSMR_data.equipment_id_present) {
    doc["equipment_id"] = DSMR_data.equipment_id;
  }
  if ( DSMR_data.energy_delivered_tariff1_present) {
   doc["energy_delivered_tariff1"] = DSMR_data.energy_delivered_tariff1.val();
  }
  if ( DSMR_data.energy_delivered_tariff2_present) {
    doc["energy_delivered_tariff2"] = DSMR_data.energy_delivered_tariff2.val();
  }
  if ( DSMR_data.energy_returned_tariff1_present) {
    doc["energy_returned_tariff1"] = DSMR_data.energy_returned_tariff1.val();
  }
  if ( DSMR_data.energy_returned_tariff2_present) {
    doc["energy_returned_tariff2"] = DSMR_data.energy_returned_tariff2.val();
  }
  if ( DSMR_data.electricity_tariff_present) {
    doc["electricity_tariff"] = DSMR_data.electricity_tariff;
  }
  if ( DSMR_data.power_delivered_present) {
    doc["power_delivered"] = DSMR_data.power_delivered.val();
  }
  if ( DSMR_data.power_returned_present) {
    doc["power_returned"] = DSMR_data.power_returned.val();
  }
  if ( DSMR_data.electricity_failures_present) {
    doc["electricity_failures"] = DSMR_data.electricity_failures;
  }
  if ( DSMR_data.electricity_long_failures_present) {
    doc["electricity_long_failures"] = DSMR_data.electricity_long_failures;
  }
  if ( DSMR_data.electricity_failure_log_present) {
    doc["electricity_failure_log"] = DSMR_data.electricity_failure_log;
  }
  if ( DSMR_data.electricity_sags_l1_present) {
    doc["electricity_sags_l1"] = DSMR_data.electricity_sags_l1;
  }
  if ( DSMR_data.electricity_swells_l1_present) {
    doc["electricity_swells_l1"] = DSMR_data.electricity_swells_l1;
  }
  if ( DSMR_data.message_long_present) {
    doc["message_long"] = DSMR_data.message_long;
  }
  if ( DSMR_data.voltage_l1_present) {
    doc["voltage_l1"] = DSMR_data.voltage_l1.val();
  }
  if ( DSMR_data.current_l1_present) {
    doc["current_l1"] = DSMR_data.current_l1;
  }
  if ( DSMR_data.power_delivered_l1_present) {
    doc["power_delivered_l1"] = DSMR_data.power_delivered_l1.val();
  }
  if ( DSMR_data.power_returned_l1_present) {
    doc["power_returned_l1"] = DSMR_data.power_returned_l1.val();
  }
  if ( DSMR_data.gas_device_type_present) {
    doc["gas_device_type"] = DSMR_data.gas_device_type;
  }
  if ( DSMR_data.gas_equipment_id_present) {
    doc["gas_equipment_id"] = DSMR_data.gas_equipment_id;
  }
  if ( DSMR_data.gas_delivered_present) {
    doc["gas_delivered"] = DSMR_data.gas_delivered.val();
  }

  // serialize json document into String
  String json_output;
  serializeJsonPretty(doc, json_output);

  if (VERBOSE) {
    Serial.println(json_output);
    Serial.print("size of document: ");
    Serial.println(json_output.length());
  }

  write_to_mqtt(json_output);
}


// simple led flash function
void flash_led(int led_pin=LED_BUILTIN, int led_on_msec=100) {
  digitalWrite(led_pin, HIGH);
  delay(led_on_msec);
  digitalWrite(led_pin, LOW); 
}


void connect_wifi() {
  while (status != WL_CONNECTED) {
    Serial.print("connecting to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, password);

    // wait 10 seconds for connection:
    delay(10000);
  }
}


void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("Connected to AP!");
 
    Serial.print("SSID: ");
    for(int i=0; i<info.connected.ssid_len; i++){
      Serial.print((char) info.connected.ssid[i]);
    }
 
    Serial.print("\nBSSID: ");
    for(int i=0; i<6; i++){
      Serial.printf("%02X", info.connected.bssid[i]);
 
      if(i<5){
        Serial.print(":");
      }
    }
    Serial.println();
} 


void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
}


void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("ja lekker, ben disconnected");
  // WiFi.disconnect();
  // connect_wifi();
}


void setup_wifi() {
  WiFi.setHostname(hostname);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  connect_wifi();
}


void setup() {
  // show that we're alive
  pinMode(LED_BUILTIN, OUTPUT); 
  flash_led();

  Serial.begin(DEFAULT_BAUDRATE);
  Serial.println((String)"project: " + PROJECT);
  Serial.println((String)"version: " + VERSION);
  Serial.println((String)"git-rev: " + PIO_SRC_REV);
  Serial.println(PURPOSE);

  Serial_in.begin(DEFAULT_BAUDRATE);
  Serial_in.begin(DEFAULT_BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println((String)"setup: serial_reader rx/tx enabled on: " + RX_PIN + "/" + TX_PIN);

  // setup wireless connection
  setup_wifi();

  // setup mqtt client
  client.setServer(MQTT_HOST, MQTT_PORT);

  // start a read right away
  reader.enable(true);
  last = millis();
}


void loop () {
  // Allow the reader to check the serial buffer regularly
  reader.loop();

  // Every minute, fire off a one-off reading
  unsigned long now = millis();
  if (now - last > COOLDOWN) {
    reader.enable(true);
    last = now;
  }

  if (reader.available()) {
    MyData data;
    String err;
    if (reader.parse(&data, &err)) {
      // print telegram to serial port
      if (VERBOSE) {
        data.applyEach(Printer());
      }
      flash_led();
      processData(data);
    } else {
      // Parser error, print error
      Serial.println(err);
    }
  }
}
