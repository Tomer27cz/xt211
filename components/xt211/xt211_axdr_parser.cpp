#include "xt211_axdr_parser.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace esphome {
namespace xt211 {

constexpr const char *TAG = "xt211.axdr";

const char *dlms_error_to_string(int error) {
  switch (error) {
    case DLMS_ERROR_CODE_OK:
      return "DLMS_ERROR_CODE_OK";
    case DLMS_ERROR_CODE_HARDWARE_FAULT:
      return "DLMS_ERROR_CODE_HARDWARE_FAULT";
    case DLMS_ERROR_CODE_TEMPORARY_FAILURE:
      return "DLMS_ERROR_CODE_TEMPORARY_FAILURE";
    case DLMS_ERROR_CODE_READ_WRITE_DENIED:
      return "DLMS_ERROR_CODE_READ_WRITE_DENIED";
    case DLMS_ERROR_CODE_UNDEFINED_OBJECT:
      return "DLMS_ERROR_CODE_UNDEFINED_OBJECT";
    case DLMS_ERROR_CODE_ACCESS_VIOLATED:
      return "DLMS_ERROR_CODE_ACCESS_VIOLATED";
    default:
      return "";
  }
}

const char *dlms_data_type_to_string(DLMS_DATA_TYPE vt) {
  switch (vt) {
    case DLMS_DATA_TYPE_NONE:
      return "DLMS_DATA_TYPE_NONE";
    case DLMS_DATA_TYPE_BOOLEAN:
      return "DLMS_DATA_TYPE_BOOLEAN";
    case DLMS_DATA_TYPE_BIT_STRING:
      return "DLMS_DATA_TYPE_BIT_STRING";
    case DLMS_DATA_TYPE_INT32:
      return "DLMS_DATA_TYPE_INT32";
    case DLMS_DATA_TYPE_UINT32:
      return "DLMS_DATA_TYPE_UINT32";
    case DLMS_DATA_TYPE_OCTET_STRING:
      return "DLMS_DATA_TYPE_OCTET_STRING";
    case DLMS_DATA_TYPE_STRING:
      return "DLMS_DATA_TYPE_STRING";
    case DLMS_DATA_TYPE_BINARY_CODED_DESIMAL:
      return "DLMS_DATA_TYPE_BINARY_CODED_DESIMAL";
    case DLMS_DATA_TYPE_STRING_UTF8:
      return "DLMS_DATA_TYPE_STRING_UTF8";
    case DLMS_DATA_TYPE_INT8:
      return "DLMS_DATA_TYPE_INT8";
    case DLMS_DATA_TYPE_INT16:
      return "DLMS_DATA_TYPE_INT16";
    case DLMS_DATA_TYPE_UINT8:
      return "DLMS_DATA_TYPE_UINT8";
    case DLMS_DATA_TYPE_UINT16:
      return "DLMS_DATA_TYPE_UINT16";
    case DLMS_DATA_TYPE_INT64:
      return "DLMS_DATA_TYPE_INT64";
    case DLMS_DATA_TYPE_UINT64:
      return "DLMS_DATA_TYPE_UINT64";
    case DLMS_DATA_TYPE_ENUM:
      return "DLMS_DATA_TYPE_ENUM";
    case DLMS_DATA_TYPE_FLOAT32:
      return "DLMS_DATA_TYPE_FLOAT32";
    case DLMS_DATA_TYPE_FLOAT64:
      return "DLMS_DATA_TYPE_FLOAT64";
    case DLMS_DATA_TYPE_DATETIME:
      return "DLMS_DATA_TYPE_DATETIME";
    case DLMS_DATA_TYPE_DATE:
      return "DLMS_DATA_TYPE_DATE";
    case DLMS_DATA_TYPE_TIME:
      return "DLMS_DATA_TYPE_TIME";
    case DLMS_DATA_TYPE_ARRAY:
      return "DLMS_DATA_TYPE_ARRAY";
    case DLMS_DATA_TYPE_STRUCTURE:
      return "DLMS_DATA_TYPE_STRUCTURE";
    case DLMS_DATA_TYPE_COMPACT_ARRAY:
      return "DLMS_DATA_TYPE_COMPACT_ARRAY";
    case DLMS_DATA_TYPE_BYREF:
      return "DLMS_DATA_TYPE_BYREF";
    default:
      return "DMS_DATA_TYPE UNKNOWN";
  }
}

bool hlp_isValueDataType(DLMS_DATA_TYPE type) {
  switch (type) {
    // Complex/Container types - NOT value types
    case DLMS_DATA_TYPE_ARRAY:
    case DLMS_DATA_TYPE_STRUCTURE:
    case DLMS_DATA_TYPE_COMPACT_ARRAY:
      return false;

    // All other types are value types
    case DLMS_DATA_TYPE_NONE:
    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_BIT_STRING:
    case DLMS_DATA_TYPE_INT32:
    case DLMS_DATA_TYPE_UINT32:
    case DLMS_DATA_TYPE_OCTET_STRING:
    case DLMS_DATA_TYPE_STRING:
    case DLMS_DATA_TYPE_BINARY_CODED_DESIMAL:
    case DLMS_DATA_TYPE_STRING_UTF8:
    case DLMS_DATA_TYPE_INT8:
    case DLMS_DATA_TYPE_INT16:
    case DLMS_DATA_TYPE_UINT8:
    case DLMS_DATA_TYPE_UINT16:
    case DLMS_DATA_TYPE_INT64:
    case DLMS_DATA_TYPE_UINT64:
    case DLMS_DATA_TYPE_ENUM:
    case DLMS_DATA_TYPE_FLOAT32:
    case DLMS_DATA_TYPE_FLOAT64:
    case DLMS_DATA_TYPE_DATETIME:
    case DLMS_DATA_TYPE_DATE:
    case DLMS_DATA_TYPE_TIME:
      return true;

    default:

      return false;
  }
}
float dlms_data_as_float(DLMS_DATA_TYPE value_type, const uint8_t *value_buffer_ptr, uint8_t value_length) {
  if (value_buffer_ptr == nullptr || value_length == 0)
    return 0.0f;

  auto be16 = [](const uint8_t *p) -> uint16_t { return (uint16_t) ((p[0] << 8) | p[1]); };
  auto be32 = [](const uint8_t *p) -> uint32_t {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
  };
  auto be64 = [](const uint8_t *p) -> uint64_t {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
      v = (v << 8) | p[i];
    return v;
  };

  switch (value_type) {
    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_ENUM:
    case DLMS_DATA_TYPE_UINT8:
      return static_cast<float>(value_buffer_ptr[0]);
    case DLMS_DATA_TYPE_INT8:
      return static_cast<float>(static_cast<int8_t>(value_buffer_ptr[0]));
    case DLMS_DATA_TYPE_UINT16:
      if (value_length >= 2)
        return static_cast<float>(be16(value_buffer_ptr));
      return 0.0f;
    case DLMS_DATA_TYPE_INT16:
      if (value_length >= 2) {
        int16_t v = static_cast<int16_t>(be16(value_buffer_ptr));
        return static_cast<float>(v);
      }
      return 0.0f;
    case DLMS_DATA_TYPE_UINT32:
      if (value_length >= 4)
        return static_cast<float>(be32(value_buffer_ptr));
      return 0.0f;
    case DLMS_DATA_TYPE_INT32:
      if (value_length >= 4) {
        int32_t v = static_cast<int32_t>(be32(value_buffer_ptr));
        return static_cast<float>(v);
      }
      return 0.0f;
    case DLMS_DATA_TYPE_UINT64:
      if (value_length >= 8)
        return static_cast<float>(be64(value_buffer_ptr));
      return 0.0f;
    case DLMS_DATA_TYPE_INT64:
      if (value_length >= 8) {
        uint64_t u = be64(value_buffer_ptr);
        int64_t v = static_cast<int64_t>(u);
        return static_cast<float>(v);
      }
      return 0.0f;
    case DLMS_DATA_TYPE_FLOAT32:
      if (value_length >= 4) {
        uint32_t u = be32(value_buffer_ptr);
        float f{};
        std::memcpy(&f, &u, sizeof(f));
        return f;
      }
      return 0.0f;
    case DLMS_DATA_TYPE_FLOAT64:
      if (value_length >= 8) {
        uint8_t b[8];
        for (int i = 0; i < 8; i++)
          b[i] = value_buffer_ptr[i];

        double d{};
        std::memcpy(&d, b, sizeof(d));
        return static_cast<float>(d);
      }
      return 0.0f;
    default:
      return 0.0f;
  }
}
std::string dlms_data_as_string(DLMS_DATA_TYPE value_type, const uint8_t *value_buffer_ptr, uint8_t value_length) {
  if (value_buffer_ptr == nullptr && value_length == 0)
    return std::string();

  auto hex_of = [](const uint8_t *p, uint8_t len) -> std::string {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t i = 0; i < len; i++) {
      ss << std::setw(2) << static_cast<int>(p[i]);
      if (i + 1 < len)
        ss << "";  // compact
    }
    return ss.str();
  };

  switch (value_type) {
    case DLMS_DATA_TYPE_OCTET_STRING:
    case DLMS_DATA_TYPE_STRING:
    case DLMS_DATA_TYPE_STRING_UTF8: {
      return std::string(reinterpret_cast<const char *>(value_buffer_ptr),
                         reinterpret_cast<const char *>(value_buffer_ptr) + value_length);
    }
    case DLMS_DATA_TYPE_BIT_STRING:
    case DLMS_DATA_TYPE_BINARY_CODED_DESIMAL:
      return hex_of(value_buffer_ptr, value_length);

    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_ENUM:
    case DLMS_DATA_TYPE_UINT8: {
      return std::to_string(static_cast<unsigned>(value_buffer_ptr ? value_buffer_ptr[0] : 0));
    }
    case DLMS_DATA_TYPE_INT8: {
      return std::to_string(static_cast<int>(static_cast<int8_t>(value_buffer_ptr ? value_buffer_ptr[0] : 0)));
    }
    case DLMS_DATA_TYPE_UINT16: {
      if (value_length >= 2) {
        uint16_t v = (uint16_t) ((value_buffer_ptr[0] << 8) | value_buffer_ptr[1]);
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_INT16: {
      if (value_length >= 2) {
        int16_t v = (int16_t) ((value_buffer_ptr[0] << 8) | value_buffer_ptr[1]);
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_UINT32: {
      if (value_length >= 4) {
        uint32_t v = ((uint32_t) value_buffer_ptr[0] << 24) | ((uint32_t) value_buffer_ptr[1] << 16) |
                     ((uint32_t) value_buffer_ptr[2] << 8) | (uint32_t) value_buffer_ptr[3];
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_INT32: {
      if (value_length >= 4) {
        int32_t v = ((int32_t) value_buffer_ptr[0] << 24) | ((int32_t) value_buffer_ptr[1] << 16) |
                    ((int32_t) value_buffer_ptr[2] << 8) | (int32_t) value_buffer_ptr[3];
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_UINT64: {
      if (value_length >= 8) {
        uint64_t v = 0;
        for (int i = 0; i < 8; i++)
          v = (v << 8) | value_buffer_ptr[i];
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_INT64: {
      if (value_length >= 8) {
        uint64_t u = 0;
        for (int i = 0; i < 8; i++)
          u = (u << 8) | value_buffer_ptr[i];
        int64_t v = static_cast<int64_t>(u);
        return std::to_string(v);
      }
      return std::string();
    }
    case DLMS_DATA_TYPE_FLOAT32:
    case DLMS_DATA_TYPE_FLOAT64: {
      float f = dlms_data_as_float(value_type, value_buffer_ptr, value_length);
      // Use minimal formatting
      std::ostringstream ss;
      ss << f;
      return ss.str();
    }
    case DLMS_DATA_TYPE_DATETIME:
    case DLMS_DATA_TYPE_DATE:
    case DLMS_DATA_TYPE_TIME:
      // For now, return hex. Higher-level layers may decode properly.
      return hex_of(value_buffer_ptr, value_length);

    case DLMS_DATA_TYPE_NONE:
    default:
      return std::string();
  }
}

void AxdrPatternRegistry::add_pattern(const AxdrDescriptorPattern &p) {
  auto it = std::upper_bound(
      patterns_.begin(), patterns_.end(), p,
      [](const AxdrDescriptorPattern &a, const AxdrDescriptorPattern &b) { return a.priority < b.priority; });
  patterns_.insert(it, p);
}

uint8_t AxdrStreamParser::peek_byte_() {
  if (this->buffer_->position + 1 > this->buffer_->size) {
    return 0xFF;
  }
  return this->buffer_->data[this->buffer_->position];
}

uint8_t AxdrStreamParser::read_byte_() {
  if (this->buffer_->position + 1 > this->buffer_->size) {
    return 0xFF;
  }
  return this->buffer_->data[this->buffer_->position++];
}

uint16_t AxdrStreamParser::read_u16_() {
  if (this->buffer_->position + 2 > this->buffer_->size) {
    return 0xFFFF;
  }
  uint16_t value =
      (this->buffer_->data[this->buffer_->position] << 8) | this->buffer_->data[this->buffer_->position + 1];
  this->buffer_->position += 2;
  return value;
}

uint32_t AxdrStreamParser::read_u32_() {
  if (this->buffer_->position + 4 > this->buffer_->size) {
    return 0xFFFFFFFF;
  }
  uint32_t value =
      (this->buffer_->data[this->buffer_->position] << 24) | (this->buffer_->data[this->buffer_->position + 1] << 16) |
      (this->buffer_->data[this->buffer_->position + 2] << 8) | this->buffer_->data[this->buffer_->position + 3];
  this->buffer_->position += 4;
  return value;
}

bool AxdrStreamParser::test_if_date_time_12b_() {
  if (this->buffer_->position + 12 > this->buffer_->size) {
    return 0;
  }

  const uint8_t *buf = this->buffer_->data + this->buffer_->position;
  if (!buf)
    return false;

  // Year
  uint16_t year = (buf[0] << 8) | buf[1];
  if (!(year == 0x0000 || (year >= 1970 && year <= 2100)))
    return false;

  // Month
  uint8_t month = buf[2];
  if (!(month == 0xFF || (month >= 1 && month <= 12)))
    return false;

  // Day of month
  uint8_t day = buf[3];
  if (!(day == 0xFF || (day >= 1 && day <= 31)))
    return false;

  // Day of week
  uint8_t dow = buf[4];
  if (!(dow == 0xFF || (dow >= 1 && dow <= 7)))
    return false;

  // Hour
  uint8_t hour = buf[5];
  if (!(hour == 0xFF || hour <= 23))
    return false;

  // Minute
  uint8_t minute = buf[6];
  if (!(minute == 0xFF || minute <= 59))
    return false;

  // Second
  uint8_t second = buf[7];
  if (!(second == 0xFF || second <= 59))
    return false;

  // Hundredths of second
  uint8_t ms = buf[8];
  if (!(ms == 0xFF || ms <= 99))
    return false;

  // some makers mix up the order
  // Deviation (timezone offset, signed, 2 bytes)
  uint16_t u_dev = (buf[9] << 8) | buf[10];
  int16_t s_dev = (int16_t) (u_dev);
  if (!((s_dev == (int16_t) 0x8000 || (s_dev >= -720 && s_dev <= 720))))
    return false;

  uint8_t clock_status = buf[11];

  return true;
}

constexpr uint16_t MAX_CLASS_ID = 0x00FF;
constexpr uint8_t MAX_ATTRIBUTE_ID = 0x20;
constexpr size_t MIN_UNTAGGED_ATTRIBUTE_DESCRIPTOR_SIZE = 9;
constexpr size_t MIN_TAGGED_ATTRIBUTE_DESCRIPTOR_SIZE = 14;

bool AxdrStreamParser::skip_data_(uint8_t type) {
  int data_size = hlp_getDataTypeSize((DLMS_DATA_TYPE) type);

  if (data_size == 0)  // Zero-length data (NONE type) - nothing to skip
    return true;

  if (data_size > 0) {  // Fixed size data
    if (this->buffer_->position + data_size > this->buffer_->size) {
      return false;
    }
    this->buffer_->position += data_size;
  } else {  // Variable size data
    uint8_t length = read_byte_();
    if (length == 0xFF) {
      return false;
    }
    if (this->buffer_->position + length > this->buffer_->size) {
      return false;
    }
    this->buffer_->position += length;
  }

  return true;
}

bool AxdrStreamParser::skip_sequence_(uint8_t type) {
  uint8_t elements_count = read_byte_();
  if (elements_count == 0xFF) {
    ESP_LOGV(TAG, "Invalid sequence length when skipping at position %d", this->buffer_->position - 1);
    return false;
  }

  ESP_LOGD(TAG, "Skipping %s with %d elements at position %d",
           (type == DLMS_DATA_TYPE_STRUCTURE) ? "STRUCTURE" : "ARRAY", elements_count, this->buffer_->position - 1);

  for (uint8_t i = 0; i < elements_count; i++) {
    uint8_t elem_type = read_byte_();
    if (!parse_element_(elem_type)) {
      ESP_LOGV(TAG, "Failed to skip element %d of %s at position %d", i + 1,
               (type == DLMS_DATA_TYPE_STRUCTURE) ? "STRUCTURE" : "ARRAY", this->buffer_->position - 1);
      return false;
    }
  }

  return true;
}

bool AxdrStreamParser::parse_data_(uint8_t type, uint8_t depth) { return skip_data_(type); }

bool AxdrStreamParser::parse_sequence_(uint8_t type, uint8_t depth) {
  uint8_t elements_count = read_byte_();
  if (elements_count == 0xFF) {
    ESP_LOGVV(TAG, "Invalid sequence length at position %d", this->buffer_->position - 1);
    return false;
  }

  uint8_t elements_consumed = 0;

  while (elements_consumed < elements_count) {
    uint32_t original_position = this->buffer_->position;

    if (try_match_patterns_(elements_consumed)) {
      uint8_t used = this->last_pattern_elements_consumed_ ? this->last_pattern_elements_consumed_ : 1;
      elements_consumed += used;
      this->last_pattern_elements_consumed_ = 0;
      continue;
    }

    if (this->buffer_->position >= this->buffer_->size) {
      ESP_LOGV(TAG, "Unexpected end while reading element %d of %s", elements_consumed + 1,
               (type == DLMS_DATA_TYPE_STRUCTURE) ? "STRUCTURE" : "ARRAY");
      return false;
    }
    uint8_t elem_type = read_byte_();
    if (!parse_element_(elem_type, depth + 1)) {
      return false;
    }
    elements_consumed++;

    if (this->buffer_->position == original_position) {
      ESP_LOGV(TAG, "No progress parsing element %d at position %d, aborting to avoid infinite loop", elements_consumed,
               original_position);
      return false;
    }
  }

  return true;
}

bool AxdrStreamParser::parse_element_(uint8_t type, uint8_t depth) {
  if (type == DLMS_DATA_TYPE_STRUCTURE || type == DLMS_DATA_TYPE_ARRAY) {
    return parse_sequence_(type, depth);
  } else {
    return parse_data_(type, depth);
  }
}

size_t AxdrStreamParser::parse() {
  if (this->buffer_ == nullptr || this->buffer_->size == 0) {
    ESP_LOGV(TAG, "Buffer is null or empty");
    return 0;
  }
  // Skip to notification flag 0x0F
  while (this->buffer_->position < this->buffer_->size) {
    uint8_t flag = read_byte_();
    if (flag == 0x0F) {
      ESP_LOGD(TAG, "Found notification flag at position %d", this->buffer_->position - 1);
      break;
    }
  }

  // Skip 5 bytes (invoke id and priority)
  for (int i = 0; i < 5; i++) {
    uint8_t priority = read_byte_();
  }

  // Check for datetime object before the data
  bool is_date_time = test_if_date_time_12b_();
  if (is_date_time) {
    ESP_LOGV(TAG, "Skipping datetime at position %d", this->buffer_->position);
    this->buffer_->position += 12;
  }

  // First byte after flag should be the data type
  uint8_t start_type = read_byte_();
  if (start_type != (uint8_t) DLMS_DATA_TYPE_STRUCTURE && start_type != (uint8_t) DLMS_DATA_TYPE_ARRAY) {
    ESP_LOGV(TAG, "Expected structure or array after notification flag, found type %02X at position %d", start_type,
             this->buffer_->position);
    return 0;
  }

  // Parse the data, looking for attribute descriptors
  bool success = parse_element_(start_type, 0);
  if (!success) {
    ESP_LOGV(TAG, "Some errors occurred parsing AXDR data");
  }

  ESP_LOGD(TAG, "Fast parsing completed, processed %d bytes", this->buffer_->position);
  return this->objects_found_;
}

bool AxdrStreamParser::capture_generic_value_(AxdrCaptures &c) {
  uint8_t vt = read_byte_();
  if (!hlp_isValueDataType((DLMS_DATA_TYPE) vt)) {
    return false;
  }
  int ds = hlp_getDataTypeSize((DLMS_DATA_TYPE) vt);
  const uint8_t *ptr = nullptr;
  uint8_t len = 0;
  if (ds > 0) {
    if (this->buffer_->position + ds > this->buffer_->size)
      return false;
    ptr = &this->buffer_->data[this->buffer_->position];
    len = (uint8_t) ds;
    this->buffer_->position += ds;
  } else if (ds == 0) {
    ptr = nullptr;
    len = 0;
  } else {
    uint8_t L = read_byte_();
    if (L == 0xFF || this->buffer_->position + L > this->buffer_->size)
      return false;
    ptr = &this->buffer_->data[this->buffer_->position];
    len = L;
    this->buffer_->position += L;
  }
  c.value_type = (DLMS_DATA_TYPE) vt;
  c.value_ptr = ptr;
  c.value_len = len;
  return true;
}

void AxdrStreamParser::emit_object_(const AxdrDescriptorPattern &pat, const AxdrCaptures &c) {
  if (!c.obis)
    return;
  uint16_t cid = c.class_id ? c.class_id : pat.default_class_id;

  if (this->show_log_) {
    ESP_LOGD(TAG, "Pattern match '%s' at idx %d ===============", pat.name, c.elem_idx);

    auto val_f = dlms_data_as_float((DLMS_DATA_TYPE) c.value_type, c.value_ptr, c.value_len);
    auto val_s = dlms_data_as_string((DLMS_DATA_TYPE) c.value_type, c.value_ptr, c.value_len);
    ESP_LOGI(TAG, "Found attribute descriptor: class_id=%d, obis=%d.%d.%d.%d.%d.%d", cid, c.obis[0], c.obis[1],
             c.obis[2], c.obis[3], c.obis[4], c.obis[5], c.value_type);
    if (c.has_scaler_unit) {
      ESP_LOGI(TAG, "Value type: %s, len %d, scaler %d, unit %d (%s)",
               dlms_data_type_to_string((DLMS_DATA_TYPE) c.value_type), c.value_len, c.scaler, c.unit_enum,
               obj_getUnitAsString(c.unit_enum));
    } else {
      ESP_LOGI(TAG, "Value type: %s, len %d", dlms_data_type_to_string((DLMS_DATA_TYPE) c.value_type), c.value_len);
    }
    ESP_LOGI(TAG, " as hex dump : %s", esphome::format_hex_pretty(c.value_ptr, c.value_len).c_str());
    ESP_LOGI(TAG, " as string   :'%s'", val_s.c_str());
    ESP_LOGI(TAG, " as number   : %f", val_f);
    if (c.has_scaler_unit) {
      ESP_LOGI(TAG, " as number * scaler  : %f", val_f * std::pow(10, c.scaler));
    }
  }

  if (callback_) {
    if (c.has_scaler_unit) {
      callback_(cid, c.obis, c.value_type, c.value_ptr, c.value_len, &c.scaler, &c.unit_enum);
    } else {
      callback_(cid, c.obis, c.value_type, c.value_ptr, c.value_len, nullptr, nullptr);
    }
  }
  this->objects_found_++;
}

bool AxdrStreamParser::match_pattern_(uint8_t elem_idx, const AxdrDescriptorPattern &pat,
                                      uint8_t &elements_consumed_at_level0) {
  AxdrCaptures cap{};
  elements_consumed_at_level0 = 0;
  uint8_t level = 0;
  auto consume_one = [&]() {
    if (level == 0)
      elements_consumed_at_level0++;
  };
  auto initial_position = this->buffer_->position;

  for (const auto &step : pat.steps) {
    switch (step.type) {
      case AxdrTokenType::EXPECT_TO_BE_FIRST: {
        if (elem_idx != 0)
          return false;
        break;
      }
      case AxdrTokenType::EXPECT_TYPE_EXACT: {
        uint8_t t = read_byte_();
        if (t != step.param_u8_a)
          return false;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_TYPE_U_I_8: {
        uint8_t t = read_byte_();
        if (!(t == DLMS_DATA_TYPE_INT8 || t == DLMS_DATA_TYPE_UINT8))
          return false;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED: {
        uint16_t v = read_u16_();
        if (
            // v == 0 ||
            v > MAX_CLASS_ID)
          return false;
        cap.class_id = v;
        break;
      }
      case AxdrTokenType::EXPECT_OBIS6_TAGGED: {
        uint8_t t = read_byte_();
        if (t != DLMS_DATA_TYPE_OCTET_STRING)
          return false;
        uint8_t len = read_byte_();
        if (len != 6)
          return false;
        if (this->buffer_->position + 6 > this->buffer_->size)
          return false;
        cap.obis = &this->buffer_->data[this->buffer_->position];
        this->buffer_->position += 6;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_OBIS6_UNTAGGED: {
        if (this->buffer_->position + 6 > this->buffer_->size)
          return false;
        cap.obis = &this->buffer_->data[this->buffer_->position];
        this->buffer_->position += 6;
        break;
      }
      case AxdrTokenType::EXPECT_ATTR8_UNTAGGED: {
        uint8_t a = read_byte_();
        if (a == 0)
          return false;
        //        cap.attr_id = a;
        break;
      }
      case AxdrTokenType::EXPECT_VALUE_GENERIC: {
        if (!capture_generic_value_(cap))
          return false;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_STRUCTURE_N: {
        uint8_t t = read_byte_();
        if (t != DLMS_DATA_TYPE_STRUCTURE)
          return false;
        uint8_t cnt = read_byte_();
        if (cnt != step.param_u8_a)
          return false;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_SCALER_TAGGED: {
        uint8_t t = read_byte_();
        if (t != DLMS_DATA_TYPE_INT8)
          return false;
        uint8_t b = read_byte_();
        cap.scaler = (int8_t) b;
        cap.has_scaler_unit = true;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED: {
        uint8_t t = read_byte_();
        if (t != DLMS_DATA_TYPE_ENUM)
          return false;
        uint8_t b = read_byte_();
        cap.unit_enum = b;
        cap.has_scaler_unit = true;
        consume_one();
        break;
      }
      case AxdrTokenType::GOING_DOWN: {
        level++;
        break;
      }
      case AxdrTokenType::GOING_UP: {
        level--;
        break;
      }
      default:
        return false;
    }
  }
  if (elements_consumed_at_level0 == 0) {
    elements_consumed_at_level0 = 1;  // Fallback: one element to move forward
  }
  cap.elem_idx = initial_position;
  emit_object_(pat, cap);
  return true;
}

bool AxdrStreamParser::try_match_patterns_(uint8_t elem_idx) {
  const auto &pats = registry_.patterns();
  for (const auto &p : pats) {
    uint8_t consumed = 0;
    uint32_t saved_position = buffer_->position;
    if (match_pattern_(elem_idx, p, consumed)) {
      this->last_pattern_elements_consumed_ = consumed;
      return true;
    } else {
      buffer_->position = saved_position;
    }
  }
  return false;
}

void AxdrStreamParser::register_pattern_dsl(const char *name, const std::string &dsl, int priority) {
  AxdrDescriptorPattern pat{name, priority, {}, 0};
  // DSL tokens separated by commas, optional spaces. Supported atoms:
  // F   : must be first element in sequence
  // C   : raw class_id (uint16 payload, no type tag)
  // TC  : tagged class_id (type tag UINT16 + uint16 payload)
  // O   : raw OBIS (6 bytes payload, no type tag)
  // TO  : tagged OBIS (type tag OCTET_STRING + length=6 + 6 bytes)
  // A   : raw attribute id (uint8 payload, no type tag)
  // TA  : tagged attribute (type tag INT8/UINT8 + 1 byte)
  // TV  : tagged value (type tag + payload for any value type)
  // TSU : tagged scaler+unit structure (STRUCTURE tag + count 2 + TS + TU)
  // Additionally supported:
  // TS  : tagged scaler (type tag INT8 + int8 payload)
  // TU  : tagged unit (type tag ENUM + uint8 payload)
  // S(...) : structure with N elements, expands inner tokens
  // Examples: "TC,TO,TA,TV", "TO,TV,TSU", "F,C,O,A,TV"

  // For simplicity we parse left-to-right and translate to steps.

  auto trim = [](const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos)
      return std::string();
    return s.substr(b, e - b + 1);
  };

  std::list<std::string> tokens;
  std::string current;
  int paren = 0;
  for (char c : dsl) {
    if (c == '(') {
      paren++;
      current.push_back(c);
    } else if (c == ')') {
      paren--;
      current.push_back(c);
    } else if (c == ',' && paren == 0) {
      tokens.push_back(trim(current));
      current.clear();
    } else
      current.push_back(c);
  }
  if (!current.empty())
    tokens.push_back(trim(current));

  for (auto it = tokens.begin(); it != tokens.end(); ++it) {
    auto &tok = *it;
    if (tok.empty())
      continue;
    if (tok == "F") {
      pat.steps.push_back({AxdrTokenType::EXPECT_TO_BE_FIRST});
    } else if (tok == "C") {
      pat.steps.push_back({AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED});
    } else if (tok == "TC") {
      pat.steps.push_back({AxdrTokenType::EXPECT_TYPE_EXACT, (uint8_t) DLMS_DATA_TYPE_UINT16});
      pat.steps.push_back({AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED});
    } else if (tok == "O") {
      pat.steps.push_back({AxdrTokenType::EXPECT_OBIS6_UNTAGGED});
    } else if (tok == "TO") {
      pat.steps.push_back({AxdrTokenType::EXPECT_OBIS6_TAGGED});
    } else if (tok == "A") {
      pat.steps.push_back({AxdrTokenType::EXPECT_ATTR8_UNTAGGED});
    } else if (tok == "TA") {
      pat.steps.push_back({AxdrTokenType::EXPECT_TYPE_U_I_8});
      pat.steps.push_back({AxdrTokenType::EXPECT_ATTR8_UNTAGGED});
    } else if (tok == "TS") {
      pat.steps.push_back({AxdrTokenType::EXPECT_SCALER_TAGGED});
    } else if (tok == "TU") {
      pat.steps.push_back({AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED});
    } else if (tok == "TSU") {
      pat.steps.push_back({AxdrTokenType::EXPECT_STRUCTURE_N, 2});
      pat.steps.push_back({AxdrTokenType::GOING_DOWN});
      pat.steps.push_back({AxdrTokenType::EXPECT_SCALER_TAGGED});
      pat.steps.push_back({AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED});
      pat.steps.push_back({AxdrTokenType::GOING_UP});
    } else if (tok == "V" || tok == "TV") {
      pat.steps.push_back({AxdrTokenType::EXPECT_VALUE_GENERIC});
    } else if (tok.rfind("S", 0) == 0) {
      size_t l = tok.find('(');
      size_t r = tok.rfind(')');
      std::list<std::string> inner_tokens;
      if (l != std::string::npos && r != std::string::npos && r > l + 1) {
        std::string inner = tok.substr(l + 1, r - l - 1);
        std::vector<std::string> innerT;
        std::string cur;
        for (char c2 : inner) {
          if (c2 == ',') {
            inner_tokens.push_back(trim(cur));
            cur.clear();
          } else
            cur.push_back(c2);
        }
        if (!cur.empty())
          inner_tokens.push_back(trim(cur));
      }
      if (!inner_tokens.empty()) {
        pat.steps.push_back({AxdrTokenType::EXPECT_STRUCTURE_N, static_cast<uint8_t>(inner_tokens.size())});
        inner_tokens.push_front("DN");
        inner_tokens.push_back("UP");
        tokens.insert(std::next(it), inner_tokens.begin(), inner_tokens.end());
      }

    } else if (tok == "DN") {
      pat.steps.push_back({AxdrTokenType::GOING_DOWN});
    } else if (tok == "UP") {
      pat.steps.push_back({AxdrTokenType::GOING_UP});
    }
  }

  registry_.add_pattern(pat);
}

}  // namespace xt211
}  // namespace esphome