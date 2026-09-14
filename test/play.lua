-- Boots with the real MVS BIOS, inserts a coin, starts, picks a fighter and plays; snapshots on a schedule.
local ports = manager.machine.ioport.ports
local function field(port, name) return ports[port].fields[name] end
local held = {}
local function press(port, name, frames) held[#held + 1] = {field(port, name), frames} end
local J = ":edge:joy:JOY1"
local shots = {}
for f = 1200, 2400, 60 do shots[#shots + 1] = f end
for f = 2500, 5000, 150 do shots[#shots + 1] = f end
local shot_set = {}
for _, f in ipairs(shots) do shot_set[f] = true end
local motion = {"P1 Down", "P1 Down", "P1 Right", "P1 Right"}
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 1300 then press(":AUDIO_COIN", "Coin 1", 6) end
  if f == 1380 then press(":edge:joy:START", "1 Player Start", 6) end
  if f == 1600 then press(J, "P1 Right", 4) end
  if f == 1700 then press(J, "P1 A", 4) end
  if f > 2150 and f < 5000 then
    local t = (f - 2150) % 240
    if t < 60 then press(J, "P1 Right", 1) end
    if t == 62 then press(J, "P1 C", 3) end
    if t == 80 then press(J, "P1 D", 3) end
    if t >= 100 and t < 104 then press(J, motion[t - 99], 1) end
    if t == 104 then press(J, "P1 A", 3) end
    if t == 150 then press(J, "P1 Up", 3); press(J, "P1 Right", 20) end
    if t == 170 then press(J, "P1 D", 3) end
    if t == 200 then press(J, "P1 Down", 12); press(J, "P1 D", 3) end
  end
  if shot_set[f] then manager.machine.video:snapshot() end
  if f >= 5020 then manager.machine:exit() end
end)
