import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT, DEVICE_CLASS_TEMPERATURE
from . import gecko_spa_ns, GeckoSpa

DEPENDENCIES = ["gecko_spa"]

CONF_GECKO_SPA_ID = "gecko_spa_id"
CONF_SENSOR_TYPE = "type"

SENSOR_TYPES = {
    "rinse_filter": "RINSE_FILTER",
    "clean_filter": "CLEAN_FILTER",
    "change_water": "CHANGE_WATER",
    "spa_checkup": "SPA_CHECKUP",
}

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="d",
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_TEMPERATURE,
).extend(
    {
        cv.GenerateID(CONF_GECKO_SPA_ID): cv.use_id(GeckoSpa),
        cv.Required(CONF_SENSOR_TYPE): cv.enum(SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GECKO_SPA_ID])
    var = await sensor.new_sensor(config)

    sensor_type = config[CONF_SENSOR_TYPE]
    if sensor_type == "rinse_filter":
        cg.add(parent.set_rinse_filter_sensor(var))
    elif sensor_type == "clean_filter":
        cg.add(parent.set_clean_filter_sensor(var))
    elif sensor_type == "change_water":
        cg.add(parent.set_change_water_sensor(var))
    elif sensor_type == "spa_checkup":
        cg.add(parent.set_spa_checkup_sensor(var))
