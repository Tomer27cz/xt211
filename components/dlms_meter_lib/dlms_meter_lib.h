#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <dlms_parser/dlms_parser.h>
#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_bearssl.h>
#elif defined(USE_ESP32)
#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_tfpsa.h>
#else
#include <dlms_parser/decryption/aes_128_gcm_decryptor_mbedtls.h>
#endif
#else
#error "Unsupported platform for dlms_meter_lib"
#endif

#include <vector>
#include <string>
#include <array>
#include <memory>

namespace esphome::dlms_meter_lib {

class DlmsMeterLibComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterLibComponent() = default;

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_decryption_key(const char *hex_key);
  void set_receive_timeout(uint32_t timeout_ms) { this->receive_timeout_ms_ = timeout_ms; }
  void set_custom_pattern(const std::string &pattern) { this->custom_pattern_ = pattern; }

#ifdef USE_SENSOR
  void register_sensor(const std::string &obis_code, sensor::Sensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const std::string &obis_code, binary_sensor::BinarySensor *sensor);
#endif

 protected:
  void read_rx_buffer_();
  void process_frame_();

  void on_data_(const char *obis_code, float float_val, const char *str_val, bool is_numeric);

  static constexpr size_t MAX_RX_BUFFER_SIZE = 2048;
  std::unique_ptr<uint8_t[]> rx_buffer_;
  size_t rx_buffer_len_{0};
  uint32_t last_rx_char_time_{0};
  bool receiving_{false};

  uint32_t receive_timeout_ms_{50};
  std::string custom_pattern_{""};
  std::unique_ptr<dlms_parser::DlmsParser> parser_;

#ifdef USE_SENSOR
  struct NumericSensorEntry {
    std::string obis;
    sensor::Sensor *sensor;
  };
  std::vector<NumericSensorEntry> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  struct TextSensorEntry {
    std::string obis;
    text_sensor::TextSensor *sensor;
  };
  std::vector<TextSensorEntry> text_sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  struct BinarySensorEntry {
    std::string obis;
    binary_sensor::BinarySensor *sensor;
  };
  std::vector<BinarySensorEntry> binary_sensors_;
#endif
};

}  // namespace esphome::dlms_meter_lib

