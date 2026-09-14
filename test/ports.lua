local out = io.open(os.getenv("HOME") .. "/neodev/xeno-fighters/test/ports.txt", "w")
emu.register_frame_done(function()
  if manager.machine.screens[":screen"]:frame_number() == 10 then
    for tag, p in pairs(manager.machine.ioport.ports) do
      for n, _ in pairs(p.fields) do out:write(tag .. " | " .. n .. "\n") end
    end
    out:close()
    manager.machine:exit()
  end
end)
