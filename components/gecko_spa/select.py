import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID
from . import gecko_spa_ns, GeckoSpa

DEPENDENCIES = ["gecko_spa"]

GeckoSpaSelect = gecko_spa_ns.class_("GeckoSpaSelect", select.Select, cg.Component)

CONF_GECKO_SPA_ID = "gecko_spa_id"

CONFIG_SCHEMA = select.select_schema(GeckoSpaSelect).extend(
    {
        cv.GenerateID(CONF_GECKO_SPA_ID): cv.use_id(GeckoSpa),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GECKO_SPA_ID])
    var = await select.new_select(
        config,
        options=["Away", "Standard", "Energy", "Super Energy", "Weekend"],
    )
    await cg.register_component(var, config)
    cg.add(var.set_parent(parent))
    cg.add(parent.set_program_select(var))
