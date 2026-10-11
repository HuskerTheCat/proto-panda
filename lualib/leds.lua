local _M = {}

local configloader = require("configloader")
local generic = require("generic")

--[[
    RGB cheek strips (non-addressable, 4-wire). Configured in hardware.json:

    "leds": {
        "enabled": true,
        "pins": { "r": 41, "g": 42, "b": 13 },
        "invert": false,
        "mode": "solid",
        "color": "#00aaff",
        "speed": 2000
    }

    mode  | what it does                        | uses
    ------|-------------------------------------|---------------
    off   | strips off                          | -
    solid | fixed color                         | color
    pulse | color breathing in and out          | color, speed
    rainbow | cycles through every hue          | speed
    blink | color on/off                        | color, speed

    speed is the length of one cycle in milliseconds.
    Expressions in animation.json can override this with their own "leds" table.
]]

local modeMap = {
    ['off'] = LED_MODE_OFF,
    ['solid'] = LED_MODE_SOLID,
    ['pulse'] = LED_MODE_PULSE,
    ['rainbow'] = LED_MODE_RAINBOW,
    ['blink'] = LED_MODE_BLINK,
}

_M.enabled = false

local function parseColor(color)
    if color == nil then
        return 255, 255, 255
    end
    if type(color) == 'table' then
        return tonumber(color[1]) or 0, tonumber(color[2]) or 0, tonumber(color[3]) or 0
    end
    if type(color) == 'string' then
        local hex = color:gsub("#", "")
        if #hex == 6 then
            return tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16), tonumber(hex:sub(5, 6), 16)
        end
    end
    error("Invalid led color '"..tostring(color).."'. Use \"#rrggbb\" or [r, g, b]")
end

function _M.apply(conf)
    if not _M.enabled or not conf then
        return
    end
    local mode = modeMap[conf.mode or 'solid']
    if not mode then
        error("No such led mode: "..tostring(conf.mode)..". Allowed: off, solid, pulse, rainbow, blink")
    end
    local r, g, b = parseColor(conf.color)
    ledsSetMode(mode, r, g, b, tonumber(conf.speed) or 2000)
end

function _M.begin()
    local leds = configloader.Get().leds

    if not leds or leds.enabled == false then
        generic.displaySplashMessage("No leds defined")
        delay(1000)
        return
    end

    if type(leds.pins) ~= 'table' or not leds.pins.r or not leds.pins.g or not leds.pins.b then
        error("Leds need 'pins' with 'r', 'g' and 'b' in hardware.json")
    end

    if not ledsBeginRgb(leds.pins.r, leds.pins.g, leds.pins.b, leds.invert == true) then
        error("Failed to start the RGB leds. Check the pins in hardware.json")
    end
    _M.enabled = true
    _M.apply(leds)
end

return _M
