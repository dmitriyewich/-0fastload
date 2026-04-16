-- thx gennariarmando and LINK/2012 and KepchiK
local memory = require 'memory'
if memory.getuint32(0xC8D4C0, true) < 9 then
	memory.fill(0x747483, 0x90, 6, true) -- Initialize game state US 1.0
	memory.setuint32(0xC8D4C0, 5, true)
	memory.fill(0x748CF6, 0x90, 5, true)
	if getModuleHandle("sampfuncs.asi") ~= 0 then
		memory.fill(0x748C9A, 0x90, 5, true)
	end
	memory.fill(0x748AA8, 0x90, 6, true)
	memory.hex2bin('C705C0D4C80005000000', 0x748AA8, 10)
	memory.fill(0x748C23, 0x90, 5, true)
	if getModuleHandle("_CoreGame.asi") == 0 then
		memory.fill(0x748C2B, 0x90, 5, true)
	end
end
