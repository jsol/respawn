fs = import('fs')

executable(
  'gen_asset',
  ['main.c'],
  dependencies: dependencies
)

if fs.is_dir('gen')
  subdir('gen')
endif
