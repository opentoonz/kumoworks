#include "cloudpresetcontrol.h"

#include "myparams.h"
#include "cloud.h"
#include "undomanager.h"
#include "mycontrol.h"
#include "pathutils.h"

#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QDir>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>
#include <QUndoCommand>

using namespace PathUtils;

CloudPresetControl::CloudPresetControl(QWidget* parent) : QWidget(parent) {
  m_presetListCombo          = new QComboBox(this);
  PlusMinusButton* addButton = new PlusMinusButton(true, this);
  m_removeButton             = new PlusMinusButton(false, this);

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setMargin(5);
  lay->setSpacing(5);
  {
    lay->addWidget(new QLabel(tr("Preset:"), this), 0);
    lay->addWidget(m_presetListCombo, 1);
    lay->addWidget(addButton, 0);
    lay->addWidget(m_removeButton, 0);
  }
  setLayout(lay);

  connect(m_presetListCombo, SIGNAL(activated(const QString&)), this,
          SLOT(onPresetSelected(const QString&)));
  connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  connect(m_removeButton, SIGNAL(clicked()), this,
          SLOT(onRemoveButtonClicked()));

  refreshPresetListCombo();
}

// load preset file and refresh combo box items
void CloudPresetControl::refreshPresetListCombo() {
  // preset file path. Presetファイルのパス
  QString settingsFp = getCloudPresetPath();
  // load preset file. Presetファイルを読み込む
  QSettings cloudPreset(settingsFp, QSettings::IniFormat);

  // clear items. コンボボックスの項目クリア
  m_presetListCombo->clear();

  // make the child groups to be the combo box items
  // ChildGroupリストをそのままコンボボックスの項目にする
  m_presetListCombo->addItems(cloudPreset.childGroups());

  // disable the remove button if the list is empty
  // リストが空なら、RemoveボタンをDisable。そうじゃなきゃEnable
  m_removeButton->setEnabled(m_presetListCombo->count() != 0);
}

void CloudPresetControl::onAddButtonClicked() {
  // make default preset name. プリセットのデフォルト名候補
  int index = 1;
  QString defaultName;
  while (1) {
    defaultName = QString("My Cloud ") + QString::number(index);
    if (m_presetListCombo->findText(defaultName, Qt::MatchExactly) == -1) break;
    index++;
  }

  // Open dialog for getting preset name. 名前指定ダイアログを開く
  bool ok;
  QString presetName = QInputDialog::getText(
      this, tr("Please specify preset name."), tr("Preset name:"),
      QLineEdit::Normal, defaultName, &ok);

  // キャンセルされたらreturn
  if (!ok || presetName.isEmpty()) return;

  if (m_presetListCombo->findText(presetName, Qt::MatchExactly) != -1) {
    QMessageBox::StandardButton ret = QMessageBox::question(
        this, tr("Question"),
        tr("Preset %1 is already registered.\nDo you want to overwrite the "
           "preset with the current parameters?")
            .arg(presetName));

    // let user to specify the preset name again
    if (ret != QMessageBox::Yes) {
      onAddButtonClicked();
      return;
    }
  }

  // if the preset is exist, overwrite it
  // 名前がかぶっていたら前のPresetに上書きして現在の設定を保存
  {
    // preset file path. Presetファイルのパス
    QString settingsFp = getCloudPresetPath();
    // load preset. Presetファイルを読み込む
    QSettings cloudPreset(settingsFp, QSettings::IniFormat);

    cloudPreset.beginGroup(presetName);
    MyParams::instance()->getCurrentCloud()->saveData(cloudPreset, true);
    cloudPreset.endGroup();
  }

  // refresh combo box. コンボボックスを更新
  refreshPresetListCombo();

  // Set the current index to the loaded one.
  // 現在のアイテムのインデックスを保存したばかりのアイテムにセット
  m_presetListCombo->setCurrentText(presetName);
}

void CloudPresetControl::onRemoveButtonClicked() {
  QString presetName = m_presetListCombo->currentText();

  // Open a confirmation dialog. 確認ダイアログを開く
  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Question"),
      tr("Are you sure you want to remove the cloud preset \"%1\" ?")
          .arg(presetName));

  // return if canceled. キャンセルされたらreturn
  if (ret != QMessageBox::Yes) return;

  // remove the current item from the preset. Presetから現在の項目を削除
  {
    // preset file path. Presetファイルのパス
    QString settingsFp = getCloudPresetPath();
    // load preset. Presetファイルを読み込む
    QSettings cloudPreset(settingsFp, QSettings::IniFormat);

    cloudPreset.remove(presetName);
  }

  // refresh combo box. コンボボックスを更新
  refreshPresetListCombo();
}

void CloudPresetControl::onPresetSelected(const QString& presetName) {
  // preset file path. Presetファイルのパス
  QString settingsFp = getCloudPresetPath();
  // load preset. Presetファイルを読み込む
  QSettings cloudPreset(settingsFp, QSettings::IniFormat);

  if (!cloudPreset.childGroups().contains(presetName, Qt::CaseInsensitive)) {
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Failed to load the cloud preset \"%1\". Refleshing the list.")
            .arg(presetName));

    refreshPresetListCombo();
    return;
  }

  MyParams* p  = MyParams::instance();
  Cloud* cloud = p->getCurrentCloud();

  // Create undo. Undo作成、現在の雲のパラメータ保持
  QUndoCommand* undo = new QUndoCommand();

  cloudPreset.beginGroup(presetName);

  // set parameters the preset values. パラメータ置き換え
  for (QString& key : cloudPreset.childKeys()) {
    ParamId pId = p->getParamIdFromIdString(key);
    if (pId == InvalidParam) continue;
    if (p->isCloudParam((ParamId)pId)) {
      ParameterUndo* pUndo =
          new ParameterUndo(cloud, pId, p->getParam(pId).value, undo);
      p->getParam(pId).value = cloudPreset.value(key);
      p->notifyParamChanged(pId);
      pUndo->setAfterVal(cloud->getParam(pId));
    }
  }

  cloudPreset.endGroup();

  // register undo. Undo登録
  UndoManager::instance()->stack()->push(undo);
}