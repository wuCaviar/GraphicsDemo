#include "GeneralPage.h"
#include "AppConfig.h"

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
    return tr("常规");
}

void GeneralPage::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    // RIP 路径设置
    auto *ripGroup = new QGroupBox(tr("RIP 路径设置"), this);
    auto *ripLayout = new QFormLayout(ripGroup);

    m_ripExePathEdit = new QLineEdit(ripGroup);
    m_ripExePathEdit->setToolTip(tr("RIP executable file path"));
    auto *browseRipExe = new QPushButton(tr("浏览..."), ripGroup);
    connect(browseRipExe, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("选择 RIP 可执行文件"), m_ripExePathEdit->text(),
            tr("可执行文件 (*.exe);;所有文件 (*)"));
        if (!path.isEmpty())
            m_ripExePathEdit->setText(path);
    });
    auto *ripExeLayout = new QHBoxLayout;
    ripExeLayout->addWidget(m_ripExePathEdit);
    ripExeLayout->addWidget(browseRipExe);
    ripLayout->addRow(tr("RIP 程序:"), ripExeLayout);

    m_ripConfigPathEdit = new QLineEdit(ripGroup);
    m_ripConfigPathEdit->setToolTip(tr("RIP configuration file path"));
    auto *browseRipCfg = new QPushButton(tr("浏览..."), ripGroup);
    connect(browseRipCfg, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("选择 RIP 配置文件"), m_ripConfigPathEdit->text(),
            tr("XML 文件 (*.xml);;所有文件 (*)"));
        if (!path.isEmpty())
            m_ripConfigPathEdit->setText(path);
    });
    auto *ripCfgLayout = new QHBoxLayout;
    ripCfgLayout->addWidget(m_ripConfigPathEdit);
    ripCfgLayout->addWidget(browseRipCfg);
    ripLayout->addRow(tr("RIP 配置:"), ripCfgLayout);

    layout->addWidget(ripGroup);

    // ICC 路径设置
    auto *iccGroup = new QGroupBox(tr("ICC 色彩管理"), this);
    auto *iccLayout = new QFormLayout(iccGroup);

    m_iccProfilePathEdit = new QLineEdit(iccGroup);
    m_iccProfilePathEdit->setToolTip(tr("ICC profile base directory"));
    auto *browseIcc = new QPushButton(tr("浏览..."), iccGroup);
    connect(browseIcc, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(
            this, tr("选择 ICC 配置文件目录"), m_iccProfilePathEdit->text());
        if (!path.isEmpty())
            m_iccProfilePathEdit->setText(path);
    });
    auto *iccPathLayout = new QHBoxLayout;
    iccPathLayout->addWidget(m_iccProfilePathEdit);
    iccPathLayout->addWidget(browseIcc);
    iccLayout->addRow(tr("ICC 目录:"), iccPathLayout);

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
    return true;
}
