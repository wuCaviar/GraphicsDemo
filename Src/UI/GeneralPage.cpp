#include "GeneralPage.h"
#include "AppConfig.h"
#include "colortransform.h"
#include "QtColorWidgets/color_dialog.hpp"

#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

GeneralPage::GeneralPage(QWidget *parent) : PreferencesPage(parent)
{
    setupUI();
}

QString GeneralPage::title() const
{
    return tr("General");
}

void GeneralPage::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    // RIP Path Settings
    auto *ripGroup = new QGroupBox(tr("RIP Path Settings"), this);
    auto *ripLayout = new QFormLayout(ripGroup);

    m_ripExePathEdit = new QLineEdit(ripGroup);
    m_ripExePathEdit->setToolTip(tr("RIP executable file path"));
    auto *browseRipExe = new QPushButton(tr("Browse..."), ripGroup);
    connect(browseRipExe, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Select RIP Executable"),
                                                    m_ripExePathEdit->text(),
                                                    tr("Executables (*.exe);;All Files (*)"));
        if (!path.isEmpty())
            m_ripExePathEdit->setText(path);
    });
    auto *ripExeLayout = new QHBoxLayout;
    ripExeLayout->addWidget(m_ripExePathEdit);
    ripExeLayout->addWidget(browseRipExe);
    ripLayout->addRow(tr("RIP Program:"), ripExeLayout);

    m_ripConfigPathEdit = new QLineEdit(ripGroup);
    m_ripConfigPathEdit->setToolTip(tr("RIP configuration file path"));
    auto *browseRipCfg = new QPushButton(tr("Browse..."), ripGroup);
    connect(browseRipCfg, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Select RIP Configuration File"),
                                                    m_ripConfigPathEdit->text(),
                                                    tr("XML Files (*.xml);;All Files (*)"));
        if (!path.isEmpty())
            m_ripConfigPathEdit->setText(path);
    });
    auto *ripCfgLayout = new QHBoxLayout;
    ripCfgLayout->addWidget(m_ripConfigPathEdit);
    ripCfgLayout->addWidget(browseRipCfg);
    ripLayout->addRow(tr("RIP Config:"), ripCfgLayout);

    layout->addWidget(ripGroup);

    // ICC Path Settings
    auto *iccGroup = new QGroupBox(tr("ICC Color Management"), this);
    auto *iccLayout = new QFormLayout(iccGroup);

    m_iccProfilePathEdit = new QLineEdit(iccGroup);
    m_iccProfilePathEdit->setToolTip(tr("ICC profile base directory"));
    auto *browseIcc = new QPushButton(tr("Browse..."), iccGroup);
    connect(browseIcc, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, tr("Select ICC Profile Directory"),
                                                         m_iccProfilePathEdit->text());
        if (!path.isEmpty())
            m_iccProfilePathEdit->setText(path);
    });
    auto *iccPathLayout = new QHBoxLayout;
    iccPathLayout->addWidget(m_iccProfilePathEdit);
    iccPathLayout->addWidget(browseIcc);
    iccLayout->addRow(tr("ICC Directory:"), iccPathLayout);

    layout->addWidget(iccGroup);
    layout->addStretch();
}

void GeneralPage::load()
{
    const AppConfig &cfg = AppConfig::instance();
    m_ripExePathEdit->setText(cfg.ripExePath());
    m_ripConfigPathEdit->setText(cfg.ripConfigPath());
    m_iccProfilePathEdit->setText(cfg.iccProfileBasePath());
}

bool GeneralPage::save()
{
    AppConfig::instance().setRipExePath(m_ripExePathEdit->text());
    AppConfig::instance().setRipConfigPath(m_ripConfigPathEdit->text());
    AppConfig::instance().setIccProfileBasePath(m_iccProfilePathEdit->text());
    AppConfig::instance().saveConfig();

    QATColorManager &cm = QATColorManager::instance();
    cm.initialize(AppConfig::instance().srgbIccPath(), AppConfig::instance().cmykIccPath());
    color_widgets::ColorDialog::setColorTransform(&cm);

    return true;
}
