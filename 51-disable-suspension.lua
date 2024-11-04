table.insert (alsa_monitor.rules, {
  matches = {
    {
      -- Matches all sinks.
      { "node.name", "matches", "FreeDV_*" },
    },
  },
  apply_properties = {
    ["session.suspend-timeout-seconds"] = 0,  -- 0 disables suspend
  },
})
