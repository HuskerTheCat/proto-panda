local _M = {
	shouldStop = false,
	VERSION_REQUIRED="3.2.1",
}

local map = require("generic").map
local input = require("input")
local drivers = require("drivers")

-- Draws a 5-bar signal strength indicator (weakest at top, strongest at bottom)
local function drawSignalStrength(x, y, rssi)
	rssi = rssi or -100
	local numBars = 1
	if rssi >= -30 then
		numBars = 5
	elseif rssi >= -50 then
		numBars = 4
	elseif rssi >= -70 then
		numBars = 3
	elseif rssi >= -85 then
		numBars = 2
	end
	for i = 0, numBars do
		oledDrawLine(x + i*2, y+11, x + i*2, y+11 - i*2 + 1, 1)
	end
end

function _M.onSetup()
	setPanelManaged(false)
	ledsSetManaged(false)
	_M.rssi = {}
	for i=0, MAX_BLE_CLIENTS-1 do
		_M.rssi[i] = -100
	end
	_M.swapTimer = 0
end

function _M.onLoop(dt)
	clearPanelBuffer()
	oledClearScreen()

	local maxN = drivers.maxClients
	if maxN < 1 then maxN = 1 end

	-- Refresh RSSI for connected devices every 5 seconds
	if _M.swapTimer < millis() then
		_M.swapTimer = millis() + 5 * 1000
		for i=0, MAX_BLE_CLIENTS-1 do
			if isElementIdConnected(i) then
				local connId = getClientIdFromControllerId(i)
				if connId >= 0 then
					_M.rssi[i] = getRRSI(connId)
				end
			end
		end
	end

	local rowH = math.max(4, math.floor(32 / maxN))
	local btnSize = math.min(6, rowH - 2)
	if btnSize < 2 then btnSize = 2 end

	for dev=0, maxN-1 do
		local first = _G['DEVICE_'..dev..'_BUTTON_FIRST']
		local last  = _G['DEVICE_'..dev..'_BUTTON_LAST']
		local left  = _G['DEVICE_'..dev..'_BUTTON_LEFT']
		local y = dev * rowH
		local hasPressed = false

		for i=first, last do
			local x = 2 + (i - left) * (btnSize + 1)
			local pressed = input.readButtonStatus(i) == BUTTON_PRESSED or input.readButtonStatus(i) == BUTTON_JUST_PRESSED
			if pressed then
				hasPressed = true
				drawPanelFillRect(x, y, btnSize, btnSize, color565(255,255,255))
			else
				drawPanelRect(x, y, btnSize, btnSize, color565(120,120,120))
			end
		end

		if hasPressed then
			ledsSetColor(100, 100, 100)
		else
			ledsSetColor(0, 0, 0)
		end
	end

	local colW = math.max(8, math.floor(64 / maxN))
	local half = math.floor(colW / 2)

	for dev=0, maxN-1 do
		local x0 = 64 + dev * colW
		local cx = x0 + half

		-- accelerometer box, top half of the panel (y 0-15)
		drawPanelRect(x0, 0, colW, 16, color565(90,90,90))
		local ax = input.readAccelerometerX(dev)
		local ay = input.readAccelerometerY(dev)
		local az = input.readAccelerometerZ(dev)
		local s = half - 1
		drawPanelLine(cx, 8, cx + s*ax, 8, color565(255,0,0))
		drawPanelLine(cx, 8, cx, 8 + s*ay, color565(0,255,0))
		drawPanelLine(cx, 8, cx + s*az, 8 + s*az, color565(0,0,255))

		-- gyro box, bottom half of the panel (y 16-31)
		drawPanelRect(x0, 16, colW, 16, color565(90,90,90))
		local gx = map(input.readGyroX(dev), -12000, 12000, -s, s)
		local gy = map(input.readGyroY(dev), -12000, 12000, -s, s)
		local gz = map(input.readGyroZ(dev), -12000, 12000, -s, s)
		drawPanelLine(cx, 24, cx + gx, 24, color565(0,255,255))
		drawPanelLine(cx, 24, cx, 24 + gy, color565(255,0,255))
		drawPanelLine(cx, 24, cx + gz, 24 + gz, color565(255,255,0))
	end

	local oledColW = math.floor(128 / maxN)

	for dev=0, maxN-1 do
		local ox = dev * oledColW
		local first = _G['DEVICE_'..dev..'_BUTTON_FIRST']
		local last  = _G['DEVICE_'..dev..'_BUTTON_LAST']
		local left  = _G['DEVICE_'..dev..'_BUTTON_LEFT']

		oledSetCursor(ox, 0)
		oledDrawText("C"..(dev+1))

		-- small button squares
		local bsize = math.min(6, math.floor(oledColW / (last-first+2)))
		if bsize < 2 then bsize = 2 end
		for i=first, last do
			local bx = ox + (i-left) * (bsize+1)
			local pressed = input.readButtonStatus(i) == BUTTON_PRESSED or input.readButtonStatus(i) == BUTTON_JUST_PRESSED
			if pressed then
				oledDrawFilledRect(bx, 10, bsize, bsize, 1)
			else
				oledDrawRect(bx, 10, bsize, bsize, 1)
			end
		end

		-- accelerometer crosshair box
		local boxSize = math.min(oledColW - 4, 28)
		local bx0 = ox + 2
		local by0 = 22
		oledDrawRect(bx0, by0, boxSize, boxSize, 1)
		local ccx = bx0 + math.floor(boxSize/2)
		local ccy = by0 + math.floor(boxSize/2)
		local s = math.floor(boxSize/2) - 1

		local ax = input.readAccelerometerX(dev)
		local ay = input.readAccelerometerY(dev)
		local az = input.readAccelerometerZ(dev)

		-- axis crosshair
		oledDrawLine(bx0, ccy, bx0+boxSize, ccy, 1)
		oledDrawLine(ccx, by0, ccx, by0+boxSize, 1)
		-- tilt vector (X/Y) as a line from center, Z shown as a filled dot size
		oledDrawLine(ccx, ccy, ccx + s*ax, ccy + s*ay, 1)
		local dotR = math.max(1, math.min(3, math.floor(math.abs(az)*3)))
		oledDrawFilledRect(ccx + s*ax - dotR, ccy + s*ay - dotR, dotR*2+1, dotR*2+1, 1)
	end

	-- Connected devices row, bottom of the OLED (y 48-63): one 13x13 box per BLE slot.
	local posY = 48
	local startX = 0
	for i=0, MAX_BLE_CLIENTS-1 do
		oledDrawRect(startX+2, posY+2, 13, 13, 1)
		if isElementIdConnected(i) then
			drawSignalStrength(startX+2, posY+2, _M.rssi[i])
		elseif _M.logo_no_handy then
			oledDrawLine(startX+2, posY+2, startX+14, posY+14, 1)
			oledDrawLine(startX+14, posY+2, startX+2, posY+14, 1)
		end
		startX = startX + 15
	end

	oledDisplay()
	flipPanelBuffer()
	ledsDisplay()
	if  (input.readButtonStatus(BUTTON_CONFIRM) == BUTTON_PRESSED and input.readButtonStatus(BUTTON_LEFT) == BUTTON_PRESSED and input.readButtonStatus(BUTTON_RIGHT) == BUTTON_PRESSED and input.readButtonStatus(BUTTON_UP) == BUTTON_PRESSED and input.readButtonStatus(BUTTON_DOWN) == BUTTON_PRESSED) then 
		_M.shouldStop = true
		return true
	end
end 

function _M.onClose()

end

return _M