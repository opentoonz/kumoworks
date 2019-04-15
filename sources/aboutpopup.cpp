#include "aboutpopup.h"

#include "pathutils.h"
#include <QIcon>
#include <QPainter>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QApplication>

using namespace PathUtils;

AboutPopup::AboutPopup(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  setFixedSize(458, 500);
  setWindowTitle(tr("About %1").arg(qApp->applicationName()));

  QString softwareStr =
      qApp->applicationName() + "  Version " + qApp->applicationVersion();
  QString dayTimeStr = tr("Built %1").arg(__DATE__);
  QString copyrightStr =
      "All contributions by DWANGO:\n"
      "Copyright(c) DWANGO Co., Ltd.  All rights reserved.\n\n"
      "All other contributions : \n"
      "Copyright(c) the respective contributors.  All rights reserved.\n\n"
      "Each contributor holds copyright over\n their respective "
      "contributions.\n"
      "The project versioning(Git) records all\n such contribution source "
      "information.";
  QLabel* copyrightLabel = new QLabel(copyrightStr, this);
  copyrightLabel->setAlignment(Qt::AlignCenter);

  QTextEdit* licenseField = new QTextEdit(this);
  licenseField->setLineWrapMode(QTextEdit::NoWrap);
  licenseField->setReadOnly(true);
  QStringList licenseFiles;
  licenseFiles << "LICENSE_ARHOSEK_SKYMODEL.txt"
               << "LICENSE_Qt.txt";

  for (const QString& fileName : licenseFiles) {
    QString licenseTxt;
    licenseTxt.append("[ " + fileName + " ]\n\n");

    QFile file(licenseDirPath() + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

    QTextStream in(&file);
    while (!in.atEnd()) {
      QString line = in.readLine();
      licenseTxt.append(line);
      licenseTxt.append("\n");
    }

    licenseTxt.append("\n-------------------------\n");

    licenseField->append(licenseTxt);
  }
  licenseField->moveCursor(QTextCursor::Start);

  QVBoxLayout* lay = new QVBoxLayout();
  lay->setMargin(10);
  lay->setSpacing(3);
  {
    lay->addSpacing(138);

    lay->addWidget(new QLabel(softwareStr, this), 0, Qt::AlignCenter);
    lay->addWidget(new QLabel(dayTimeStr, this), 0, Qt::AlignCenter);

    lay->addSpacing(10);

    lay->addWidget(copyrightLabel, 0, Qt::AlignCenter);

    lay->addSpacing(10);

    lay->addWidget(new QLabel(tr("Licenses"), this), 0, Qt::AlignLeft);
    lay->addWidget(licenseField, 1);
  }
  setLayout(lay);
}

void AboutPopup::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.drawPixmap(100, 50, QIcon(":Resources/kumoworks_logo.svg").pixmap(258, 38));
}
