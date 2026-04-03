#include "dlms_meter_lib.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#include <cstdio>

namespace esphome::dlms_meter_lib
{
static constexpr const char *TAG = "dlms_meter_lib";

static void log_callback(dlms_parser::LogLevel level, const char *fmt, va_list args) {
  static char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  switch (level) {
  case dlms_parser::LogLevel::ERROR:
    ESP_LOGE(TAG, "%s", buf);
    break;
  case dlms_parser::LogLevel::WARNING:
    ESP_LOGW(TAG, "%s", buf);
    break;
  case dlms_parser::LogLevel::INFO:
    ESP_LOGI(TAG, "%s", buf);
    break;
  case dlms_parser::LogLevel::VERBOSE:
    ESP_LOGV(TAG, "%s", buf);
    break;
  default:
    ESP_LOGVV(TAG, "%s", buf);
    break;
  }
}

void DlmsMeterLibComponent::setup() {
  dlms_parser::Logger::set_log_function(log_callback);
  this->parser_ = std::make_unique<dlms_parser::DlmsParser>();
  this->parser_->load_default_patterns();

  this->rx_buffer_ = std::make_unique<uint8_t[]>(MAX_RX_BUFFER_SIZE);
  this->rx_buffer_len_ = 0;
}

void DlmsMeterLibComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DLMS METER LIB Component:");
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_ms_);
  if (!this->custom_pattern_.empty()) {
    ESP_LOGCONFIG(TAG, "  Custom Pattern: %s", this->custom_pattern_.c_str());
  }

#ifdef USE_SENSOR
  for (const auto &entry : this->sensors_) {
    LOG_SENSOR("  ", "Numeric Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis.c_str());
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (const auto &entry : this->text_sensors_) {
    LOG_TEXT_SENSOR("  ", "Text Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis.c_str());
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (const auto &entry : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Binary Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis.c_str());
  }
#endif
}

void DlmsMeterLibComponent::set_decryption_key(const char *hex_key) {
  auto key = dlms_parser::Aes128GcmDecryptionKey::from_hex(hex_key);
  if (key) {
    this->parser_->set_decryption_key(*key);
  } else {
    ESP_LOGE(TAG, "Invalid decryption key");
  }
}

void DlmsMeterLibComponent::loop() {
  this->read_rx_buffer_();
    if (this->receiving_ && App.get_loop_component_start_time() - this->last_rx_char_time_ > this->receive_timeout_ms_) {
    this->process_frame_();
  }
}

void DlmsMeterLibComponent::read_rx_buffer_() {
  int available = this->available();
  if (available == 0)
    return;

  this->receiving_ = true;
  this->last_rx_char_time_ = App.get_loop_component_start_time();

  while (this->available()) {
    if (this->rx_buffer_len_ >= MAX_RX_BUFFER_SIZE) {
      ESP_LOGW(TAG, "RX Buffer overflow. Frame too large! Truncating.");
      break;
    }

    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->rx_buffer_[this->rx_buffer_len_++] = byte;
    } else {
      ESP_LOGW(TAG, "Failed to read byte from UART.");
      break;
    }
  }
}

void DlmsMeterLibComponent::process_frame_() {
  if (this->rx_buffer_len_ == 0)
    return;

  ESP_LOGD(TAG, "PUSH frame size: %zu bytes", this->rx_buffer_len_);

  auto callback = [this](const char *obis_code, float float_val, const char *str_val, bool is_numeric) {
    this->on_data_(obis_code, float_val, str_val, is_numeric);
  };

  this->parser_->parse(std::span<uint8_t>(this->rx_buffer_.get(), this->rx_buffer_len_), callback);
  this->rx_buffer_len_ = 0;
}

void DlmsMeterLibComponent::on_data_(const char *obis_code, float float_val, const char *str_val, bool is_numeric) {
  int updated_count = 0;

#ifdef USE_SENSOR
  if (is_numeric) {
    for (const auto &entry : this->sensors_) {
      if (entry.obis == obis_code) {
        ESP_LOGD(TAG, "Found sensor for OBIS code %s: '%s'", obis_code, entry.sensor->get_name().c_str());
        ESP_LOGD(TAG, "Publishing data");
        entry.sensor->publish_state(float_val);
        updated_count++;
      }
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
  for (const auto &entry : this->text_sensors_) {
    if (entry.obis == obis_code) {
      ESP_LOGD(TAG, "Found sensor for OBIS code %s: '%s'", obis_code, entry.sensor->get_name().c_str());
      ESP_LOGD(TAG, "Publishing data");
      entry.sensor->publish_state(str_val);
      updated_count++;
    }
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (is_numeric) {
    bool state = float_val != 0.0f;
    for (const auto &entry : this->binary_sensors_) {
      if (entry.obis == obis_code) {
        ESP_LOGD(TAG, "Found sensor for OBIS code %s: '%s'", obis_code, entry.sensor->get_name().c_str());
        ESP_LOGD(TAG, "Publishing data");
        entry.sensor->publish_state(state);
        updated_count++;
      }
    }
  }
#endif

  if (updated_count == 0) {
    ESP_LOGV(TAG, "Received OBIS %s, but no sensors are registered for it.", obis_code);
  }
}


#ifdef USE_SENSOR
void DlmsMeterLibComponent::register_sensor(const std::string &obis_code, sensor::Sensor *sensor) {
  this->sensors_.push_back({obis_code, sensor});
}
#endif
#ifdef USE_TEXT_SENSOR
void DlmsMeterLibComponent::register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor) {
  this->text_sensors_.push_back({obis_code, sensor});
}
#endif
#ifdef USE_BINARY_SENSOR
void DlmsMeterLibComponent::register_binary_sensor(const std::string &obis_code, binary_sensor::BinarySensor *sensor) {
  this->binary_sensors_.push_back({obis_code, sensor});
}
#endif
}  // namespace esphome::dlms_meter