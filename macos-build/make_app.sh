#!/bin/sh
rsvg-convert ../images/wavbreaker.svg -w 256 -h 256 -o wavbreaker.png
makeicns -256 wavbreaker.png
rm -f wavbreaker.png

mkdir -p wavbreaker.app/Contents/{MacOS,Resources}

cp ../src/wavbreaker wavbreaker.app/Contents/MacOS/
mv wavbreaker.icns wavbreaker.app/Contents/Resources/
cp Info.plist wavbreaker.app/Contents/
find wavbreaker.app -type f -exec ls -l {} +
