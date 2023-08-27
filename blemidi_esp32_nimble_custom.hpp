#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32_NimBLE.h>

template<class _SCallbacks, class _CCallbacks>
class BLEMIDI_ESP32_NimBLE_Custom
{
private:
    BLEServer *_server = nullptr;
    BLEAdvertising *_advertising = nullptr;
    BLECharacteristic *_characteristic = nullptr;

    bleMidi::BLEMIDI_Transport<class BLEMIDI_ESP32_NimBLE_Custom<_SCallbacks, _CCallbacks>> *_bleMidiTransport = nullptr;

    friend _SCallbacks;
    friend _CCallbacks;

protected:
    QueueHandle_t mRxQueue;

public:
    BLEMIDI_ESP32_NimBLE_Custom()
    {
    }

    bool begin(const char *, bleMidi::BLEMIDI_Transport<class BLEMIDI_ESP32_NimBLE_Custom<_SCallbacks, _CCallbacks>> *);

    void end()
    {
    }

    void write(uint8_t *buffer, size_t length)
    {
        _characteristic->setValue(buffer, length);
        _characteristic->notify();
    }

    bool available(byte *pvBuffer)
    {
        // return 1 byte from the Queue
        return xQueueReceive(mRxQueue, (void *)pvBuffer, 0); // return immediately when the queue is empty
    }

    void add(byte value)
    {
        // called from BLE-MIDI, to add it to a buffer here
        xQueueSend(mRxQueue, &value, portMAX_DELAY);
    }

protected:
    void receive(uint8_t *buffer, size_t length)
    {
        // forward the buffer so it can be parsed
        _bleMidiTransport->receive(buffer, length);
    }

    void connected()
    {
        if (_bleMidiTransport->_connectedCallback)
            _bleMidiTransport->_connectedCallback();
    }

    void disconnected()
    {
        if (_bleMidiTransport->_disconnectedCallback)
            _bleMidiTransport->_disconnectedCallback();
    }
};

class MyCharacteristicCallbacks;
class MyServerCallbacks : public BLEServerCallbacks {
public:
  MyServerCallbacks(BLEMIDI_ESP32_NimBLE_Custom<MyServerCallbacks, MyCharacteristicCallbacks> *bluetoothEsp32)
    : _bluetoothEsp32(bluetoothEsp32) {}

protected:
  BLEMIDI_ESP32_NimBLE_Custom<MyServerCallbacks, MyCharacteristicCallbacks> *_bluetoothEsp32 = nullptr;

  void onConnect(BLEServer *) {
    if (_bluetoothEsp32)
      _bluetoothEsp32->connected();
  };

  void onDisconnect(BLEServer *) {
    if (_bluetoothEsp32)
      _bluetoothEsp32->disconnected();
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  MyCharacteristicCallbacks(BLEMIDI_ESP32_NimBLE_Custom<MyServerCallbacks, MyCharacteristicCallbacks> *bluetoothEsp32)
    : _bluetoothEsp32(bluetoothEsp32) {}

protected:
  BLEMIDI_ESP32_NimBLE_Custom<MyServerCallbacks, MyCharacteristicCallbacks> *_bluetoothEsp32 = nullptr;

  void onWrite(BLECharacteristic *characteristic) {
    std::string rxValue = characteristic->getValue();
    if (rxValue.length() > 0) {
      _bluetoothEsp32->receive((uint8_t *)(rxValue.c_str()), rxValue.length());
    }
  }
};


template<class _SCallbacks, class _CCallbacks>
bool BLEMIDI_ESP32_NimBLE_Custom<_SCallbacks, _CCallbacks>::begin(const char *deviceName, bleMidi::BLEMIDI_Transport<class BLEMIDI_ESP32_NimBLE_Custom<_SCallbacks, _CCallbacks>> *bleMidiTransport)
{
    _bleMidiTransport = bleMidiTransport;

    BLEDevice::init(deviceName);

    // To communicate between the 2 cores.
    // Core_0 runs here, core_1 runs the BLE stack
    mRxQueue = xQueueCreate(64, sizeof(uint8_t)); // TODO Settings::MaxBufferSize

    _server = BLEDevice::createServer();
    _server->setCallbacks(new _SCallbacks(this));
    _server->advertiseOnDisconnect(true);

    // Create the BLE Service
    auto service = _server->createService(BLEUUID(bleMidi::SERVICE_UUID));

    // Create a BLE Characteristic
    _characteristic = service->createCharacteristic(
        BLEUUID(bleMidi::CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::WRITE_NR);

    _characteristic->setCallbacks(new _CCallbacks(this));

    auto _security = new NimBLESecurity();
    _security->setAuthenticationMode(ESP_LE_AUTH_BOND);

    // Start the service
    service->start();

    // Start advertising
    _advertising = _server->getAdvertising();
    _advertising->addServiceUUID(service->getUUID());
    _advertising->setAppearance(0x00);
    _advertising->start();

    return true;
}
