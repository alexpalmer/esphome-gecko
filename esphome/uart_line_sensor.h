#include "esphome.h"

class UartLineReader : public Component, public TextSensor, public UARTDevice {
 public:
  UartLineReader(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override {
    ESP_LOGI("uart_reader", "UART Line Reader initialized");
  }

  void loop() override {
    while (available()) {
      char c = read();
      if (c == '\n' || c == '\r') {
        if (buffer_.length() > 0) {
          publish_state(buffer_);
          buffer_.clear();
        }
      } else if (buffer_.length() < 128) {
        buffer_ += c;
      }
    }
  }

 private:
  std::string buffer_;
};
