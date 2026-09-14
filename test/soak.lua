-- Arcade soak: coin, start, pick, then random human input vs the cpu. Logs states; flags stuck states.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local ports = manager.machine.ioport.ports
local J = ports[":edge:joy:JOY1"]
local log = io.open(os.getenv("HOME") .. "/neodev/xeno-fighters/test/soak.log", "w")
local names = {"P1 Up", "P1 Down", "P1 Left", "P1 Right", "P1 A", "P1 B", "P1 C", "P1 D"}
local held = {}
local function press(port, name, frames) held[#held + 1] = {ports[port].fields[name], frames} end
math.randomseed(11)
local last_state = {-1, -1}
local since = {0, 0}
local b0 = tonumber(os.getenv("FIGHTERS") or "0x100138")
local size = tonumber(os.getenv("FIGHTER_SIZE") or "100")
local base = {b0, b0 + size}
local LIMIT = tonumber(os.getenv("SOAK_FRAMES") or "25000")
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  for i = #held, 1, -1 do
    local h = held[i]
    if h[2] <= 0 then h[1]:clear_value(); table.remove(held, i) else h[1]:set_value(1); h[2] = h[2] - 1 end
  end
  if f == 1300 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f == 1400 then press(":edge:joy:START", "1 Player Start", 8) end
  if f % 2000 == 1500 then press(":AUDIO_COIN", "Coin 1", 8) end
  if f % 2000 == 1600 then press(":edge:joy:START", "1 Player Start", 8) end
  if f > 1500 and f % 6 == 0 then
    for _, n in ipairs(names) do J.fields[n]:clear_value() end
    local r = math.random(100)
    if r < 30 then J.fields["P1 Right"]:set_value(1) elseif r < 40 then J.fields["P1 Left"]:set_value(1)
    elseif r < 50 then J.fields["P1 Down"]:set_value(1) elseif r < 55 then J.fields["P1 Up"]:set_value(1) end
    if math.random(100) < 35 then J.fields[names[4 + math.random(4)]]:set_value(1) end
  end
  for i = 1, 2 do
    local st = mem:read_u8(base[i] + 0x19)
    if st ~= last_state[i] then last_state[i] = st; since[i] = f end
    local free = (st <= 2) or st == 12 or st == 17 or st == 16 or st == 18
    if not free and f - since[i] == 600 then
      log:write(string.format("%d STUCK p%d state=%d anim=%d step=%d\n", f, i, st, mem:read_u8(base[i] + 0x1c), mem:read_u8(base[i] + 0x1d)))
    end
  end
  if f % 60 == 0 then
    log:write(string.format("%d p1 st=%d hp=%d | p2 st=%d hp=%d | mode=%d\n", f, mem:read_u8(base[1] + 0x19), mem:read_i16(base[1] + 0x20),
      mem:read_u8(base[2] + 0x19), mem:read_i16(base[2] + 0x20), mem:read_u8(0x10fdaf)))
  end
  if f % 1500 == 0 then manager.machine.video:snapshot() end
  if f >= LIMIT then log:close(); manager.machine:exit() end
end)
