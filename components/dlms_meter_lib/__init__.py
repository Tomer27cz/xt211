import re

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RECEIVE_TIMEOUT, PLATFORM_ESP32, PLATFORM_ESP8266

CODEOWNERS = ["@Tomer27cz"]
DEPENDENCIES = ["uart"]

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_OBIS_CODE = "obis_code"

CONF_DECRYPTION_KEY = "decryption_key"
CONF_CUSTOM_PATTERN = "custom_pattern"

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter_lib")
DlmsMeterLibComponent = dlms_meter_component_ns.class_(
    "DlmsMeterLibComponent", cg.Component, uart.UARTDevice
)


def validate_key(value):
    value = cv.string_strict(value)
    if len(value) != 32:
        raise cv.Invalid("Decryption key must be 32 hex characters (16 bytes)")
    try:
        bytes.fromhex(value)
    except ValueError as exc:
        raise cv.Invalid("Decryption key must be hex values from 00 to FF") from exc
    return value

def obis_code(value):
    value = cv.string(value)
    # Validate standard OBIS format: A.B.C.D.E.F (e.g., 1.0.1.8.0.255)
    match = re.match(r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(
            f"{value} is not a valid OBIS code (expected format: A.B.C.D.E.F)"
        )
    return value

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterLibComponent),
            cv.Optional(CONF_RECEIVE_TIMEOUT, default="50ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DECRYPTION_KEY): validate_key,
            cv.Optional(CONF_CUSTOM_PATTERN, default=""): cv.string,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP8266, PLATFORM_ESP32]),
    )

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "dlms_meter_lib", require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_DECRYPTION_KEY in config:
        cg.add(var.set_decryption_key(config[CONF_DECRYPTION_KEY]))

    cg.add(var.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT]))
    cg.add(var.set_custom_pattern(config[CONF_CUSTOM_PATTERN]))

    cg.add_library("dlms_parser", None, "https://github.com/esphome-libs/dlms_parser")