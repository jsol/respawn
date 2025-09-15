raylib_dep = dependency('raylib')
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

includes = include_directories(['../engine', '../assets/gen'])
executable(
  'respawn',
  ['animation.c', 'main.c'],
  include_directories: includes,
  dependencies: [raylib_dep, m_dep],
  link_with: [lib_engine]
)
