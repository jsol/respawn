#!/bin/bash

echo "#include <raylib.h>" > asset.h
echo "#include <raylib.h>" > asset.c
echo "#include \"asset.h\"" > asset.c
echo "" >> asset.h
echo "enum asset {" >> asset.h
echo "" >> asset.h

echo "switch (ass) {" > asset.c.switch
for f in *.png; do
  echo "$f"
  ../build/assets/gen_asset "$f"

  en=${f%.png}
  echo "ASSET_${en^^}," >> asset.h
  echo "" >> asset.h

  echo "#include \"${en}.h\"" >> asset.c
  echo "" >> asset.c
  echo "'${en}.c'," >> asset.build

  echo "case ASSET_${en^^}:" >> asset.c.switch
  echo "img.data = ${en^^}_DATA;" >> asset.c.switch
  echo "img.width = ${en^^}_WIDTH;" >> asset.c.switch
  echo "img.height = ${en^^}_HEIGHT;" >> asset.c.switch
  echo "img.format = ${en^^}_FORMAT;" >> asset.c.switch
  echo "img.mipmaps = 1;" >> asset.c.switch
  echo "" >> asset.c.switch
  echo "return img;" >> asset.c.switch

  echo "" >> asset.c.switch
done

echo "raylib_dep = dependency('raylib')" > asset.build
echo "lib_assets = library('assets', ['asset.c'], dependencies: raylib_dep)" >> asset.build

echo "};" >> asset.h
echo "" >> asset.h
echo "" >> asset.h
echo "Image asset_get(enum asset);" >> asset.h

echo "Image asset_get(enum asset ass) {" >> asset.c
echo "" >> asset.c
echo "Image img = { 0 };" >> asset.c
echo "" >> asset.c

cat asset.c.switch >> asset.c
rm asset.c.switch

echo "" >> asset.c
echo "}" >>asset.c
echo "return img;" >> asset.c
echo "}" >>asset.c

echo "" >> asset.c
mkdir -p gen
rm gen/*
mv *.h  gen
mv asset.c gen
cp asset.build gen/meson.build

