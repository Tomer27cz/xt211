import re
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_BAUD_RATE,
    CONF_RECEIVE_TIMEOUT,
)

DEPENDENCIES = ["uart"]

DEFAULTS_BAUD_RATE_SESSION = 9600
DEFAULTS_RECEIVE_TIMEOUT = "2000ms"

CONF_XT211_ID = "xt211_id"
CONF_OBIS_CODE = "obis_code"
CONF_DONT_PUBLISH = "dont_publish"
CONF_CLASS = "class"

CONF_PUSH_SHOW_LOG = "push_show_log"
CONF_PUSH_CUSTOM_PATTERN = "push_custom_pattern"


xt211_ns = cg.esphome_ns.namespace("xt211")
XT211 = xt211_ns.class_(
    "XT211Component", cg.Component, uart.UARTDevice
)

BAUD_RATES = [300, 600, 1200, 2400, 4800, 9600, 19200]

def obis_code(value):
    value = cv.string(value)
    #    match = re.match(r"^\d{1,3}-\d{1,3}:\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    match = re.match(r"^\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid OBIS code")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XT211),
            cv.Optional(CONF_BAUD_RATE, default=DEFAULTS_BAUD_RATE_SESSION): cv.one_of(
                *BAUD_RATES
            ),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default=DEFAULTS_RECEIVE_TIMEOUT
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PUSH_SHOW_LOG, default=False): cv.boolean,
            cv.Optional(CONF_PUSH_CUSTOM_PATTERN, default=""): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_receive_timeout_ms(config[CONF_RECEIVE_TIMEOUT]))

    cg.add(var.set_push_show_log(config[CONF_PUSH_SHOW_LOG]))
    cg.add(var.set_push_custom_pattern_dsl(config[CONF_PUSH_CUSTOM_PATTERN]))

    cg.add_build_flag("-Wno-error=implicit-function-declaration")
    # cg.add_library("GuruxDLMS", None, "https://github.com/latonita/GuruxDLMS.c#platformio")
    # cg.add_library("GuruxDLMS", None, "https://github.com/Gurux/GuruxDLMS.c#platformio")