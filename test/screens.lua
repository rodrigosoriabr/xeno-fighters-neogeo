local ports = manager.machine.ioport.ports
local held = {}
local function press(port, name, frames) held[#held + 1] = {ports[port].fields[name], frames} end
local J = ":edge:joy:JOY1"
local shots = {1000, 1250, 1450, 1520, 1600, 1700, 1800, 1860, 1920, 2000, 2080, 2200}
local set = {}
for _, f in ipairs(shots) do set[f] = true end
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 1300 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 1400 then press(":edge:joy:START", "1 Player Start", 8) end
  if f == 1500 then press(J, "P1 Right", 4) end
  if f == 1560 then press(J, "P1 Down", 4) end
  if f == 1650 then press(J, "P1 A", 4) end
  if set[f] then manager.machine.video:snapshot() end
  if f >= 2210 then manager.machine:exit() end
end)
