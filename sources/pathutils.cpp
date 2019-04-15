#include "pathutils.h"

#include <QDir>
#include <QStandardPaths>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace PathUtils {
QString licenseDirPath() {
#ifdef __APPLE__
  char pathbuf[PATH_MAX + 1];
  uint32_t bufsize = sizeof(pathbuf);
  _NSGetExecutablePath(pathbuf, &bufsize);

  QString qpath(pathbuf);
  QStringList qpath_tokens = qpath.split('/');
  qpath_tokens.removeLast();
  qpath_tokens.removeLast();
  qpath_tokens << "Resources"
               << "licenses";
  QString loc_path = qpath_tokens.join("/");
  return QString(loc_path.toUtf8().constData());
#else
  return QString("./config/licenses");
#endif
}

QString translationDirPath() {
#ifdef __APPLE__
  char pathbuf[PATH_MAX + 1];
  uint32_t bufsize = sizeof(pathbuf);
  _NSGetExecutablePath(pathbuf, &bufsize);

  QString qpath(pathbuf);
  QStringList qpath_tokens = qpath.split('/');
  qpath_tokens.removeLast();
  qpath_tokens.removeLast();
  qpath_tokens << "Resources"
               << "loc";
  QString loc_path = qpath_tokens.join("/");
  return QString(loc_path.toUtf8().constData());
#else
  return QDir::currentPath() + "/config/loc";
#endif
}

QString getResourceCloudPresetPath() {
#ifdef __APPLE__
  char pathbuf[PATH_MAX + 1];
  uint32_t bufsize = sizeof(pathbuf);
  _NSGetExecutablePath(pathbuf, &bufsize);

  QString qpath(pathbuf);
  QStringList qpath_tokens = qpath.split('/');
  qpath_tokens.removeLast();
  qpath_tokens.removeLast();
  qpath_tokens << "Resources"
               << "ini";
  QString loc_path = qpath_tokens.join("/");
  return QString(loc_path.toUtf8().constData()) + "/cloudpreset.ini";
#else
  return QDir::currentPath() + "/config/ini/cloudpreset.ini";
#endif
}

QString configDirPath() {
#ifdef __APPLE__
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  return path + "/KumoWorks";
#else
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
#endif
}

QString getDefaultSettingsPath() {
  return configDirPath() + "/defaultsettings.ini";
}

QString getCloudPresetPath() { return configDirPath() + "/cloudpreset.ini"; }

QString getGradientPresetPath() {
  return configDirPath() + "/gradientpreset.ini";
}
}
