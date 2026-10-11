local expressions = require("expressions")
local generic = require("generic")
local configloader = require("configloader")
local input = require("input")

--[[
    Boop sensor. Configured in misc.json under "boop":

    trigger_mode        "apds9960" (proximity over I2C) or "gpio" (simple on/off sensor)
    int_gpio            apds9960: pin wired to the sensor's INT (-1 to poll over I2C only)
    threshold           apds9960: proximity 1-255 that counts as a boop (higher = closer)
    gpio / gpio_state   gpio: input pin and the level it reads when booped
    boopTriggerDuration ms the sensor must stay triggered before the boop starts
    longHoldDuration    ms of holding to switch to the next face instead (0 disables)
    boopAnimationName   expression played while booped
    returnToExpression  expression shown after a boop (defaults to main_animation)

    A short boop plays the boop animation and then goes back to the default face.
    Holding past longHoldDuration beeps, and letting go switches to the next face.
]]

local _M = {
    config = {},
    mode = "apds9960",
    configured = false,

    boopIsOn = false,
    triggeredTime = 0,
    longHoldFired = false,
    baseExpression = nil,

    quit = false,
}

local function sensorTriggered()
    if _M.mode == "apds9960" then
        return apdsIsTriggered()
    end
    return digitalRead(_M.gpio) == _M.gpio_state
end

function _M.Load()
    _M.config = configloader.Get().boop
    local config = _M.config

    if config.enabled == false then
        return
    end

    _M.mode = config.trigger_mode or "apds9960"
    _M.boopTriggerDuration = tonumber(config.boopTriggerDuration) or 50
    _M.longHoldDuration = tonumber(config.longHoldDuration) or 1000

    if _M.mode == "apds9960" then
        local threshold = tonumber(dictGet("boop_threshold")) or tonumber(config.threshold) or 60
        local intPin = tonumber(config.int_gpio) or -1
        if not apdsBegin(threshold, intPin) then
            generic.displayWarning("Boop", "APDS-9960 not found", 2000)
            return
        end
    elseif _M.mode == "gpio" then
        if type(config.gpio) ~= 'number' or type(config.gpio_state) ~= 'number' then
            error("Boop mode gpio needs numeric 'gpio' and 'gpio_state' in misc.json")
        end
        _M.gpio = config.gpio
        _M.gpio_state = config.gpio_state
        pinMode(_M.gpio, _M.gpio_state == 1 and INPUT_PULLDOWN or INPUT_PULLUP)
    else
        error("Invalid boop trigger_mode '"..tostring(_M.mode).."'. Allowed: apds9960, gpio")
    end

    local boopExp = expressions.GetExpression(config.boopAnimationName or "boop")
    if not boopExp or boopExp.transition then
        error("boopAnimationName '"..tostring(config.boopAnimationName).."' must be a non-transition expression")
    end
    _M.boopExpression = boopExp.name

    _M.returnExpression = config.returnToExpression or configloader.Get().main_animation
    if _M.returnExpression and not expressions.GetExpression(_M.returnExpression) then
        error("returnToExpression '"..tostring(_M.returnExpression).."' is not a valid expression")
    end

    _M.configured = true
    print("Boop mode is ".._M.mode)
end

function _M.reset()
    _M.boopIsOn = false
    _M.triggeredTime = 0
    _M.longHoldFired = false
end

function _M.manageBoop(dt)
    if not _M.configured then
        return
    end

    if sensorTriggered() then
        _M.triggeredTime = _M.triggeredTime + dt

        if not _M.boopIsOn and _M.triggeredTime >= _M.boopTriggerDuration then
            _M.boopIsOn = true
            _M.baseExpression = expressions.GetCurrentExpressionId()
            expressions.SetExpression(_M.boopExpression)
        end

        if _M.boopIsOn and not _M.longHoldFired and _M.longHoldDuration > 0 and _M.triggeredTime >= _M.longHoldDuration then
            _M.longHoldFired = true
            toneDuration(880, 80)
        end
        return
    end

    if _M.boopIsOn then
        if _M.longHoldFired then
            expressions.Next(_M.baseExpression)
        elseif _M.returnExpression then
            expressions.SetExpression(_M.returnExpression)
        else
            expressions.SetExpression(_M.baseExpression)
        end
    end
    _M.reset()
end

--[[
    Calibration screen (Settings > Calibrate boop, needs a remote).
    Shows the live proximity reading. LEFT/RIGHT move the threshold,
    UP sets it just under the current reading, CONFIRM saves, BACK cancels.
]]

function _M.onEnter()
    if _M.mode ~= "apds9960" or not apdsIsStarted() then
        generic.displayWarning("Boop", "No APDS-9960", 2000)
        return false
    end
    _M.quit = false
    _M.newThreshold = apdsGetThreshold()
    return true
end

function _M.Calibrate(dt)
    local reading = apdsReadProximity()
    _M.lastReading = reading

    if input.readButtonStatus(BUTTON_LEFT) == BUTTON_JUST_PRESSED then
        _M.newThreshold = math.max(1, _M.newThreshold - 5)
    elseif input.readButtonStatus(BUTTON_RIGHT) == BUTTON_JUST_PRESSED then
        _M.newThreshold = math.min(255, _M.newThreshold + 5)
    elseif input.readButtonStatus(BUTTON_UP) == BUTTON_JUST_PRESSED then
        _M.newThreshold = math.max(1, reading - 10)
    elseif input.readButtonStatus(BUTTON_CONFIRM) == BUTTON_JUST_PRESSED then
        apdsSetThreshold(_M.newThreshold)
        dictSet("boop_threshold", tostring(_M.newThreshold))
        dictSave()
        toneDuration(440, 200)
        _M.quit = true
    elseif input.readButtonStatus(BUTTON_BACK) == BUTTON_JUST_PRESSED then
        _M.quit = true
    end
end

function _M.CalibrateDraw()
    local reading = _M.lastReading or 0
    oledClearScreen()
    oledSetFontSize(1)
    oledSetCursor(0, 0)
    oledDrawText("Boop sensitivity\nNow: "..reading.."  Trigger: ".._M.newThreshold)

    local barW = math.floor(reading * 127 / 255)
    oledDrawRect(0, 26, 128, 10, 1)
    oledDrawFilledRect(0, 26, barW, 10, 1)
    local tx = math.floor(_M.newThreshold * 127 / 255)
    oledDrawFilledRect(tx, 22, 1, 18, 1)

    oledSetCursor(0, 44)
    oledDrawText(reading >= _M.newThreshold and "BOOP!" or "")
    oledSetCursor(0, 56)
    oledDrawText("</> adj ^ auto OK save")
    oledDisplay()
end

return _M
