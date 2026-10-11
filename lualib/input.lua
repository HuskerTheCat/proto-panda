local versions = require("versions")
local drivers = require("drivers")
local configloader = require("configloader")

local ACCELEROMETER_MULTIPLIER =  4 / 32768

local input = {
    started = false,
   
    panda_buttons = {},
    panda_buttons_state = {},

    legacyReadButtonStatus = readButtonStatus,
    legacyreadAccelerometerX = readAccelerometerX,
    legacyreadAccelerometerY = readAccelerometerY,
    legacyreadAccelerometerZ = readAccelerometerZ,
    legacyreadGyroX = readGyroX,
    legacyreadGyroY = readGyroY,
    legacyreadGyroZ = readGyroZ,
    legacygetBleDeviceLastUpdate = getBleDeviceLastUpdate,

    mode = "NONE",

    maxControls=2,

    keybind = {},
    forced_buttons = {},
}

_G.BUTTON_RELEASED = 0
_G.BUTTON_JUST_PRESSED = 1
_G.BUTTON_PRESSED = 2
_G.BUTTON_JUST_RELEASED = 3


_G.BUTTON_LEFT = 1
_G.BUTTON_DOWN = 2
_G.BUTTON_RIGHT = 3
_G.BUTTON_UP = 4
_G.BUTTON_CONFIRM = 5
_G.BUTTON_BACK = 6
_G.BUTTON_AUX_A = 7
_G.BUTTON_AUX_B = 8


function input.parseInputLocation(str)
    local idx = 1
    local parsed = {
        resource = {},
        reference = false
    }
    local controllerType = ""
    local mappedLogString = ""
    for element in str:gmatch("([^%.]+)") do  
        if idx == 1 then  
            if not drivers[element] then  
                error("There is no driver for the type '"..element.."' in "..str)
            end
            controllerType = element
        elseif idx == 2 then  
            local handler = drivers[controllerType][0]
            local toMatch = nil

            if not handler then  
                error("Unavalible resource '"..controllerType.."'")
            end
            
            local mode = 'none'
            if element:match(".-=.+") then 
                mode = 'equals'
                element, toMatch = element:match("(.-)=(.+)")
            elseif element:match(".->.+") then 
                mode = 'greater'
                element, toMatch = element:match("(.-)>(.+)")
                toMatch = tonumber(toMatch)
                if not toMatch then 
                    error("Unavalible resource '"..element.."' in '"..str.."'. is expected to be a number")
                end
            elseif element:match(".-<.+") then 
                mode = 'lesser'
                element, toMatch = element:match("(.-)<(.+)")
                toMatch = tonumber(toMatch)
                if not toMatch then 
                    error("Unavalible resource '"..element.."' in '"..str.."'. is expected to be a number")
                end
            end


            parsed.name = element
            if not handler[element] then  
                local avaliable = ""
                for i in pairs(handler) do  
                    avaliable = avaliable .. i..', ' 
                end
                error("Unavalible resource '"..element.."' in '"..str.."'. Avaliable: "..avaliable)
            end


            --Reference of the element directly
            for i=0,MAX_BLE_CLIENTS-1 do 
                local reference = drivers[controllerType][i][element]
                if type(reference) == 'table' then  
                    parsed.reference = true
                    parsed.resource[i] = reference
                else
                    parsed.resource[i] = element
                end
            end
            
            if toMatch then  
                parsed.match = tonumber(toMatch) or toMatch
                parsed.mode = mode
                mappedLogString = "'"..controllerType.."' when '"..element.."' equals '"..parsed.match.."'"
                break
            end
        elseif idx == 3 then
            mappedLogString = "'"..controllerType.."' directly on '"..parsed.name.."["..element.."]'"
            parsed.location = tonumber(element) or element
            break
        end
        idx = idx+1
    end

    local dataLocation = drivers[controllerType][0][parsed.name]
    if type(dataLocation) ~= 'table' and not parsed.match then   
        error("Cannot use indexing on '"..str.."'. Maybe you meant = on the last dot?")
    end
    return parsed,controllerType, mappedLogString
end

function input.setupControls(maxN)
    local pdButtonIdOffset = 1
    for i=0,maxN-1 do 
        for b=1,MAX_BLE_BUTTONS do 
            input.panda_buttons[pdButtonIdOffset] = 0
            input.panda_buttons_state[pdButtonIdOffset] = _G.BUTTON_RELEASED
            input.forced_buttons[pdButtonIdOffset] = 0
            pdButtonIdOffset = pdButtonIdOffset +1
        end
        _G['DEVICE_'..i..'_BUTTON_LEFT']    = _G.BUTTON_LEFT        + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_DOWN']    = _G.BUTTON_DOWN        + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_RIGHT']   = _G.BUTTON_RIGHT       + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_UP']      = _G.BUTTON_UP          + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_CONFIRM'] = _G.BUTTON_CONFIRM     + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_BACK']    = _G.BUTTON_BACK        + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_AUX_A']   = _G.BUTTON_AUX_A       + i * MAX_BLE_BUTTONS
        _G['DEVICE_'..i..'_BUTTON_AUX_B']   = _G.BUTTON_AUX_B       + i * MAX_BLE_BUTTONS
        

        _G['DEVICE_'..i..'_BUTTON_FIRST']   = _G.BUTTON_LEFT        + i * MAX_BLE_BUTTONS   
        _G['DEVICE_'..i..'_BUTTON_LAST']    = _G.BUTTON_AUX_B       + i * MAX_BLE_BUTTONS
    end
end

function input.Start()
    if input.mode == "BLE" then  
        beginBleScanning()
    elseif input.mode == "NONE" then
    else
        error("Invalid input mode '"..input.mode.."'")
    end
end

function input.Load()
    local confs = configloader.Get()

    if not confs.input then  
        error("keybinds.json missing 'input'")
    end

    local mode = confs.input.mode or "BLE"
    mode = mode:upper()
    input.mode = mode

    if not confs.input.drivers or #confs.input.drivers == 0 then  
        confs.input.drivers = {"generic"}
    end

    local maxN = confs.input and (confs.input.maxBleDevices or 2) or 2
    _G.MAX_BLE_CLIENTS = maxN
    input.maxControls =  maxN
    input.setupControls(maxN)
    print("Gonna use "..maxN.." controllers")

    drivers.Start(maxN)

    if mode == "BLE" then
        setLogDiscoveredBleDevices(true)
        generic.displaySplashMessage("Starting:\nBLE")
        startBLE()
        setMaximumControls(maxN)
        startBLERadio(ESP_PWR_LVL_P9)
    elseif mode == "NONE" then
        --We just accept
    else
        error("Invalid input mode: "..tostring(mode))
    end

    drivers.EnableDrivers(confs.input)


    drivers.WrapUp()

    input.SetKeybinds(confs.keybinds) 
end

function input.SetKeybinds(keybinds) 
    for location, target in pairs(keybinds) do  
        local binding, controllerType, logMsg = input.parseInputLocation(location)
        local toInsert = input.keybind[controllerType]
        if not toInsert then  
            toInsert = {}
            --Lua stores references of tables :3
            --This makes our life easier
            input.keybind[controllerType] = toInsert
        end
        if not _G[target] or not tonumber(_G[target]) then  
            error("Unknown action "..target)
        end
        log("[KEYBIND] Mapped "..tostring(target).." on "..tostring(logMsg))
        binding.action = tonumber(_G[target])
        toInsert[#toInsert+1] = binding
    end
end

function input.updatGenericButtonStates(inputModes, clientId, pdButtonIdOffset)
    local keybind = input.keybind
    local actions = {}
    for b=1,MAX_BLE_BUTTONS do
        actions[b] = 0
    end

    for _, name in pairs(inputModes) do
        local controller = drivers[name]
        --Search for inputs on each type.. joystick, panda, mouse, keyboard etc
        if keybind[name] then
            for __, bind in pairs(keybind[name]) do
                local element = bind.resource[clientId]
                local reading = 0
                if not bind.reference then  
                    element = controller[clientId][element]
                end
                if bind.match then  
                    if bind.mode == 'equals' then
                        reading = element == bind.match and 1 or 0
                    elseif bind.mode == 'greater' then 
                        local n = tonumber(element) or 0
                        if n > bind.match then  
                            reading = 1
                        else 
                            reading = 0
                        end
                    elseif bind.mode == 'lesser' then 
                        local n = tonumber(element) or 0
                        if n < bind.match then  
                            reading = 1
                        else 
                            reading = 0
                        end
                    end
                elseif bind.location then 
                    if type(element) ~= 'table' then  
                        error(tostring(bind.resource[clientId]).." is looking for "..tostring(controller[clientId]).." at "..bind.location.." ref is "..tostring(bind.reference).." element = "..tostring(element))
                    end
                    reading = element[bind.location]
                end
                if reading == 1 and actions[bind.action] == 0 then  
                    actions[bind.action] = 1
                end
            end
        end
    end
    if input.button_timeout then 
        if input.button_timeout < millis() then  
            for idx=1,MAX_BLE_BUTTONS do
                input.forced_buttons[idx-pdButtonIdOffset] = nil
            end
            input.button_timeout = 0
        else 
            for btn,state in pairs(input.forced_buttons) do  
                actions[btn-pdButtonIdOffset] = state
            end
        end
    end
    for idx=1,MAX_BLE_BUTTONS do
        input.updateButtonByreading(pdButtonIdOffset+idx, actions[idx])
    end
end

function input.updateButtonByreading(buttonId, reading)
    if input.panda_buttons_state[buttonId] == _G.BUTTON_JUST_PRESSED then  
        input.panda_buttons_state[buttonId] = _G.BUTTON_PRESSED
    end
    if input.panda_buttons_state[buttonId] == _G.BUTTON_JUST_RELEASED then  
        input.panda_buttons_state[buttonId] = _G.BUTTON_RELEASED
    end

    if input.panda_buttons[buttonId] ~= reading then 
        input.panda_buttons[buttonId] = reading
        if reading == 1 then 
            input.panda_buttons_state[buttonId] = _G.BUTTON_JUST_PRESSED
        else
            input.panda_buttons_state[buttonId] = _G.BUTTON_JUST_RELEASED
        end
    end
end

function press(key, controller)
    controller = controller or 1
    input.forced_buttons[key + (controller-1) * MAX_BLE_BUTTONS] = _G.BUTTON_JUST_PRESSED
    input.button_timeout = millis() + 250
end

_G.press = press

function input.update()
    local pdButtonIdOffset = 0
    for i=0,MAX_BLE_CLIENTS-1 do 
        local controllerList = drivers.type_by_id[i]
        if controllerList ~= nil then
            input.updatGenericButtonStates(controllerList.mode, i, pdButtonIdOffset)
        end
        pdButtonIdOffset = pdButtonIdOffset + MAX_BLE_BUTTONS
    end
end



function input.getBleDeviceLastUpdate(id)
    if input.legacygetBleDeviceLastUpdate then  
        return input.legacygetBleDeviceLastUpdate(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    return dv.last_update
end

function input.readAccelerometerX(id)
    if input.legacyreadAccelerometerX then  
        return input.legacyreadAccelerometerX(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    local v = dv.ax
    return v * ACCELEROMETER_MULTIPLIER
end

function input.readAccelerometerY(id)
    if input.legacyreadAccelerometery then  
        return input.legacyreadAccelerometery(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    local v = dv.ay
    return v * ACCELEROMETER_MULTIPLIER
end

function input.readAccelerometerZ(id)
    if input.legacyreadAccelerometerz then  
        return input.legacyreadAccelerometerz(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    local v = dv.az
    return v * ACCELEROMETER_MULTIPLIER
end

function input.readGyroX(id)
    if input.legacyreadGyroX then  
        return input.legacyreadGyroX(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    return dv.gx
end

function input.readGyroY(id)
    if input.legacyreadGyroY then  
        return input.legacyreadGyroY(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    return dv.gy
end

function input.readGyroZ(id)
    if input.legacyreadGyroZ then  
        return input.legacyreadGyroZ(id)
    end
    local dv = drivers.panda[id]
    if not dv then 
        return 0
    end
    return dv.gz
end

function input.readButtonStatus(button)
    if input.legacyReadButtonStatus then  
        return input.legacyReadButtonStatus()
    end
    return input.panda_buttons_state[button]
end

local function checkRegisterGlobalCompat(gfname, fn)
    if _G[gfname] then 
        if _G[gfname] ~= fn then  
            print("Rewritten function "..gfname..' in legacy mode')
        end
    end
end

checkRegisterGlobalCompat('readButtonStatus', input.readButtonStatus)
checkRegisterGlobalCompat('readAccelerometerX', input.readAccelerometerX)
checkRegisterGlobalCompat('readAccelerometerY', input.readAccelerometerY)
checkRegisterGlobalCompat('readAccelerometerZ', input.readAccelerometerZ)
checkRegisterGlobalCompat('readGyroX', input.readGyroX)
checkRegisterGlobalCompat('readGyroY', input.readGyroY)
checkRegisterGlobalCompat('readGyroZ', input.readGyroZ)
checkRegisterGlobalCompat('getBleDeviceLastUpdate', input.getBleDeviceLastUpdate)
checkRegisterGlobalCompat('getBleDeviceUpdateDt', input.getBleDeviceLastUpdate)



return input