cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

executable(
  'respawn',
  ['animation.c', 'main.c', 'menu.c'],
  dependencies: dependencies,
  link_args: link_args,
  include_directories: raygui_include,
)
