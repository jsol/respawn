
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

src = [
  'common.c',
  'engine.c',
  'incident.c',
  'map.c',
  'map_opts.c',
  'map_opts_ranked.c',
  'message.c',
  'player.c',
  'player_local.c',
  'player_npc.c',
  'portals.c',
  'spell.c',
]
lib_engine = library('respawn-engine', src, dependencies: m_dep)
