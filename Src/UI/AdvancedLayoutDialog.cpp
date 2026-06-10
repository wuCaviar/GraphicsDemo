#include "AdvancedLayoutDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QVBoxLayout>

AdvancedLayoutDialog::AdvancedLayoutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Advanced Settings"));
    setMinimumWidth(480);
    setMinimumHeight(500);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Scroll area ---
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *scrollWidget = new QWidget(scrollArea);
    auto *scrollLayout = new QVBoxLayout(scrollWidget);

    // ========== Group 1: Resolution Settings ==========
    auto *resGroup = new QGroupBox(tr("Resolution Settings"), scrollWidget);
    auto *resForm = new QFormLayout(resGroup);

    m_lessResRatioEdit = new QLineEdit(scrollWidget);
    m_lessResRatioEdit->setAlignment(Qt::AlignCenter);
    m_lessResRatioEdit->setToolTip(
        tr("Default DPI when input image has no DPI info; also affects image scaling"));
    resForm->addRow(tr("Low Resolution Threshold"), m_lessResRatioEdit);

    m_maxResRatioEdit = new QLineEdit(scrollWidget);
    m_maxResRatioEdit->setAlignment(Qt::AlignCenter);
    m_maxResRatioEdit->setToolTip(
        tr("Output DPI cap: output image DPI will never exceed this value\n"
           "E.g. input DPI=300, cap=150 means output DPI is clamped to 150"));
    resForm->addRow(tr("Max DPI Cap"), m_maxResRatioEdit);

    m_otherResRatioEdit = new QLineEdit(scrollWidget);
    m_otherResRatioEdit->setAlignment(Qt::AlignCenter);
    m_otherResRatioEdit->setToolTip(tr("Fixed DPI value used when resolution mode is \"Custom\""));
    resForm->addRow(tr("Custom DPI Value"), m_otherResRatioEdit);

    m_otherResModeCombo = new QComboBox(scrollWidget);
    m_otherResModeCombo->addItems({ tr("Maximum"), tr("Minimum"), tr("Custom") });
    m_otherResModeCombo->setToolTip(tr("Output DPI strategy:\n"
                                       "Maximum — use the highest DPI in this group\n"
                                       "Minimum — use the lowest DPI in this group\n"
                                       "Custom — always use the \"Custom\" DPI value"));
    connect(m_otherResModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { m_otherResRatioEdit->setEnabled(index == 2); });
    resForm->addRow(tr("Resolution Mode"), m_otherResModeCombo);

    scrollLayout->addWidget(resGroup);

    // ========== Group 2: Layout Options ==========
    auto *optGroup = new QGroupBox(tr("Layout Options"), scrollWidget);
    auto *optForm = new QFormLayout(optGroup);

    m_boolSelest1 = new QCheckBox(tr("Append Date to Output Folder"), scrollWidget);
    m_boolSelest1->setToolTip(
        tr("Append a date suffix to the layout output folder name (handled by caller)"));
    optForm->addRow(m_boolSelest1);

    m_boolSelest2 = new QCheckBox(tr("Output Material Saving Report"), scrollWidget);
    m_boolSelest2->setToolTip(tr("Output a material saving statistics text file after layout"));
    optForm->addRow(m_boolSelest2);

    m_boolSelest3 = new QCheckBox(tr("Add Title"), scrollWidget);
    m_boolSelest3->setToolTip(tr("Add a folder name title at the top of the layout result image"));
    optForm->addRow(m_boolSelest3);

    m_boolSelest4 = new QCheckBox(tr("Enable Color Marking"), scrollWidget);
    m_boolSelest4->setToolTip(
        tr("Draw color marker dashed lines for each piece outline during layout\n"
           "Checking this enables Color 1–10 below"));
    connect(m_boolSelest4, &QCheckBox::toggled, this, &AdvancedLayoutDialog::onBoolSelest4Toggled);
    optForm->addRow(m_boolSelest4);

    m_boolSelest5 = new QCheckBox(tr("Alternate Coloring"), scrollWidget);
    m_boolSelest5->setToolTip(tr("Check to color every other piece (alternate mode);\n"
                                 "Unchecked colors every piece"));
    optForm->addRow(m_boolSelest5);

    scrollLayout->addWidget(optGroup);

    // ========== Group 3: Color Marking ==========
    auto *colorGroup = new QGroupBox(tr("Color Marking Settings"), scrollWidget);
    auto *colorForm = new QFormLayout(colorGroup);

    const char *colorLabels[] = { "Color 1", "Color 2", "Color 3", "Color 4", "Color 5",
                                  "Color 6", "Color 7", "Color 8", "Color 9", "Color 10" };
    for (int i = 0; i < 10; ++i) {
        m_colorCombo[i] = new QComboBox(scrollWidget);
        m_colorCombo[i]->setIconSize(QSize(32, 16));
        addColorItems(m_colorCombo[i]);
        colorForm->addRow(tr(colorLabels[i]), m_colorCombo[i]);
    }

    scrollLayout->addWidget(colorGroup);
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // --- Buttons ---
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Load existing settings
    loadSettings();
}

void AdvancedLayoutDialog::loadSettings()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    settings.beginGroup("Setting");
    m_lessResRatioEdit->setText(settings.value("less_resolution_ratio", 100).toString());
    m_maxResRatioEdit->setText(settings.value("max_resolution_ratio", 150).toString());
    m_otherResRatioEdit->setText(settings.value("other_resolution_ratio", 100).toString());
    int otherMode = settings.value("other_resolution_ratio_mode", 0).toInt();
    m_otherResModeCombo->setCurrentIndex(otherMode);
    m_otherResRatioEdit->setEnabled(otherMode == 2);

    m_boolSelest1->setChecked(settings.value("bool_Selest1", false).toBool());
    m_boolSelest2->setChecked(settings.value("bool_Selest2", true).toBool());
    m_boolSelest3->setChecked(settings.value("bool_Selest3", true).toBool());
    m_boolSelest4->setChecked(settings.value("bool_Selest4", false).toBool());
    m_boolSelest5->setChecked(settings.value("bool_Selest5", false).toBool());

    for (int i = 0; i < 10; ++i) {
        int colorIdx = settings.value(QString("Color%1").arg(i + 1), 0).toInt();
        m_colorCombo[i]->setCurrentIndex(colorIdx);
    }
    settings.endGroup();

    // Apply initial enabled state
    onBoolSelest4Toggled(m_boolSelest4->isChecked());
}

void AdvancedLayoutDialog::saveSettings()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    settings.beginGroup("Setting");
    settings.setValue("less_resolution_ratio", m_lessResRatioEdit->text().toInt());
    settings.setValue("max_resolution_ratio", m_maxResRatioEdit->text().toInt());
    settings.setValue("other_resolution_ratio", m_otherResRatioEdit->text().toInt());
    settings.setValue("other_resolution_ratio_mode", m_otherResModeCombo->currentIndex());
    settings.setValue("bool_Selest1", m_boolSelest1->isChecked());
    settings.setValue("bool_Selest2", m_boolSelest2->isChecked());
    settings.setValue("bool_Selest3", m_boolSelest3->isChecked());
    settings.setValue("bool_Selest4", m_boolSelest4->isChecked());
    settings.setValue("bool_Selest5", m_boolSelest5->isChecked());
    for (int i = 0; i < 10; ++i) {
        settings.setValue(QString("Color%1").arg(i + 1), m_colorCombo[i]->currentIndex());
    }
    settings.endGroup();
}

void AdvancedLayoutDialog::onBoolSelest4Toggled(bool checked)
{
    m_boolSelest5->setEnabled(checked);
    for (int i = 0; i < 10; ++i) {
        m_colorCombo[i]->setEnabled(checked);
    }
}

void AdvancedLayoutDialog::addColorItems(QComboBox *comboBox)
{
    struct ColorEntry
    {
        QString name;
        QColor color;
    };
    const QList<ColorEntry> colorList = {
        { tr("Red"), QColor(255, 0, 0) },      { tr("Green"), QColor(85, 255, 0) },
        { tr("Blue"), QColor(0, 85, 255) },    { tr("Yellow"), QColor(255, 255, 0) },
        { tr("Black"), QColor(0, 0, 0) },      { tr("White"), QColor(255, 255, 255) },
        { tr("Pink"), QColor(255, 192, 203) }, { tr("Purple"), QColor(128, 0, 128) },
    };

    for (const auto &entry : colorList) {
        QPixmap pixmap(40, 20);
        pixmap.fill(entry.color);
        comboBox->addItem(QIcon(pixmap), entry.name);
    }
}
