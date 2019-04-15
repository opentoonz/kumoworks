#pragma once
#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>

namespace PathUtils {
QString licenseDirPath();
QString translationDirPath();
QString getResourceCloudPresetPath();
QString configDirPath();
QString getDefaultSettingsPath();
QString getCloudPresetPath();
QString getGradientPresetPath();
}

#endif
