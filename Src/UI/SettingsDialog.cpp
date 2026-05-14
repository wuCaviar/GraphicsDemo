#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#if defined(Q_OS_MACOS)
static const QString kConfigFile =
    QStringLiteral("/Volumes/Caviar/Test/GraphicsDemo/Bin/ripconfig.xml");
#elif defined(Q_OS_WIN)
static const QString kConfigFile = QStringLiteral("ripconfig.xml");
#endif

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setupUI();
    loadConfig();
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("设置"));
    setMinimumWidth(400);

    auto *mainLayout = new QVBoxLayout(this);

    // ---- 分辨率 ----
    auto *resGroup = new QGroupBox(tr("分辨率"));
    auto *resLayout = new QHBoxLayout(resGroup);

    resLayout->addWidget(new QLabel(tr("X:")));
    m_resolutionXSpin = new QSpinBox;
    m_resolutionXSpin->setRange(1, 99999);
    m_resolutionXSpin->setSuffix(tr(" dpi"));
    m_resolutionXSpin->setValue(300);
    resLayout->addWidget(m_resolutionXSpin);

    resLayout->addWidget(new QLabel(tr("Y:")));
    m_resolutionYSpin = new QSpinBox;
    m_resolutionYSpin->setRange(1, 99999);
    m_resolutionYSpin->setSuffix(tr(" dpi"));
    m_resolutionYSpin->setValue(300);
    resLayout->addWidget(m_resolutionYSpin);

    mainLayout->addWidget(resGroup);

    // ---- 曲线设置 ----
    auto *curveGroup = new QGroupBox(tr("曲线设置"));
    auto *curveLayout = new QFormLayout(curveGroup);

    auto *dotCurveRow = new QHBoxLayout;
    m_dotCurveEdit = new QLineEdit;
    m_dotCurveEdit->setPlaceholderText(tr("网点曲线文件路径"));
    dotCurveRow->addWidget(m_dotCurveEdit);
    auto *dotCurveBtn = new QPushButton(tr("选择文件..."));
    dotCurveRow->addWidget(dotCurveBtn);
    curveLayout->addRow(tr("网点曲线:"), dotCurveRow);

    auto *colorCurveRow = new QHBoxLayout;
    m_colorCurveEdit = new QLineEdit;
    m_colorCurveEdit->setPlaceholderText(tr("色彩曲线文件路径"));
    colorCurveRow->addWidget(m_colorCurveEdit);
    auto *colorCurveBtn = new QPushButton(tr("选择文件..."));
    colorCurveRow->addWidget(colorCurveBtn);
    curveLayout->addRow(tr("色彩曲线:"), colorCurveRow);

    mainLayout->addWidget(curveGroup);

    // ---- 输出信息 ----
    auto *outputGroup = new QGroupBox(tr("输出信息"));
    auto *outputLayout = new QFormLayout(outputGroup);

    auto *outputRow = new QHBoxLayout;
    m_outputPathEdit = new QLineEdit;
    m_outputPathEdit->setPlaceholderText(tr("输出路径"));
    outputRow->addWidget(m_outputPathEdit);
    auto *outputBtn = new QPushButton(tr("选择路径..."));
    outputRow->addWidget(outputBtn);
    outputLayout->addRow(tr("输出路径:"), outputRow);

    mainLayout->addWidget(outputGroup);

    // ---- 按钮 ----
    auto *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(dotCurveBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("选择网点曲线文件"), {}, tr("曲线文件 (*.p)"));
        if (!path.isEmpty())
            m_dotCurveEdit->setText(path);
    });
    connect(colorCurveBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("选择色彩曲线文件"), {}, tr("色彩曲线文件 (*.icm)"));
        if (!path.isEmpty())
            m_colorCurveEdit->setText(path);
    });
    connect(outputBtn, &QPushButton::clicked, this, [this]() {
        QString path =
            QFileDialog::getExistingDirectory(this, tr("选择输出路径"));
        if (!path.isEmpty())
            m_outputPathEdit->setText(path);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveConfig();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

int SettingsDialog::resolutionX() const
{
    return m_resolutionXSpin->value();
}

int SettingsDialog::resolutionY() const
{
    return m_resolutionYSpin->value();
}

QString SettingsDialog::dotCurvePath() const
{
    return m_dotCurveEdit->text();
}

QString SettingsDialog::colorCurvePath() const
{
    return m_colorCurveEdit->text();
}

QString SettingsDialog::outputPath() const
{
    return m_outputPathEdit->text();
}

void SettingsDialog::loadConfig()
{
    QFile file(kConfigFile);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDomDocument doc;
    if (!doc.setContent(&file))
        return;
    file.close();

    QDomElement root = doc.documentElement(); // <RIPInfo>
    if (root.tagName() != QLatin1String("RIPInfo"))
        return;

    // <DPI X="..." Y="..."/>
    QDomElement dpiEl = root.firstChildElement(QLatin1String("DPI"));
    if (!dpiEl.isNull()) {
        if (dpiEl.hasAttribute(QLatin1String("X")))
            m_resolutionXSpin->setValue(
                dpiEl.attribute(QLatin1String("X")).toInt());
        if (dpiEl.hasAttribute(QLatin1String("Y")))
            m_resolutionYSpin->setValue(
                dpiEl.attribute(QLatin1String("Y")).toInt());
    }

    // <ICC ProofFile="..." ICCFile="..."/>
    QDomElement iccEl = root.firstChildElement(QLatin1String("ICC"));
    if (!iccEl.isNull()) {
        m_dotCurveEdit->setText(iccEl.attribute(QLatin1String("ProofFile")));
        m_colorCurveEdit->setText(iccEl.attribute(QLatin1String("ICCFile")));
    }

    // <OutInfo OutPath="..."/>
    QDomElement outEl = root.firstChildElement(QLatin1String("OutInfo"));
    if (!outEl.isNull())
        m_outputPathEdit->setText(outEl.attribute(QLatin1String("OutPath")));
}

void SettingsDialog::saveConfig()
{
    QFile file(kConfigFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction(
        QStringLiteral("xml"),
        QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
    QDomElement root = doc.createElement(QStringLiteral("RIPInfo"));
    doc.appendChild(root);

    // <DPI X="..." Y="..."/>
    QDomElement dpiEl = doc.createElement(QStringLiteral("DPI"));
    dpiEl.setAttribute(QStringLiteral("X"), m_resolutionXSpin->value());
    dpiEl.setAttribute(QStringLiteral("Y"), m_resolutionYSpin->value());
    root.appendChild(dpiEl);

    // <ICC ProofFile="..." ICCFile="..."/>
    QDomElement iccEl = doc.createElement(QStringLiteral("ICC"));
    iccEl.setAttribute(QStringLiteral("ProofFile"), m_dotCurveEdit->text());
    iccEl.setAttribute(QStringLiteral("ICCFile"), m_colorCurveEdit->text());
    root.appendChild(iccEl);

    // <OutInfo OutPath="..."/>
    QDomElement outEl = doc.createElement(QStringLiteral("OutInfo"));
    outEl.setAttribute(QStringLiteral("OutPath"), m_outputPathEdit->text());
    root.appendChild(outEl);

    QTextStream ts(&file);
    doc.save(ts, 4);
    file.close();
}
