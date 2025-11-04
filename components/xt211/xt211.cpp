#include "xt211.h"
#include "xt211_axdr_parser.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <sstream>

namespace esphome {
  namespace xt211 {

    static const char *TAG0 = "xt211_";

#define TAG (this->tag_.c_str())

    static constexpr uint8_t
    BOOT_WAIT_S = 10;

    void XT211Component::setup() {
      ESP_LOGD(TAG, "setup (PUSH mode only)");

      this->buffers_.init(DEFAULT_IN_BUF_SIZE_PUSH);

#ifdef USE_ESP32
      iuart_ = make_unique<XT211Uart>(*static_cast<uart::IDFUARTComponent *>(this->parent_));
#endif

#if USE_ESP8266
      iuart_ = make_unique<XT211Uart>(*static_cast<uart::ESP8266UartComponent *>(this->parent_));
#endif

      this->set_baud_rate_(this->baud_rate_);

      CosemObjectFoundCallback fn = [this](auto... args) { (void) this->set_sensor_value(args...); };

      this->axdr_parser_ = new AxdrStreamParser(&this->buffers_.in, fn, this->push_show_log_);

      // default patterns
      this->axdr_parser_->register_pattern_dsl("T1", "TC,TO,TS,TV");
      this->axdr_parser_->register_pattern_dsl("T2", "TO,TV,TSU");
      this->axdr_parser_->register_pattern_dsl("T3", "TV,TC,TSU,TO");
      this->axdr_parser_->register_pattern_dsl("U.ZPA", "F,C,O,A,TV");

      // user-provided pattern
      if (this->push_custom_pattern_dsl_.length() > 0) {
        this->axdr_parser_->register_pattern_dsl("CUSTOM", this->push_custom_pattern_dsl_, 0);
      }

      bool locked = false;
      for (int i = 0; i < 3; i++)
        if (this->try_lock_uart_session_()) {
          locked = true;
          break;
        }

      if (!locked) {
        ESP_LOGE(TAG, "Failed to lock UART session. Aborting setup.");
        this->mark_failed();
        return;
      }

      this->set_timeout(BOOT_WAIT_S * 1000, [this]() {
        ESP_LOGD(TAG, "Boot timeout, component is ready to use");
        this->clear_rx_buffers_();
        this->set_next_state_(State::IDLE);
      });
    }

    void XT211Component::dump_config() {
      ESP_LOGCONFIG(TAG, "XT211 (PUSH Mode Read-Only):");
      ESP_LOGCONFIG(TAG, "  Receive Timeout: %ums", this->receive_timeout_ms_);
      ESP_LOGCONFIG(TAG, "  Mode: PUSH Data Reception");
      ESP_LOGCONFIG(TAG, "  Sensors:");
      for (const auto &sensors: sensors_) {
        auto &s = sensors.second;
        ESP_LOGCONFIG(TAG, "    OBIS code: %s, Name: %s", s->get_obis_code().c_str(), s->get_sensor_name().c_str());
      }
    }

    void XT211Component::register_sensor(XT211SensorBase *sensor) {
      this->sensors_.insert({sensor->get_obis_code(), sensor});
    }

    void XT211Component::abort_mission_() {
      ESP_LOGV(TAG, "Push mode error, returning to listening");
      this->clear_rx_buffers_();
      this->set_next_state_(State::IDLE);
    }

    void XT211Component::report_failure(bool failure) {
      if (!failure) {
        this->stats_.failures_ = 0;
        return;
      }

      this->stats_.failures_++;
      ESP_LOGE(TAG, "Failure reported. Count: %u", this->stats_.failures_);
    }

    void XT211Component::loop() {
      if (!this->is_ready() || this->state_ == State::NOT_INITIALIZED)
        return;

      switch (this->state_) {
        case State::IDLE: {
          this->update_last_rx_time_();
          this->indicate_transmission(false);
          this->indicate_session(false);

          // Push mode listening logic
          if (this->available() > 0) {
            // Set up for receiving push data
            memset(this->buffers_.in.data, 0, buffers_.in.capacity);
            this->buffers_.in.size = 0;
            // read what we can then move forward to avoid buffer overflow
            this->receive_frame_raw_();

            ESP_LOGV(TAG, "Push mode: incoming data detected");
            this->stats_.connections_tried_++;
            this->loop_state_.session_started_ms = millis();

            this->indicate_transmission(true);
            this->set_next_state_(State::COMMS_RX);
          }
        }
          break;

        case State::WAIT:
          if (this->check_wait_timeout_()) {
            this->set_next_state_(this->wait_.next_state);
            this->update_last_rx_time_();
          }
          break;

        case State::COMMS_RX: {
          this->handle_comms_rx_();
        }
          break;

        case State::MISSION_FAILED: {
          this->set_next_state_(State::IDLE);
          this->report_failure(true);
          this->stats_dump();
        }
          break;

        case State::PUSH_DATA_PROCESS: {
          this->handle_push_data_process_();
        }
          break;

        case State::PUBLISH: {
          this->handle_publish_();
        }
          break;

        default:
          // Should not happen
          ESP_LOGW(TAG, "Unhandled state: %s", state_to_string(this->state_));
          this->set_next_state_(State::IDLE);
          break;
      }
    }

    void XT211Component::handle_comms_rx_() {
      this->log_state_();

      if (this->check_rx_timeout_()) {
        ESP_LOGI(TAG, "Push data reception completed (timeout reached)");

        this->indicate_connection(false);
        this->indicate_transmission(false);

        // check if we received any data at all
        this->indicate_connection(true);
        if (this->buffers_.in.size > 0) {
          ESP_LOGV(TAG, "Push mode RX data avail, len=%d", this->buffers_.in.size);
          this->set_next_state_(State::PUSH_DATA_PROCESS);
        } else {
          ESP_LOGV(TAG, "Push mode RX timeout, no data, idling");
          this->set_next_state_(State::IDLE);
        }
        return;
      }

      received_frame_size_ = this->receive_frame_raw_();
      //  keep reading until timeout
    }

    void XT211Component::handle_push_data_process_() {
      this->log_state_();
      ESP_LOGD(TAG, "Processing received push data");
      this->loop_state_.sensor_iter = this->sensors_.begin();
      this->set_next_state_(State::PUBLISH);
      this->process_push_data();
      this->clear_rx_buffers_();
    }

    void XT211Component::handle_publish_() {
      this->log_state_();
      ESP_LOGD(TAG, "Publishing data");
      this->update_last_rx_time_();

      if (this->loop_state_.sensor_iter != this->sensors_.end()) {
        if (this->loop_state_.sensor_iter->second->shall_we_publish()) {
          this->loop_state_.sensor_iter->second->publish();
        }
        this->loop_state_.sensor_iter++;
      } else {
        this->stats_dump();
        if (this->crc_errors_per_session_sensor_ != nullptr) {
          this->crc_errors_per_session_sensor_->publish_state(this->stats_.crc_errors_per_session());
        }
        this->report_failure(false);
        this->set_next_state_(State::IDLE);
        ESP_LOGD(TAG, "Total time: %u ms", millis() - this->loop_state_.session_started_ms);
      }
    }

// This is the entry point for PollingComponent, which is now unused.
    void XT211Component::update() {}

    void XT211Component::set_next_state_delayed_(uint32_t ms, State next_state) {
      if (ms == 0) {
        set_next_state_(next_state);
      } else {
        ESP_LOGV(TAG, "Short delay for %u ms", ms);
        set_next_state_(State::WAIT);
        wait_.start_time = millis();
        wait_.delay_ms = ms;
        wait_.next_state = next_state;
      }
    }

    void XT211Component::PushBuffers::init(size_t default_in_buf_size) {
      BYTE_BUFFER_INIT(&in);
      bb_capacity(&in, default_in_buf_size);
      this->reset();
    }

    void XT211Component::PushBuffers::reset() {
      in.size = 0;
      in.position = 0;
    }

    void XT211Component::PushBuffers::check_and_grow_input(uint16_t
    more_data) {
    const uint16_t GROW_EPSILON = 20;
    if (in.size + more_data > in.capacity) {
    ESP_LOGVV(TAG0,
    "Growing input buffer from %d to %d", in.capacity, in.size + more_data + GROW_EPSILON);
    bb_capacity(&in, in
    .size + more_data + GROW_EPSILON);
  }
}

void XT211Component::process_push_data() {
  ESP_LOGD(TAG, "Processing PUSH data frame with AXDR parser");

  // Ensure we parse from the beginning of the collected frame
  this->buffers_.in.position = 0;
  const auto total_size = this->buffers_.in.size;
  ESP_LOGD(TAG, "PUSH frame size: %u bytes", static_cast<unsigned>(total_size));

  size_t total_objects = 0;
  size_t iterations = 0;

  while (this->buffers_.in.position < this->buffers_.in.size) {
    auto before = this->buffers_.in.position;
    auto parsed_now = this->axdr_parser_->parse();
    auto after = this->buffers_.in.position;
    iterations++;

    if (parsed_now == 0 && after == before) {
      // No progress, avoid potential infinite loop on malformed frames
      ESP_LOGW(TAG, "AXDR parser made no progress at pos=%u/%u, aborting", static_cast<unsigned>(after),
               static_cast<unsigned>(this->buffers_.in.size));
      break;
    }
    total_objects += parsed_now;
    ESP_LOGV(TAG, "AXDR iteration %u: parsed=%u, pos=%u/%u, objects_total=%u", static_cast<unsigned>(iterations),
             static_cast<unsigned>(parsed_now), static_cast<unsigned>(after),
             static_cast<unsigned>(this->buffers_.in.size), static_cast<unsigned>(total_objects));
  }

  ESP_LOGD(TAG, "PUSH data parsing complete: %u objects, bytes consumed %u/%u", static_cast<unsigned>(total_objects),
           static_cast<unsigned>(this->buffers_.in.position), static_cast<unsigned>(total_size));
}

int XT211Component::set_sensor_value(uint16_t class_id, const uint8_t *obis_code, DLMS_DATA_TYPE value_type,
                                         const uint8_t *value_buffer_ptr, uint8_t value_length, const int8_t *scaler,
                                         const uint8_t *unit) {
  static char obis_buf[32];
  auto er = hlp_getLogicalNameToString(obis_code, obis_buf);

  std::string obis_str(obis_buf);

  auto range = this->sensors_.equal_range(obis_str);
  int found_count = 0;
  for (auto it = range.first; it != range.second; ++it) {
    XT211SensorBase *sensor = it->second;
    if (!sensor->shall_we_publish()) {
      continue;
    }
    ESP_LOGD(TAG, "Found sensor for OBIS code %s: '%s' ", obis_buf, sensor->get_sensor_name().c_str());
    found_count++;

#ifdef USE_SENSOR
    if (sensor->get_type() == SensorType::SENSOR) {
  float val = dlms_data_as_float(value_type, value_buffer_ptr, value_length);
  if (scaler != nullptr) {
    float scale = pow(10, *scaler);
    val *= scale;
  }
  static_cast<XT211Sensor *>(sensor)->set_value(val);
}
#endif

#ifdef USE_TEXT_SENSOR
    if (sensor->get_type() == SensorType::TEXT_SENSOR) {
  auto val = dlms_data_as_string(value_type, value_buffer_ptr, value_length);
  static_cast<XT211TextSensor *>(sensor)->set_value(val.c_str());
}
#endif

#ifdef USE_BINARY_SENSOR
    if (sensor->get_type() == SensorType::BINARY_SENSOR) {
  bool val = dlms_data_as_float(value_type, value_buffer_ptr, value_length) != 0.0f;
  static_cast<XT211BinarySensor *>(sensor)->set_value(val);
}
#endif

  }

  if (found_count == 0) {
    ESP_LOGVV(TAG, "No sensor found for OBIS code: '%s'", (char *) obis_buf);
  } else {
    ESP_LOGVV(TAG, "Updated %d sensors for OBIS code: '%s'", found_count, (char *) obis_buf);
  }

  return DLMS_ERROR_CODE_OK;
}

void XT211Component::indicate_transmission(bool transmission_on) {
#ifdef USE_BINARY_SENSOR
  if (this->transmission_binary_sensor_) {
  this->transmission_binary_sensor_->publish_state(transmission_on);
}
#endif
}

void XT211Component::indicate_session(bool session_on) {
#ifdef USE_BINARY_SENSOR
  if (this->session_binary_sensor_) {
  this->session_binary_sensor_->publish_state(session_on);
}
#endif
}

void XT211Component::indicate_connection(bool connection_on) {
#ifdef USE_BINARY_SENSOR
  if (this->connection_binary_sensor_) {
  this->connection_binary_sensor_->publish_state(connection_on);
}
#endif
}

size_t XT211Component::receive_frame_(FrameStopFunction stop_fn) {
  const uint32_t read_time_limit_ms = 45;
  size_t ret_val;

  auto count_available = this->available();
  if (count_available <= 0)
    return 0;

  uint32_t read_start = millis();
  uint8_t * p;

  // ESP_LOGVV(TAG, "avail RX: %d", count_available);
  buffers_.check_and_grow_input(count_available);

  while (count_available-- > 0) {
    if (millis() - read_start > read_time_limit_ms) {
      return 0;
    }

    p = &this->buffers_.in.data[this->buffers_.in.size];
    if (!iuart_->read_one_byte(p)) {
      return 0;
    }
    this->buffers_.in.size++;

    if (stop_fn(this->buffers_.in.data, this->buffers_.in.size)) {
      ESP_LOGVV(TAG, "RX: %s", format_hex_pretty(this->buffers_.in.data, this->buffers_.in.size).c_str());
      ret_val = this->buffers_.in.size;

      this->update_last_rx_time_();
      return ret_val;
    }

    yield();
    App.feed_wdt();
  }
  return 0;
}

size_t XT211Component::receive_frame_raw_() {
  auto frame_end_check_timeout = [](uint8_t *b, size_t s) {
    return false;  // never stop by content, only by timeout
  };
  return receive_frame_(frame_end_check_timeout);
}

void XT211Component::clear_rx_buffers_() {
  int available = this->available();
  if (available > 0) {
    ESP_LOGVV(TAG, "Cleaning garbage from UART input buffer: %d bytes", available);
  }

  int len;
  while (available > 0) {
    len = std::min(available, (int) buffers_.in.capacity);
    this->read_array(this->buffers_.in.data, len);
    available -= len;
  }
  memset(this->buffers_.in.data, 0, buffers_.in.capacity);
  this->buffers_.in.size = 0;
  this->buffers_.in.position = 0;
}

const char *XT211Component::state_to_string(State state) {
  switch (state) {
    case State::NOT_INITIALIZED:
      return "NOT_INITIALIZED";
    case State::IDLE:
      return "IDLE";
    case State::WAIT:
      return "WAIT";
    case State::COMMS_RX:
      return "COMMS_RX";
    case State::MISSION_FAILED:
      return "MISSION_FAILED";
    case State::PUBLISH:
      return "PUBLISH";
    case State::PUSH_DATA_PROCESS:
      return "PUSH_DATA_PROCESS";
    default:
      return "UNKNOWN";
  }
}

void XT211Component::log_state_(State *next_state) {
  if (this->state_ != this->last_reported_state_) {
    if (next_state == nullptr) {
      ESP_LOGV(TAG, "State::%s", state_to_string(this->state_));
    } else {
      ESP_LOGV(TAG, "State::%s -> %s", state_to_string(this->state_), state_to_string(*next_state));
    }
    this->last_reported_state_ = this->state_;
  }
}

void XT211Component::stats_dump() {
  ESP_LOGV(TAG, "============================================");
  ESP_LOGV(TAG, "Data collection and publishing finished.");
  ESP_LOGV(TAG, "Total number of sessions ............. %u", this->stats_.connections_tried_);
  ESP_LOGV(TAG, "Total number of invalid frames ....... %u", this->stats_.invalid_frames_);
  ESP_LOGV(TAG, "Total number of CRC errors ........... %u", this->stats_.crc_errors_);
  ESP_LOGV(TAG, "Total number of CRC errors recovered . %u", this->stats_.crc_errors_recovered_);
  ESP_LOGV(TAG, "CRC errors per session ............... %f", this->stats_.crc_errors_per_session());
  ESP_LOGV(TAG, "Number of failures ................... %u", this->stats_.failures_);
  ESP_LOGV(TAG, "============================================");
}

bool XT211Component::try_lock_uart_session_() {
  if (AnyObjectLocker::try_lock(this->parent_)) {
    ESP_LOGV(TAG, "UART bus %p locked by %s", this->parent_, this->tag_.c_str());
    return true;
  }
  ESP_LOGV(TAG, "UART bus %p busy", this->parent_);
  return false;
}

void XT211Component::set_baud_rate_(uint32_t baud_rate) {
  if (this->iuart_ != nullptr) {
    this->iuart_->update_baudrate(baud_rate);
  }
}

uint8_t XT211Component::next_obj_id_ = 0;

std::string XT211Component::generateTag() { return str_sprintf("%s%03d", TAG0, ++next_obj_id_); }

}  // namespace xt211
}  // namespace esphome