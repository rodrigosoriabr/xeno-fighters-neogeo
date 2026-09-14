-- screenshot tour: intro story, title, options, select, VS, fight HUD (SNAP_AT env overrides nothing)
local ports = manager.machine.ioport.ports
local held = {}
local function press(port, name, frames) held[#held + 1] = {ports[port].fields[name], frames} end
local J = ":edge:joy:JOY1"
local shots = {700, 1000, 1400, 1800, 2150, 2330, 2450, 2560, 2700, 2800, 2950, 3100, 3300, 3500, 3700, 3900, 4200, 4600}
local set = {}
for _, f in ipairs(shots) do set[f] = true end
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 2200 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 2400 then press(":edge:joy:START", "1 Player Start", 8) end
  if f == 2500 then press(J, "P1 Right", 4) end     -- options: difficulty -> HARD
  if f == 2540 then press(J, "P1 A", 4) end
  if f == 2580 then press(J, "P1 A", 4) end
  if f == 2620 then press(J, "P1 A", 4) end          -- start
  if f == 2750 then press(J, "P1 Right", 4) end
  if f == 2780 then press(J, "P1 Down", 4) end      -- cursor on 5 (NYXA)
  if f == 2850 then press(J, "P1 A", 4) end
  if f > 3400 and f % 23 == 0 then press(J, "P1 C", 3) end
  if set[f] then manager.machine.video:snapshot() end
  if f >= 4610 then manager.machine:exit() end
end)
