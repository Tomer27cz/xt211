#pragma once

#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include <cmath> // for std::pow

namespace esphome {
    namespace xt211 {

        enum SensorType { SENSOR, TEXT_SENSOR, BINARY_SENSOR };

// const char * UNIT_STR_UNKNOWN = "Unknown unit";
#define UNIT_STR_UNKNOWN_NOT_YET "Unknown unit / not yet known"
#define UNIT_STR_UNKNOWN "Unknown unit"

        class XT211SensorBase {
        public:
            static const uint8_t MAX_REQUEST_SIZE = 15;

            virtual SensorType get_type() const = 0;
            virtual const StringRef &get_sensor_name() = 0;
            virtual EntityBase *get_base() = 0;
            virtual void publish() = 0;

            void set_obis_code(const char *obis_code) { this->obis_code_ = obis_code; }
            const std::string &get_obis_code() const { return this->obis_code_; }

            void set_dont_publish(bool dont_publish) { this->we_shall_publish_ = !dont_publish; }
            bool shall_we_publish() const { return this->we_shall_publish_; }

            void set_obis_class(int obis_class) { this->obis_class_ = obis_class; }
            int get_obis_class() { return this->obis_class_; }

            virtual bool has_got_scale_and_unit() { return false; } // PULL mode logic removed

        protected:
            std::string obis_code_;
            int obis_class_{3 /*DLMS_OBJECT_TYPE_REGISTER*/};
            bool we_shall_publish_{true};
            // Removed has_value_, tries_
        };

        class XT211Sensor : public XT211SensorBase, public sensor::Sensor {
        public:
            SensorType get_type() const override { return SENSOR; }
            const StringRef &get_sensor_name() { return this->get_name(); }
            EntityBase *get_base() { return this; }
            void publish() override { publish_state(this->value_); }

            bool has_got_scale_and_unit() override { return true; } // PULL mode logic removed

            void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }

            void set_value(float value) {
                // Scale is applied in XT211Component::set_sensor_value before calling this
                this->value_ = value * multiplier_;
            }

        protected:
            float value_{NAN};
            float multiplier_{1.0f};
            // Removed scaler_, scale_f_, unit_, unit_s_, scale_and_unit_detected_
        };

#ifdef USE_TEXT_SENSOR
        class XT211TextSensor : public XT211SensorBase, public text_sensor::TextSensor {
 public:
  SensorType get_type() const override { return TEXT_SENSOR; }
  const StringRef &get_sensor_name() { return this->get_name(); }
  EntityBase *get_base() { return this; }
  void publish() override { publish_state(value_); }

  bool has_got_scale_and_unit() override { return true; }

  void set_value(const char *value) {
    value_ = std::string(value);
  }

 protected:
  std::string value_;

};
#endif
#ifdef USE_BINARY_SENSOR
        class XT211BinarySensor : public XT211SensorBase, public binary_sensor::BinarySensor {
 public:
  SensorType get_type() const override { return BINARY_SENSOR; }
  const StringRef &get_sensor_name() { return this->get_name(); }
  EntityBase *get_base() { return this; }
  void publish() override { publish_state(value_); }

  bool has_got_scale_and_unit() override { return true; }

  void set_value(bool value) {
    value_ = value;
  }

 protected:
  bool value_;

};
#endif

    } // namespace xt211
} // namespace esphome