import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import DlmsMeterLibComponent, obis_code, CONF_DLMS_METER_ID, CONF_OBIS_CODE

DEPENDENCIES = ["dlms_meter_lib"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterLibComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(hub.register_text_sensor(config[CONF_OBIS_CODE], var))