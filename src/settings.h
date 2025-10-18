#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include "global.h"

class AppSettings;
class QAbstractButton;
class QComboBox;

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = nullptr);
    ~Settings();

private slots:
    void on_buttonBox_clicked(QAbstractButton *button);

private:
    Ui::Settings *ui;

    void findDataAndSet(QComboBox* comboBox, int data);
    void populateComboBoxes();
    void loadParamsToUI(Global::BenchmarkTest test, Global::PerformanceProfile profile,
                        QComboBox* pattern, QComboBox* blockSize, QComboBox* queues, QComboBox* threads);
    void saveParamsFromUI(Global::BenchmarkTest test, Global::PerformanceProfile profile,
                          QComboBox* pattern, QComboBox* blockSize, QComboBox* queues, QComboBox* threads);
    void applyPreset(Global::BenchmarkPreset preset);

    // UI element groups for each profile/test combination
    struct TestWidgets {
        QComboBox* pattern;
        QComboBox* blockSize;
        QComboBox* queues;
        QComboBox* threads;
    };

    TestWidgets getTestWidgets(Global::PerformanceProfile profile, Global::BenchmarkTest test);
};

#endif // SETTINGS_H
