import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_DLMS_METER_ID, CONF_OBIS_CODE

from . import DlmsMeterLibComponent, obis_code

DEPENDENCIES = ["dlms_meter_lib"]

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterLibComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await sensor.new_sensor(config)
    cg.add(hub.register_sensor(config[CONF_OBIS_CODE], var))