import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_ID

mygx_ns = cg.esphome_ns.namespace("mygxdisplay")
MyGxDisplay = mygx_ns.class_("MyGxDisplay", display.DisplayBuffer, cg.PollingComponent)

CONF_WIDTH = "width"
CONF_HEIGHT = "height"
CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_CS_PIN = "cs_pin"
CONF_DC_PIN = "dc_pin"
CONF_RST_PIN = "reset_pin"
CONF_BUSY_PIN = "busy_pin"

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MyGxDisplay),
    cv.Required(CONF_WIDTH): cv.int_,
    cv.Required(CONF_HEIGHT): cv.int_,
    cv.Required(CONF_SCK_PIN): cv.int_,
    cv.Required(CONF_MOSI_PIN): cv.int_,
    cv.Optional(CONF_MISO_PIN, default=-1): cv.int_,
    cv.Required(CONF_CS_PIN): cv.int_,
    cv.Required(CONF_DC_PIN): cv.int_,
    cv.Required(CONF_RST_PIN): cv.int_,
    cv.Required(CONF_BUSY_PIN): cv.int_,
}).extend(cv.polling_component_schema("never"))

async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_WIDTH], config[CONF_HEIGHT],
        config[CONF_SCK_PIN], config[CONF_MOSI_PIN], config[CONF_MISO_PIN],
        config[CONF_CS_PIN], config[CONF_DC_PIN], config[CONF_RST_PIN], config[CONF_BUSY_PIN]
    )
    # НЕ вызываем cg.register_component здесь — это сделает register_display
    await display.register_display(var, config)
