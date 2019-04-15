#include "mytoolbar.h"

#define _USE_MATH_DEFINES
#include "cloud.h"
#include "renderpopup.h"
#include "progressview.h"
#include "pathutils.h"
#include "aboutpopup.h"

#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QApplication>
#include <QStyle>
#include <iostream>
#include <QMessageBox>
#include <QLineEdit>
#include <QIntValidator>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMenu>
#include <QMenuBar>
#include <QToolButton>
#include <QDir>
#include <QSettings>

using namespace PathUtils;

MyToolBar::MyToolBar(QWidget* parent) : QToolBar(tr("Tool bar"), parent) {
  setFocusPolicy(Qt::NoFocus);

  QToolButton* menuBtn = new QToolButton(this);
  QMenu* fileMenu      = new QMenu();
  QAction* newScene    = new QAction(tr("New Scene"));
  QAction* loadScene   = new QAction(tr("Load Scene"));
  QAction* saveScene   = new QAction(tr("Save Scene"));
  QAction* saveDefault = new QAction(tr("Save Default Settings"));

  QToolButton* helpBtn = new QToolButton(this);
  QMenu* helpMenu      = new QMenu();
  QAction* about       = new QAction(tr("About %1").arg(qApp->applicationName()));

  QToolButton* renderBtn   = new QToolButton(this);
  QPushButton* drawModeBtn = new QPushButton(tr("Draw"), this);
  QPushButton* prevModeBtn = new QPushButton(tr("Preview"), this);

  m_widthEdit           = new QLineEdit(this);
  m_heightEdit          = new QLineEdit(this);
  m_previewQualityCombo = new QComboBox(this);

  ProgressView* progressView = new ProgressView(this);

  QMenu* languageSubMenu = new QMenu(tr("Language"));
  createLanguageActions(languageSubMenu);

  menuBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  menuBtn->setText(tr("File"));
  menuBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
  menuBtn->setPopupMode(QToolButton::MenuButtonPopup);
  menuBtn->setPopupMode(QToolButton::InstantPopup);
  newScene->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
  loadScene->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));
  saveScene->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
  saveDefault->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
  renderBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  renderBtn->setText(tr("Render"));
  renderBtn->setIcon(QIcon(":Resources/render.svg"));

  drawModeBtn->setIcon(QIcon(":Resources/drawmode.svg"));
  prevModeBtn->setIcon(QIcon(":Resources/prevmode.svg"));

  drawModeBtn->setCheckable(true);
  prevModeBtn->setCheckable(true);

  m_modeButtonGroup = new QButtonGroup(this);
  m_modeButtonGroup->addButton(drawModeBtn, (int)DrawMode);
  m_modeButtonGroup->addButton(prevModeBtn, (int)PreviewMode);
  m_modeButtonGroup->setExclusive(true);
  drawModeBtn->setChecked(true);

  m_widthEdit->setFixedWidth(50);
  m_heightEdit->setFixedWidth(50);
  m_widthEdit->setValidator(new QIntValidator(100, 10000, this));
  m_heightEdit->setValidator(new QIntValidator(100, 10000, this));
  QSize sceneSize = MyParams::instance()->getSceneSize();
  m_widthEdit->setText(QString::number(sceneSize.width()));
  m_heightEdit->setText(QString::number(sceneSize.height()));

  QLabel* prevQualLabel =
      new QLabel(MyParams::instance()->getParam(ToolbarVoxelSkip).name, this);
  m_previewQualityCombo->addItem(tr("Full"), 1);
  m_previewQualityCombo->addItem(tr("1/2"), 2);
  m_previewQualityCombo->addItem(tr("1/3"), 3);
  m_previewQualityCombo->addItem(tr("1/4"), 4);
  m_previewQualityCombo->setCurrentIndex(
      m_previewQualityCombo->findData(MyParams::instance()->voxelSkip()));

  m_autoPrevBtn = new QPushButton(tr("Auto"), this);
  m_autoPrevBtn->setCheckable(true);
  m_autoPrevBtn->setChecked(MyParams::instance()->autoPreview());
  m_autoPrevBtn->setEnabled(MyParams::instance()->voxelSkip() >= 3);

  helpBtn->setText(tr("Help"));
  helpBtn->setIcon(QIcon(":Resources/question.svg"));
  helpBtn->setPopupMode(QToolButton::MenuButtonPopup);
  helpBtn->setPopupMode(QToolButton::InstantPopup);

  fileMenu->addAction(newScene);
  fileMenu->addAction(loadScene);
  fileMenu->addSeparator();
  fileMenu->addAction(saveScene);
  fileMenu->addSeparator();
  fileMenu->addAction(saveDefault);
  fileMenu->addMenu(languageSubMenu);
  menuBtn->setMenu(fileMenu);
  addWidget(menuBtn);

  addSeparator();
  addWidget(renderBtn);

  addSeparator();
  addWidget(new QLabel(tr("Mode:"), this));
  addWidget(drawModeBtn);
  addWidget(prevModeBtn);

  QWidget* widget  = new QWidget(this);
  QHBoxLayout* lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(3);
  {
    lay->addStretch(1);
    lay->addWidget(progressView);
    lay->addStretch(1);
    lay->addWidget(new QLabel(tr("Scene size:"), this), 0);
    lay->addWidget(m_widthEdit, 0);
    lay->addWidget(new QLabel("x", this), 0);
    lay->addWidget(m_heightEdit, 0);
  }
  widget->setLayout(lay);
  addWidget(widget);

  addSeparator();
  addWidget(prevQualLabel);
  addWidget(m_previewQualityCombo);
  addWidget(m_autoPrevBtn);

  addSeparator();
  helpMenu->addAction(about);
  helpBtn->setMenu(helpMenu);
  addWidget(helpBtn);

  bool ret = true;
  ret      = ret && connect(m_modeButtonGroup, SIGNAL(buttonClicked(int)), this,
                       SLOT(onModeButtonClicked(int)));
  ret = ret && connect(MyParams::instance(), SIGNAL(modeChanged(int)), this,
                       SLOT(onModeChanged(int)));

  ret = ret && connect(newScene, &QAction::triggered,
                       []() { MyParams::instance()->newScene(); });
  ret = ret && connect(loadScene, &QAction::triggered,
                       []() { MyParams::instance()->load(); });
  ret = ret && connect(saveScene, &QAction::triggered,
                       []() { MyParams::instance()->save(); });
  ret = ret && connect(saveDefault, &QAction::triggered, [&]() {
          QMessageBox::StandardButton btn = QMessageBox::question(
              this, tr("Question"),
              tr("Are you sure you want to save the Default Settings?"));
          if (btn == QMessageBox::Yes)
            MyParams::instance()->saveDefaultParameters();
        });

  ret = ret && connect(renderBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onRenderButtonClicked()));

  ret = ret && connect(m_widthEdit, SIGNAL(editingFinished()), this,
                       SLOT(onSceneSizeEdited()));
  ret = ret && connect(m_heightEdit, SIGNAL(editingFinished()), this,
                       SLOT(onSceneSizeEdited()));
  ret = ret && connect(MyParams::instance(), SIGNAL(sceneFilePathChanged()),
                       this, SLOT(syncSceneSizeField()));

  ret = ret && connect(m_previewQualityCombo, SIGNAL(activated(int)), this,
                       SLOT(onPreviewQualityChanged()));
  // sync UI when parameter is externally changed.
  // 外部からパラメータが変化したとき、UIを同期する
  ret = ret && connect(MyParams::instance(), SIGNAL(paramChanged(ParamId)),
                       this, SLOT(onParameterChanged(ParamId)));

  ret = ret && connect(m_autoPrevBtn, SIGNAL(clicked(bool)), this,
                       SLOT(onAutoPrevButtonClicked(bool)));

  ret = ret &&
        connect(about, SIGNAL(triggered()), this, SLOT(onAboutTriggered()));

  assert(ret);
}

void MyToolBar::onModeButtonClicked(int id) {
  MyParams::instance()->setMode((Mode)id);
}

void MyToolBar::onModeChanged(int mode) {
  m_modeButtonGroup->button(mode)->setChecked(true);
}

void MyToolBar::onRenderButtonClicked() {
  if (MyParams::instance()->getClouds().size() == 1 &&
      MyParams::instance()->getCurrentCloud()->getBBox().isEmpty()) {
    QMessageBox::warning(this, tr("Warning"), tr("No cloud layer to render!"));
    return;
  }

  RenderPopup renderPopup;
  renderPopup.exec();
}

void MyToolBar::onSceneSizeEdited() {
  int width  = m_widthEdit->text().toInt();
  int height = m_heightEdit->text().toInt();

  MyParams::instance()->setSceneSize(QSize(width, height));
}

void MyToolBar::syncSceneSizeField() {
  QSize sceneSize = MyParams::instance()->getSceneSize();
  m_widthEdit->setText(QString::number(sceneSize.width()));
  m_heightEdit->setText(QString::number(sceneSize.height()));
}

void MyToolBar::onPreviewQualityChanged() {
  int skip = m_previewQualityCombo->currentData().toInt();
  MyParams::instance()->getParam(ToolbarVoxelSkip).value = skip;
  MyParams::instance()->notifyParamChanged(ToolbarVoxelSkip);
}

void MyToolBar::onParameterChanged(ParamId pId) {
  if (pId != ToolbarVoxelSkip) return;
  int skip = MyParams::instance()->voxelSkip();
  m_previewQualityCombo->setCurrentIndex(m_previewQualityCombo->findData(skip));
  m_autoPrevBtn->setEnabled(skip >= 3);
}

void MyToolBar::onAutoPrevButtonClicked(bool on) {
  MyParams::instance()->setAutoPreview(on);
}

void MyToolBar::createLanguageActions(QMenu* parentMenu) {
  QString currentLang("en");
  if (QFile::exists(getDefaultSettingsPath())) {
    QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);

    settings.beginGroup("Preferences");
    currentLang = settings.value("language", currentLang).toString();
    settings.endGroup();
  }

  m_languagesGroup = new QActionGroup(this);
  m_languagesGroup->setExclusive(true);
  QAction* english = new QAction(QString("English"), this);
  english->setData(QString("en"));
  english->setCheckable(true);

  m_languagesGroup->addAction(english);
  parentMenu->addAction(english);

  english->setChecked(true);

  QStringList filters;
  filters << "*.qm";
  QStringList entries = QDir(translationDirPath())
                            .entryList(filters, QDir::Files | QDir::Readable);

  for (QString& entry : entries) {
    if (!entry.startsWith("KumoWorks_")) continue;
    QString langName = entry;
    langName.chop(3);  // remove extension
    langName.replace("KumoWorks_", "");

    QAction* action =
        new QAction(QString(QLocale(langName).nativeLanguageName()), this);
    action->setData(langName);
    action->setCheckable(true);

    m_languagesGroup->addAction(action);
    parentMenu->addAction(action);

    if (langName == currentLang) action->setChecked(true);
  }

  connect(m_languagesGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onLanguageTriggered(QAction*)));
}

void MyToolBar::onLanguageTriggered(QAction* langAction) {
  QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("Preferences");
  QString oldLang = settings.value("language").toString();
  settings.setValue("language", langAction->data());
  settings.endGroup();

  // open warning popup
  if (oldLang != langAction->data().toString())
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Language will be changed after restarting %1.").arg(qApp->applicationName()));
}

void MyToolBar::onAboutTriggered() {
  AboutPopup aboutPopup(this);
  aboutPopup.exec();
}