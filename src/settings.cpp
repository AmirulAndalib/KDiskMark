#include "settings.h"
#include "ui_settings.h"

#include <QMetaEnum>

#include "appsettings.h"
#include "global.h"

Settings::Settings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Settings)
{
    ui->setupUi(this);

    ui->buttonBox->addButton(QStringLiteral("NVMe SSD"), QDialogButtonBox::ActionRole);

    populateComboBoxes();

    const AppSettings settings;

    // Load current settings for all profiles and tests
    for (const auto& profile : {Global::PerformanceProfile::Default,
                                Global::PerformanceProfile::Peak,
                                Global::PerformanceProfile::Demo}) {
        const auto tests = (profile == Global::PerformanceProfile::Default)
            ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                             Global::BenchmarkTest::Test_2,
                                             Global::BenchmarkTest::Test_3,
                                             Global::BenchmarkTest::Test_4}
            : (profile == Global::PerformanceProfile::Peak)
                ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                                 Global::BenchmarkTest::Test_2}
                : QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1};

        for (const auto& test : tests) {
            TestWidgets widgets = getTestWidgets(profile, test);
            loadParamsToUI(test, profile, widgets.pattern, widgets.blockSize,
                          widgets.queues, widgets.threads);
        }
    }

    findDataAndSet(ui->MeasuringTime, settings.getMeasuringTime());
    findDataAndSet(ui->IntervalTime, settings.getIntervalTime());
}

Settings::~Settings()
{
    delete ui;
}

void Settings::findDataAndSet(QComboBox *comboBox, int data)
{
    comboBox->setCurrentIndex(comboBox->findData(data));
}

void Settings::on_buttonBox_clicked(QAbstractButton *button)
{
    AppSettings settings;
    const auto standardBtn = ui->buttonBox->standardButton(button);

    if (standardBtn == QDialogButtonBox::Ok) {
        // Save all settings
        for (const auto& profile : {Global::PerformanceProfile::Default,
                                    Global::PerformanceProfile::Peak,
                                    Global::PerformanceProfile::Demo}) {
            const auto tests = (profile == Global::PerformanceProfile::Default)
                ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                                 Global::BenchmarkTest::Test_2,
                                                 Global::BenchmarkTest::Test_3,
                                                 Global::BenchmarkTest::Test_4}
                : (profile == Global::PerformanceProfile::Peak)
                    ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                                     Global::BenchmarkTest::Test_2}
                    : QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1};

            for (const auto& test : tests) {
                TestWidgets widgets = getTestWidgets(profile, test);
                saveParamsFromUI(test, profile, widgets.pattern, widgets.blockSize,
                                widgets.queues, widgets.threads);
            }
        }

        settings.setMeasuringTime(ui->MeasuringTime->currentData().toInt());
        settings.setIntervalTime(ui->IntervalTime->currentData().toInt());

        close();
    }
    else if (standardBtn == QDialogButtonBox::RestoreDefaults) {
        applyPreset(Global::BenchmarkPreset::Standard);
    }
    else {
        // NVMe SSD button
        applyPreset(Global::BenchmarkPreset::NVMe_SSD);
    }
}

void Settings::populateComboBoxes()
{
    // Populate time intervals
    for (int val : { 0, 1, 3, 5, 10, 30, 60, 180, 300, 600 }) {
        ui->IntervalTime->addItem(val < 60 ? QStringLiteral("%1 %2").arg(val).arg(tr("sec"))
                                           : QStringLiteral("%1 %2").arg(val / 60).arg(tr("min")), val);
    }

    for (int val : { 5, 10, 20, 30, 60 }) {
        ui->MeasuringTime->addItem(val < 60 ? QStringLiteral("%1 %2").arg(val).arg(tr("sec"))
                                            : QStringLiteral("%1 %2").arg(val / 60).arg(tr("min")), val);
    }

    // Collect all pattern combo boxes
    QVector<QComboBox*> patternBoxes = {
        ui->DefaultProfile_Test_1_Pattern, ui->DefaultProfile_Test_2_Pattern,
        ui->DefaultProfile_Test_3_Pattern, ui->DefaultProfile_Test_4_Pattern,
        ui->PeakPerformanceProfile_Test_1_Pattern, ui->PeakPerformanceProfile_Test_2_Pattern,
        ui->DemoProfile_Test_1_Pattern
    };

    QVector<QComboBox*> queueBoxes = {
        ui->DefaultProfile_Test_1_Queues, ui->DefaultProfile_Test_2_Queues,
        ui->DefaultProfile_Test_3_Queues, ui->DefaultProfile_Test_4_Queues,
        ui->PeakPerformanceProfile_Test_1_Queues, ui->PeakPerformanceProfile_Test_2_Queues,
        ui->DemoProfile_Test_1_Queues
    };

    QVector<QComboBox*> blockSizeBoxes = {
        ui->DefaultProfile_Test_1_BlockSize, ui->DefaultProfile_Test_2_BlockSize,
        ui->DefaultProfile_Test_3_BlockSize, ui->DefaultProfile_Test_4_BlockSize,
        ui->PeakPerformanceProfile_Test_1_BlockSize, ui->PeakPerformanceProfile_Test_2_BlockSize,
        ui->DemoProfile_Test_1_BlockSize
    };

    QVector<QComboBox*> threadBoxes = {
        ui->DefaultProfile_Test_1_Threads, ui->DefaultProfile_Test_2_Threads,
        ui->DefaultProfile_Test_3_Threads, ui->DefaultProfile_Test_4_Threads,
        ui->PeakPerformanceProfile_Test_1_Threads, ui->PeakPerformanceProfile_Test_2_Threads,
        ui->DemoProfile_Test_1_Threads
    };

    // Populate patterns
    for (const Global::BenchmarkIOPattern &pattern : { Global::BenchmarkIOPattern::SEQ, Global::BenchmarkIOPattern::RND }) {
        QString patternName = QMetaEnum::fromType<Global::BenchmarkIOPattern>().valueToKey(pattern);
        for (auto* box : patternBoxes) {
            box->addItem(patternName, pattern);
        }
    }

    // Populate queues, block sizes, and threads
    for (int i = 1, j = 1; i <= 64; i++, (j <= 8192 ? j *= 2 : j)) {
        QString i_str = QString::number(i);
        QString j_str = QString::number(j);

        // Queues (up to 512)
        if (j <= 512) {
            for (auto* box : queueBoxes) {
                box->addItem(j_str, j);
            }

            // Block sizes (4 KiB to 512 KiB)
            if (j >= 4) {
                for (auto* box : blockSizeBoxes) {
                    box->addItem(QStringLiteral("%1 %2").arg(j).arg(tr("KiB")), j);
                }
            }
        }
        // Block sizes (1 MiB to 8 MiB)
        else if (j <= 8192) {
            for (auto* box : blockSizeBoxes) {
                box->addItem(QStringLiteral("%1 %2").arg(j / 1024).arg(tr("MiB")), j);
            }
        }

        // Threads (1 to 64)
        for (auto* box : threadBoxes) {
            box->addItem(i_str, i);
        }
    }
}

Settings::TestWidgets Settings::getTestWidgets(Global::PerformanceProfile profile, Global::BenchmarkTest test)
{
    TestWidgets widgets{nullptr, nullptr, nullptr, nullptr};

    if (profile == Global::PerformanceProfile::Default) {
        switch (test) {
        case Global::BenchmarkTest::Test_1:
            widgets = {ui->DefaultProfile_Test_1_Pattern, ui->DefaultProfile_Test_1_BlockSize,
                      ui->DefaultProfile_Test_1_Queues, ui->DefaultProfile_Test_1_Threads};
            break;
        case Global::BenchmarkTest::Test_2:
            widgets = {ui->DefaultProfile_Test_2_Pattern, ui->DefaultProfile_Test_2_BlockSize,
                      ui->DefaultProfile_Test_2_Queues, ui->DefaultProfile_Test_2_Threads};
            break;
        case Global::BenchmarkTest::Test_3:
            widgets = {ui->DefaultProfile_Test_3_Pattern, ui->DefaultProfile_Test_3_BlockSize,
                      ui->DefaultProfile_Test_3_Queues, ui->DefaultProfile_Test_3_Threads};
            break;
        case Global::BenchmarkTest::Test_4:
            widgets = {ui->DefaultProfile_Test_4_Pattern, ui->DefaultProfile_Test_4_BlockSize,
                      ui->DefaultProfile_Test_4_Queues, ui->DefaultProfile_Test_4_Threads};
            break;
        }
    }
    else if (profile == Global::PerformanceProfile::Peak) {
        switch (test) {
        case Global::BenchmarkTest::Test_1:
            widgets = {ui->PeakPerformanceProfile_Test_1_Pattern, ui->PeakPerformanceProfile_Test_1_BlockSize,
                      ui->PeakPerformanceProfile_Test_1_Queues, ui->PeakPerformanceProfile_Test_1_Threads};
            break;
        case Global::BenchmarkTest::Test_2:
            widgets = {ui->PeakPerformanceProfile_Test_2_Pattern, ui->PeakPerformanceProfile_Test_2_BlockSize,
                      ui->PeakPerformanceProfile_Test_2_Queues, ui->PeakPerformanceProfile_Test_2_Threads};
            break;
        default:
            break;
        }
    }
    else if (profile == Global::PerformanceProfile::Demo) {
        if (test == Global::BenchmarkTest::Test_1) {
            widgets = {ui->DemoProfile_Test_1_Pattern, ui->DemoProfile_Test_1_BlockSize,
                      ui->DemoProfile_Test_1_Queues, ui->DemoProfile_Test_1_Threads};
        }
    }

    return widgets;
}

void Settings::loadParamsToUI(Global::BenchmarkTest test, Global::PerformanceProfile profile,
                               QComboBox* pattern, QComboBox* blockSize, QComboBox* queues, QComboBox* threads)
{
    const AppSettings settings;
    Global::BenchmarkParams params = settings.getBenchmarkParams(test, profile);

    findDataAndSet(pattern, params.Pattern);
    findDataAndSet(blockSize, params.BlockSize);
    findDataAndSet(queues, params.Queues);
    findDataAndSet(threads, params.Threads);
}

void Settings::saveParamsFromUI(Global::BenchmarkTest test, Global::PerformanceProfile profile,
                                 QComboBox* pattern, QComboBox* blockSize, QComboBox* queues, QComboBox* threads)
{
    AppSettings settings;
    settings.setBenchmarkParams(test, profile, {
        static_cast<Global::BenchmarkIOPattern>(pattern->currentData().toInt()),
        blockSize->currentData().toInt(),
        queues->currentData().toInt(),
        threads->currentData().toInt()
    });
}

void Settings::applyPreset(Global::BenchmarkPreset preset)
{
    AppSettings settings;

    for (const auto& profile : {Global::PerformanceProfile::Default,
                                Global::PerformanceProfile::Peak,
                                Global::PerformanceProfile::Demo}) {
        const auto tests = (profile == Global::PerformanceProfile::Default)
            ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                             Global::BenchmarkTest::Test_2,
                                             Global::BenchmarkTest::Test_3,
                                             Global::BenchmarkTest::Test_4}
            : (profile == Global::PerformanceProfile::Peak)
                ? QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1,
                                                 Global::BenchmarkTest::Test_2}
                : QVector<Global::BenchmarkTest>{Global::BenchmarkTest::Test_1};

        for (const auto& test : tests) {
            Global::BenchmarkParams params = settings.defaultBenchmarkParams(test, profile, preset);
            TestWidgets widgets = getTestWidgets(profile, test);

            findDataAndSet(widgets.pattern, params.Pattern);
            findDataAndSet(widgets.blockSize, params.BlockSize);
            findDataAndSet(widgets.queues, params.Queues);
            findDataAndSet(widgets.threads, params.Threads);
        }
    }

    findDataAndSet(ui->MeasuringTime, settings.defaultMeasuringTime());
    findDataAndSet(ui->IntervalTime, settings.defaultIntervalTime());
}
