#include "PreferencesDialog.h"

#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setMinimumWidth(400);
    setupUI();
}

void PreferencesDialog::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    // Reserved for future preferences
}
