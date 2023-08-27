// #include <Adafruit_TinyUSB.h>
#include <stdio.h>

#include <BLEMIDI_Transport.h>
//#include <hardware/BLEMIDI_ESP32_NimBLE.h>
#include "blemidi_esp32_nimble_custom.hpp"
#include <NimBLELog.h>

/*
// Initialize USB MIDI
struct USBInitializer {
  USBInitializer() {
    USB.productName("ESP32 BLE MIDI");
    USB.manufacturerName("mesotokyo");
  }
} usbInitializer;

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidi, MIDI_USB);
*/

// Initialize BLE MIDI

// create custom server handler
class CustomServerCallbacks;
class CustomCharacteristicCallbacks;
typedef BLEMIDI_ESP32_NimBLE_Custom<CustomServerCallbacks, CustomCharacteristicCallbacks> NimBLE_Custom;

class CustomServerCallbacks : public BLEServerCallbacks {
public:
  CustomServerCallbacks(NimBLE_Custom *bluetoothEsp32)
    : _bluetoothEsp32(bluetoothEsp32) {}

protected:
  NimBLE_Custom *_bluetoothEsp32 = nullptr;

  void onConnect(BLEServer *pServer, ble_gap_conn_desc* desc) {
    if (_bluetoothEsp32) {
      _bluetoothEsp32->connected();
    }
    /*
      int16_t conn_handle,
      uint16_t minInterval // unit: 1.25ms
      uint16_t maxInterval, // unit: 1.25ms
      uint16_t latency, // unit: intervals
      uint16_t timeout // unit: 10ms, 5x interval is best
      see: https://mynewt.apache.org/latest/network/ble_hs/ble_gap.html#c.ble_gap_upd_params
     */
    log_i("interval: %d, timeout: %d, latency: %d on startup",
          desc->conn_itvl, desc->supervision_timeout, desc->conn_latency);

    //pServer->updateConnParams(desc->conn_handle, 15, 15, 0, 400);
    //pServer->updateConnParams(desc->conn_handle, 6, 8, 0, 400);
  };

  void onDisconnect(BLEServer *) {
    if (_bluetoothEsp32)
      _bluetoothEsp32->disconnected();
  }
};

class CustomCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  CustomCharacteristicCallbacks(NimBLE_Custom *bluetoothEsp32)
    : _bluetoothEsp32(bluetoothEsp32) {}

protected:
  NimBLE_Custom *_bluetoothEsp32 = nullptr;

  void onWrite(BLECharacteristic *characteristic) {
    std::string rxValue = characteristic->getValue();
    if (rxValue.length() > 0) {
      _bluetoothEsp32->receive((uint8_t *)(rxValue.c_str()), rxValue.length());
    }
  }
  void onSubscribe(BLECharacteristic* pCharacteristic,
                     ble_gap_conn_desc* desc,
                     uint16_t subValue) {
    log_i("interval: %d, timeout: %d, latency: %d on subscribe",
          desc->conn_itvl, desc->supervision_timeout, desc->conn_latency);
    
  }
};

// The following code is almost the same as BLEMIDI_CREATE_INSTANCE("EPS32_BLE_MIDI", MIDI_BLE) macro, but create BLEMIDI_Transport as `bleDevice`.
typedef bleMidi::BLEMIDI_Transport<NimBLE_Custom> NimBLE_Transport;
NimBLE_Transport  bleDevice("EPS32_BLE_MIDI");
midi::MidiInterface<NimBLE_Transport, bleMidi::MySettings> MIDI_BLE(bleDevice);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  bleDevice.setHandleConnected(OnConnected);
  bleDevice.setHandleDisconnected(OnDisconnected);

  /*
  while (!TinyUSBDevice.mounted()) {
    delay(10);
  }
  */
  MIDI_BLE.begin(MIDI_CHANNEL_OMNI);
  //MIDI_USB.begin(MIDI_CHANNEL_OMNI);

  MIDI_BLE.setHandleNoteOn(bleHandleNoteOn);
  MIDI_BLE.setHandleNoteOff(bleHandleNoteOff);
  //MIDI_USB.setHandleNoteOn(usbHandleNoteOn);
  //MIDI_USB.setHandleNoteOff(usbHandleNoteOff);
  
  log_i("start");
}

void OnConnected() {
  digitalWrite(LED_BUILTIN, LOW);
  log_i("connect\n");
}

void OnDisconnected() {
  digitalWrite(LED_BUILTIN, HIGH);
  log_i("disconnect");
}
 
void loop()
{
  MIDI_BLE.read();
  //MIDI_USB.read();
  delay(10);
}

void usbHandleNoteOff(byte channel, byte note, byte velocity) {
  MIDI_BLE.sendNoteOff(note, velocity, channel);
}

void usbHandleNoteOn(byte channel, byte note, byte velocity) {
  MIDI_BLE.sendNoteOn(note, velocity, channel);
}

void bleHandleNoteOff(byte channel, byte note, byte velocity) {
  //MIDI_USB.sendNoteOff(note, velocity, channel);
}

void bleHandleNoteOn(byte channel, byte note, byte velocity) {
  //MIDI_USB.sendNoteOn(note, velocity, channel);
}
