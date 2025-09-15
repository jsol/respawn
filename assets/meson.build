fs = import('fs')

raylib_dep = dependency('raylib')

executable(
  'gen_asset',
  ['main.c'],
  dependencies: [raylib_dep]
)

if fs.is_dir('gen')
  subdir('gen')
endif
