
#define _USE_MATH_DEFINES
#include <QApplication>
#include <QTranslator>

#include "mainwindow.h"
#include "myparams.h"
#include "cloud.h"
#include "pathutils.h"
#include <iostream>
#include <QSettings>
#include <QDir>
#include <QFile>

using namespace PathUtils;

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  a.setApplicationName("KumoWorks");
  a.setApplicationVersion("1.0");

  std::cout << "path = " << QDir::currentPath().toStdString() << std::endl;

  QString langName("en");
  if (QFile::exists(getDefaultSettingsPath())) {
    QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);

    settings.beginGroup("Preferences");
    langName = settings.value("language", langName).toString();
    settings.endGroup();
  }

  QTranslator translator;
  if (langName != "en") {
    bool ret = translator.load(QLocale(langName), "KumoWorks", "_",
                               translationDirPath(), ".qm");

    if (!ret)
      qWarning("failed-no tr file");
    else
      a.installTranslator(&translator);
  }

  // copy cloudpreset.ini if it does not exist
  if (!QFile(getCloudPresetPath()).exists()) {
    // create configuration directory
    QDir(configDirPath()).mkpath("tmp");
    QDir(configDirPath()).rmdir("tmp");
    bool ret = QFile::copy(getResourceCloudPresetPath(), getCloudPresetPath());
    if (!ret) qWarning("failed to copy cloud preset");
  }

  // generate defaultsettings.ini if it does not exist
  if (!QFile(getDefaultSettingsPath()).exists())
    MyParams::instance()->saveDefaultParameters();

  qRegisterMetaType<ParamId>("ParamId");

  MainWindow w;

  w.show();

  MyParams::instance()->newScene();
  return a.exec();
}
