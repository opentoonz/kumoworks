#!/bin/bash
MACDEPLOYQT_PATH=~/Qt5.9.2/5.9.2/clang_64/bin/macdeployqt
if [ $# = 1 ]; then
    MACDEPLOYQT_PATH=$1
fi
echo "MACDEPLOYQT_PATH = $MACDEPLOYQT_PATH"
$MACDEPLOYQT_PATH ../build/KumoWorks.app
cp -r ../sources/loc ../build/KumoWorks.app/Contents/Resources/
cp -r ../misc/licenses ../build/KumoWorks.app/Contents/Resources/
cp -r ../misc/ini ../build/KumoWorks.app/Contents/Resources/
