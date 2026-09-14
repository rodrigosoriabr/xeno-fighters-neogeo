-- attract sequence probe: snapshots across the intro -> demo transition, logs bios_user_mode
local mem = manager.machine.devices[":maincpu"].spaces["program"]
emu.register_frame_done(function()
  local f = manager.machine.screens[":screen"]:frame_number()
  if f % 60 == 0 then print(f, "user_mode", mem:read_u8(0x10fdaf)) end
  if f >= 2000 and f <= 2600 and f % 100 == 0 then manager.machine.video:snapshot() end
  if f >= 2610 then manager.machine:exit() end
end)
