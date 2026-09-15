-- trailer capture (real BIOS): intro, options EASY, select NYXA, scripted fight; Nyxa hits 7x, CPU hits 1/6 so she wins on camera. Snapshot every 30 frames (file N = frame 30*(N+1)). Record with -aviwrite, cut with ffmpeg (imageio-ffmpeg via pip).
local ports = manager.machine.ioport.ports
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local P1, P2 = 0x100120, 0x100120 + 162
local last1, last2
local held = {}
local J = ":edge:joy:JOY1"
local flip = false
local function press(port, name, frames, delay)
  if flip and port == J then
    if name == "P1 Right" then name = "P1 Left" elseif name == "P1 Left" then name = "P1 Right" end
  end
  held[#held + 1] = {ports[port].fields[name], frames, delay or 0}
end
local function motion(list, button, start)
  local t = start
  for _, d in ipairs(list) do
    for n in d:gmatch("[^|]+") do press(J, n, 4, t) end
    t = t + 3
  end
  press(J, button, 5, t)
end
local FWD = "P1 Right"
local moves = {
  function(t) motion({"P1 Down", "P1 Down|P1 Right", "P1 Right"}, "P1 A", t) end,           -- 236A projectile
  function(t) press(J, FWD, 30, t); press(J, "P1 C", 4, t + 32); press(J, "P1 D", 4, t + 48) end,
  function(t) press(J, "P1 Up", 6, t); press(J, FWD, 20, t); press(J, "P1 D", 4, t + 16) end,  -- jump in kick
  function(t) press(J, "P1 Down", 20, t); press(J, "P1 D", 4, t + 4) end,                     -- sweep
  function(t) motion({"P1 Right", "P1 Down", "P1 Down|P1 Right"}, "P1 B", t) end,           -- 623B rising
  function(t) motion({"P1 Down", "P1 Down|P1 Right", "P1 Right", "P1 Down", "P1 Down|P1 Right", "P1 Right"}, "P1 D", t) end, -- super
  function(t) press(J, FWD, 24, t); press(J, "P1 A", 3, t + 26); press(J, "P1 C", 3, t + 34) end,
}
local k = 0
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[3] > 0 then h[3] = h[3] - 1
    elseif h[2] <= 0 then h[1]:clear_value(); table.remove(held, i)
    else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 2200 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 2400 then press(":edge:joy:START", "1 Player Start", 8) end
  if f == 2590 then press(J, "P1 Left", 4) end
  if f == 2620 then press(J, "P1 A", 4) end
  if f == 2640 then press(J, "P1 A", 4) end
  if f == 2660 then press(J, "P1 A", 4) end
  if f == 2750 then press(J, "P1 Right", 4) end
  if f == 2800 then press(J, "P1 Down", 4) end
  if f == 2900 then press(J, "P1 A", 4) end
  -- trailer balance: Nyxa's hits count 4x, the CPU's hits count 1/3 (health at +0x20)
  local hp1, hp2 = mem:read_i16(P1 + 0x20), mem:read_i16(P2 + 0x20)
  if last1 and hp1 < last1 and hp1 > 0 then hp1 = last1 - math.max(1, (last1 - hp1) // 6); mem:write_i16(P1 + 0x20, hp1) end
  if last2 and hp2 < last2 and hp2 > 0 then hp2 = math.max(0, last2 - (last2 - hp2) * 7); if hp2 == 0 then hp2 = 1 end; mem:write_i16(P2 + 0x20, hp2) end
  last1, last2 = hp1, hp2
  if f >= 3400 and f % 55 == 0 then flip = mem:read_u8(P1 + 0x18) == 0 end
  if f >= 3400 and f % 55 == 0 then k = k % #moves + 1; moves[k](0) end
  if f % 30 == 0 then manager.machine.video:snapshot() end
  if f >= 8400 then manager.machine:exit() end
end)
