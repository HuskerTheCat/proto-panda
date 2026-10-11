local _M = {}

local json = require("json")
local CONFIG_FILES_DEFAULT = {"/animation.json", "/keybinds.json", "/misc.json", "/wifi.json",  "/hardware.json"}

local requiredConfigs = {"expressions", "overlays", "frames", "leds","boop","fft","scripts",}

function _M.LoadAndMerge(filename)
	local fp, err = io.open(filename, "r")
	if not fp then 
		error("Failed to load "..filename..": "..tostring(err))
	end
	local content = fp:read("*a")
	fp:close()
	json.filename = filename
	local content = json.decode(content)
	for i,b in pairs(content) do  
		if _M.config[i] then
			log("[WARNING] rewriting config "..i..", done by "..filename)
		end
		_M.config[i] = b
	end
end

function _M.Load()
	_M.config = {}

	for i, filename in pairs(CONFIG_FILES_DEFAULT) do
		_M.LoadAndMerge(filename)
	end

	for _, name in pairs(requiredConfigs) do
		if not _M.config[name] then  
			error("Config is missing '"..name.."'")
		end
	end



	return
end

function _M.Get()
	return _M.config
end

return _M