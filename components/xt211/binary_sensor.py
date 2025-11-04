# import esphome.codegen as cg
# import esphome.config_validation as cv
# from esphome.components import binary_sensor
# from esphome.const import (
#     DEVICE_CLASS_CONNECTIVITY,
#     ENTITY_CATEGORY_DIAGNOSTIC,
# )
# from . import (
#     XT211,
#     xt211_ns,
#     CONF_XT211_ID,
# )
#
# AUTO_LOAD = ["xt211"]
#
# CONF_TRANSMISSION = "transmission"
# CONF_SESSION = "session"
# CONF_CONNECTION = "connection"
#
# ICON_TRANSMISSION = "mdi:swap-horizontal"
# ICON_CONNECTION = "mdi:lan-connect"
# ICON_SESSION = "mdi:sync"
#
# CONFIG_SCHEMA = cv.Schema(
#     {
#         cv.GenerateID(CONF_XT211_ID): cv.use_id(XT211),
#         cv.Optional(CONF_TRANSMISSION):  binary_sensor.binary_sensor_schema(
#             device_class=DEVICE_CLASS_CONNECTIVITY,
#             entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
#             icon=ICON_TRANSMISSION,
#         ),
#         cv.Optional(CONF_SESSION):  binary_sensor.binary_sensor_schema(
#             device_class=DEVICE_CLASS_CONNECTIVITY,
#             entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
#             icon=ICON_SESSION,
#         ),
#         cv.Optional(CONF_CONNECTION):  binary_sensor.binary_sensor_schema(
#             device_class=DEVICE_CLASS_CONNECTIVITY,
#             entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
#             icon=ICON_CONNECTION,
#         ),
#     }
# )
#
# async def to_code(config):
#     hub = await cg.get_variable(config[CONF_XT211_ID])
#
#     if conf := config.get(CONF_TRANSMISSION):
#         sensor = await binary_sensor.new_binary_sensor(config[CONF_TRANSMISSION])
#         cg.add(hub.set_transmission_binary_sensor(sensor))
#
#     if conf := config.get(CONF_SESSION):
#         sensor = await binary_sensor.new_binary_sensor(config[CONF_SESSION])
#         cg.add(hub.set_session_binary_sensor(sensor))
#
#     if conf := config.get(CONF_CONNECTION):
#         sensor = await binary_sensor.new_binary_sensor(config[CONF_CONNECTION])
#         cg.add(hub.set_connection_binary_sensor(sensor))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import (
    XT211,
    xt211_ns,
    obis_code,
    CONF_XT211_ID,
    CONF_OBIS_CODE,
    CONF_DONT_PUBLISH,
    CONF_CLASS,
)

AUTO_LOAD = ["xt211"]

XT211BinarySensor = xt211_ns.class_(
    "XT211BinarySensor", binary_sensor.BinarySensor
)


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(
        XT211BinarySensor,
    ).extend(
        {
            cv.GenerateID(CONF_XT211_ID): cv.use_id(XT211),
            cv.Required(CONF_OBIS_CODE): obis_code,
            cv.Optional(CONF_DONT_PUBLISH, default=False): cv.boolean,
            cv.Optional(CONF_CLASS, default=1): cv.int_,
        }
    ),
    cv.has_exactly_one_key(CONF_OBIS_CODE),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_XT211_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_obis_code(config[CONF_OBIS_CODE]))
    cg.add(var.set_dont_publish(config.get(CONF_DONT_PUBLISH)))
    cg.add(var.set_obis_class(config[CONF_CLASS]))

    cg.add(component.register_sensor(var))