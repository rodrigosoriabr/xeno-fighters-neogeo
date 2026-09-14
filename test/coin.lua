local mem = manager.machine.devices[":maincpu"].spaces["program"]
local ports = manager.machine.ioport.ports
local log = io.open(os.getenv("HOME") .. "/neodev/xeno-fighters/test/coin.log", "w")
local held = {}
local function press(port, name, frames) held[#held + 1] = {ports[port].fields[name], frames} end
local last = ""
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 1300 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 1400 then press(":edge:joy:START", "1 Player Start", 8) end
  local s = string.format("req=%d mode=%d credit=%02x start=%d pmod1=%d mvs=%d demo=%d stat=%02x",
    mem:read_u8(0x10fdae), mem:read_u8(0x10fdaf), mem:read_u8(0x10fdb0), mem:read_u8(0x10fdb4),
    mem:read_u8(0x10fdb6), mem:read_u8(0x10fd82), mem:read_u8(0x100130), mem:read_u8(0x10fdac))
  if s ~= last then log:write(f .. " " .. s .. "\n"); last = s end
  if f == 1500 or f == 1700 or f == 1850 or f == 2000 or f == 2150 or f == 2300 then manager.machine.video:snapshot() end
  if f == 1600 then press(":edge:joy:JOY1", "P1 Right", 4) end
  if f == 1650 then press(":edge:joy:JOY1", "P1 A", 4) end
  if f >= 2320 then log:close(); manager.machine:exit() end
end)
