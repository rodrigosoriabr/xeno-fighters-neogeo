-- with a SCREEN_TEST build: coin + start, then a snapshot every 150 frames
local ports = manager.machine.ioport.ports
local held = {}
local function press(port, name, frames) held[#held + 1] = {ports[port].fields[name], frames} end
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 900 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 1000 then press(":edge:joy:START", "1 Player Start", 8) end
  if f > 1100 and f % 150 == 0 then manager.machine.video:snapshot() end
  if f >= 8400 then manager.machine:exit() end
end)
