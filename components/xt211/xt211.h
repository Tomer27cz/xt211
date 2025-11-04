#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>

#include "xt211_sensor.h"
#include "xt211_uart.h"
#include "object_locker.h"

#include "client.h"
#include "converters.h"
#include "cosem.h"
#include "dlmssettings.h"

namespace esphome {
  namespace xt211 {
    static const size_t DEFAULT_IN_BUF_SIZE_PUSH = 2048;

    using SensorMap = std::multimap<std::string, XT211SensorBase *>;

    using FrameStopFunction = std::function<
    bool(uint8_t
    *buf,
    size_t size
    )>;

    class AxdrStreamParser;

    class XT211Component : public PollingComponent, public uart::UARTDevice {
    public:
      XT211Component() : tag_(generateTag()) {};

      void setup() override;

      void dump_config() override;

      void loop() override;

      void update() override; // Kept for PollingComponent, but will be empty
      float get_setup_priority() const override { return setup_priority::DATA; };

      void set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; };

      void set_receive_timeout_ms(uint32_t timeout) { this->receive_timeout_ms_ = timeout; };

      void register_sensor(XT211SensorBase *sensor);

      void set_reboot_after_failure(uint16_t number_of_failures) { this->failures_before_reboot_ = number_of_failures; }

      // PUSH mode is now default, these are for configuring it
      void set_push_show_log(bool show_log) { this->push_show_log_ = show_log; }

      void set_push_custom_pattern_dsl(const std::string &dsl) { this->push_custom_pattern_dsl_ = dsl; }

      bool has_error{true};

#ifdef USE_BINARY_SENSOR
      SUB_BINARY_SENSOR(transmission)
SUB_BINARY_SENSOR(session)
SUB_BINARY_SENSOR(connection)
#endif

#ifdef USE_TEXT_SENSOR
      SUB_TEXT_SENSOR(last_scan)
#endif

    protected:
      bool push_show_log_{false};
      std::string push_custom_pattern_dsl_{""};

      uint32_t receive_timeout_ms_{2000};

      std::unique_ptr <XT211Uart> iuart_;

      SensorMap sensors_;

      sensor::Sensor *crc_errors_per_session_sensor_{};

      enum class State : uint8_t {
        NOT_INITIALIZED,
        IDLE,
        WAIT,
        COMMS_RX,
        MISSION_FAILED,
        PUSH_DATA_PROCESS, // Process received push data
        PUBLISH,
      } state_{State::NOT_INITIALIZED};
      State last_reported_state_{State::NOT_INITIALIZED};

      struct {
        uint32_t start_time{0};
        uint32_t delay_ms{0};
        State next_state{State::IDLE};
      } wait_;

      bool is_idling() const { return this->state_ == State::WAIT || this->state_ == State::IDLE; };

      void set_next_state_(State next_state) { state_ = next_state; };

      void set_next_state_delayed_(uint32_t ms, State next_state);

      void process_push_data();

      // State handler methods extracted from loop()
      void handle_comms_rx_();

      void handle_push_data_process_();

      void handle_publish_();

      int set_sensor_value(uint16_t class_id, const uint8_t *obis_code, DLMS_DATA_TYPE value_type,
                           const uint8_t *value_buffer_ptr, uint8_t value_length, const int8_t *scaler,
                           const uint8_t *unit);

      void indicate_transmission(bool transmission_on);

      void indicate_session(bool session_on);

      void indicate_connection(bool connection_on);

      AxdrStreamParser *axdr_parser_{nullptr};

      size_t received_frame_size_{0};

      uint32_t baud_rate_{9600};

      uint32_t last_rx_time_{0};

      struct LoopState {
        uint32_t session_started_ms{0};           // start of session
        SensorMap::iterator sensor_iter{nullptr}; // publishing sensor values

      } loop_state_;

      struct PushBuffers {
        gxByteBuffer in;

        void init(size_t default_in_buf_size);

        void reset();

        void check_and_grow_input(uint16_t more_data);

      } buffers_;

    protected:
      void clear_rx_buffers_();

      void set_baud_rate_(uint32_t baud_rate);

      size_t receive_frame_(FrameStopFunction stop_fn);

      size_t receive_frame_raw_();

      inline void update_last_rx_time_() { this->last_rx_time_ = millis(); }

      bool check_wait_timeout_() { return millis() - wait_.start_time >= wait_.delay_ms; }

      bool check_rx_timeout_() { return millis() - this->last_rx_time_ >= receive_timeout_ms_; }

      void report_failure(bool failure);

      void abort_mission_();

      const char *state_to_string(State state);

      void log_state_(State *next_state = nullptr);

      struct Stats {
        uint32_t connections_tried_{0};
        uint32_t crc_errors_{0};
        uint32_t crc_errors_recovered_{0};
        uint32_t invalid_frames_{0};
        uint8_t failures_{0};

        float crc_errors_per_session() const { return (float) crc_errors_ / connections_tried_; }
      } stats_;

      void stats_dump();

      uint8_t failures_before_reboot_{0};

      bool try_lock_uart_session_();
      // unlock_uart_session_() has been removed

    public:
    private:
      static uint8_t next_obj_id_;
      std::string tag_;

      static std::string generateTag();
    };

  } // namespace xt211
} // namespace esphome