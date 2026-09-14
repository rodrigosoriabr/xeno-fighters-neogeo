-- demo mode: log fighters and snapshot the first frames where P2 is airborne
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local log = io.open(os.getenv("HOME") .. "/neodev/xeno-fighters/test/jump.log", "w")
local shots = 0
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  local out = {}
  for i, base in ipairs({0x100138, 0x10019c}) do
    out[#out + 1] = string.format("p%d x=%d y=%d st=%d an=%d sp=%d cols=%d", i, mem:read_i32(base + 8) // 256,
      mem:read_i32(base + 12) // 256, mem:read_u8(base + 0x19), mem:read_u8(base + 0x1c), mem:read_u8(base + 0x1d), mem:read_u8(base + 0x36))
  end
  local y2 = mem:read_i32(0x10019c + 12) // 256
  local y1 = mem:read_i32(0x100138 + 12) // 256
  if f >= 1552 and f <= 1567 and f % 3 == 0 then
    manager.machine.video:snapshot(); shots = shots + 1
    log:write(f .. " SNAP " .. table.concat(out, " | ") .. "\n")
  end
  if f >= 1570 then log:close(); manager.machine:exit() end
end)
