import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
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

XT211TextSensor = xt211_ns.class_(
    "XT211TextSensor", text_sensor.TextSensor
)


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(
        XT211TextSensor,
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
    var = await text_sensor.new_text_sensor(config)
    cg.add(var.set_obis_code(config[CONF_OBIS_CODE]))
    cg.add(var.set_dont_publish(config.get(CONF_DONT_PUBLISH)))
    cg.add(var.set_obis_class(config[CONF_CLASS]))

    cg.add(component.register_sensor(var))