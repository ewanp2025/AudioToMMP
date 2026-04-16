#include "housevocalstabs.h"
#include <QApplication>
#include <QClipboard>
#include <cmath>

HouseVocalStabsTab::HouseVocalStabsTab(QWidget *parent) : QWidget(parent) {
    setupUI();

    connect(pitchSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(durationSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(glottalScoopSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(vibratoRateSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(vibratoDepthSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(breathSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(brightnessSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);
    connect(shimmerSlider, &QSlider::valueChanged, this, &HouseVocalStabsTab::updateLabels);

    connect(generateButton, &QPushButton::clicked, this, &HouseVocalStabsTab::generateStab);

    updateLabels();
    generateStab();
}

void HouseVocalStabsTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *controlsGroup = new QGroupBox("Advanced Vocal Stab Engine");
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsGroup);


    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("Stab Style:"));
    stabPresetCombo = new QComboBox();
    stabPresetCombo->addItems({"Vocal Ahh (Classic)", "Soul Ooh", "Yeah! Stab", "Woo Baby", "Work It!", "Feel The Groove", "Big Fun"});
    presetLayout->addWidget(stabPresetCombo);
    controlsLayout->addLayout(presetLayout);


    QHBoxLayout *pitchLayout = new QHBoxLayout();
    pitchLabel = new QLabel("Base Pitch:");
    pitchSlider = new QSlider(Qt::Horizontal); pitchSlider->setRange(60, 84); pitchSlider->setValue(72);
    pitchLayout->addWidget(pitchLabel); pitchLayout->addWidget(pitchSlider);
    controlsLayout->addLayout(pitchLayout);

    QHBoxLayout *scoopLayout = new QHBoxLayout();
    glottalLabel = new QLabel("Glottal Click Strength:");
    glottalScoopSlider = new QSlider(Qt::Horizontal); glottalScoopSlider->setRange(0, 100); glottalScoopSlider->setValue(85);
    scoopLayout->addWidget(glottalLabel); scoopLayout->addWidget(glottalScoopSlider);
    controlsLayout->addLayout(scoopLayout);


    QHBoxLayout *durLayout = new QHBoxLayout();
    durationLabel = new QLabel("Stab Length:");
    durationSlider = new QSlider(Qt::Horizontal); durationSlider->setRange(20, 120); durationSlider->setValue(48);
    durLayout->addWidget(durationLabel); durLayout->addWidget(durationSlider);
    controlsLayout->addLayout(durLayout);


    QHBoxLayout *vRateLayout = new QHBoxLayout();
    vibratoRateLabel = new QLabel("Vibrato Rate:");
    vibratoRateSlider = new QSlider(Qt::Horizontal); vibratoRateSlider->setRange(30, 90); vibratoRateSlider->setValue(62);
    vRateLayout->addWidget(vibratoRateLabel); vRateLayout->addWidget(vibratoRateSlider);
    controlsLayout->addLayout(vRateLayout);

    QHBoxLayout *vDepthLayout = new QHBoxLayout();
    vibratoDepthLabel = new QLabel("Vibrato Depth:");
    vibratoDepthSlider = new QSlider(Qt::Horizontal); vibratoDepthSlider->setRange(0, 25); vibratoDepthSlider->setValue(14);
    vDepthLayout->addWidget(vibratoDepthLabel); vDepthLayout->addWidget(vibratoDepthSlider);
    controlsLayout->addLayout(vDepthLayout);

    QHBoxLayout *breathLayout = new QHBoxLayout();
    breathLabel = new QLabel("Breath / Aspiration:");
    breathSlider = new QSlider(Qt::Horizontal); breathSlider->setRange(0, 65); breathSlider->setValue(28);
    breathLayout->addWidget(breathLabel); breathLayout->addWidget(breathSlider);
    controlsLayout->addLayout(breathLayout);

    QHBoxLayout *brightLayout = new QHBoxLayout();
    brightnessLabel = new QLabel("Brightness:");
    brightnessSlider = new QSlider(Qt::Horizontal); brightnessSlider->setRange(0, 50); brightnessSlider->setValue(38);
    brightLayout->addWidget(brightnessLabel); brightLayout->addWidget(brightnessSlider);
    controlsLayout->addLayout(brightLayout);

    QHBoxLayout *shimmerLayout = new QHBoxLayout();
    shimmerLabel = new QLabel("Shimmer (organic life):");
    shimmerSlider = new QSlider(Qt::Horizontal); shimmerSlider->setRange(0, 40); shimmerSlider->setValue(18);
    shimmerLayout->addWidget(shimmerLabel); shimmerLayout->addWidget(shimmerSlider);
    controlsLayout->addLayout(shimmerLayout);

    nightlyCheckBox = new QCheckBox("Nightly Build (ExprTk - radians)");
    nightlyCheckBox->setChecked(true);
    generateButton = new QPushButton("GENERATE ADVANCED VOCAL STAB");
    generateButton->setStyleSheet("font-weight: bold; height: 42px; background-color: #c026d3; color: white;");

    controlsLayout->addWidget(nightlyCheckBox);
    controlsLayout->addWidget(generateButton);

    mainLayout->addWidget(controlsGroup);

    QGroupBox *outputGroup = new QGroupBox("Paste into LMMS Xpressive");
    QVBoxLayout *outLayout = new QVBoxLayout(outputGroup);
    outLayout->addWidget(new QLabel("W1 (Formant-weighted harmonics):"));
    w1Output = new QTextEdit(); w1Output->setMaximumHeight(60); w1Output->setReadOnly(true);
    outLayout->addWidget(w1Output);

    outLayout->addWidget(new QLabel("O1 (Full physical model):"));
    o1Output = new QTextEdit(); o1Output->setReadOnly(true);
    outLayout->addWidget(o1Output);

    mainLayout->addWidget(outputGroup);
}

void HouseVocalStabsTab::updateLabels() {
    int midi = pitchSlider->value();
    pitchLabel->setText(QString("Base Pitch: %1").arg(midi));
    glottalLabel->setText(QString("Glottal Click: %1%").arg(glottalScoopSlider->value()));
    durationLabel->setText(QString("Length: %1 s").arg(durationSlider->value()/100.0, 0, 'f', 2));
    vibratoRateLabel->setText(QString("Vibrato: %1 Hz").arg(vibratoRateSlider->value()/10.0));
    breathLabel->setText(QString("Aspiration: %1%").arg(breathSlider->value()));
    brightnessLabel->setText(QString("Brightness: %1%").arg(brightnessSlider->value()));
    shimmerLabel->setText(QString("Shimmer: %1%").arg(shimmerSlider->value()));
}

QString HouseVocalStabsTab::buildAdvancedVocalExpression(bool nightly) {
    bool isNightly = nightly;

    double baseMidi = pitchSlider->value();
    double baseHz   = 440.0 * std::pow(2.0, (baseMidi - 69.0) / 12.0);

    double dur      = durationSlider->value() / 100.0;
    double scoop    = glottalScoopSlider->value() / 100.0 * 12.0;
    double vibRate  = vibratoRateSlider->value() / 10.0;
    double vibDepth = vibratoDepthSlider->value() / 100.0;
    double breath   = breathSlider->value() / 100.0;
    double bright   = brightnessSlider->value() / 100.0;
    double shimmer  = shimmerSlider->value() / 100.0;

    QString sinFunc = isNightly ? "sin" : "sinew";
    QString piConst = isNightly ? "6.28318 * " : "";


    QString w1 = "";
    for (int n = 1; n <= 24; ++n) {

        QString f1 = QString("0.95 / (1 + (((%1*t - 780)/165) * ((%1*t - 780)/165)))").arg(n);
        QString f2 = QString("0.82 / (1 + (((%1*t - 1420)/210) * ((%1*t - 1420)/210)))").arg(n);
        QString f3 = QString("0.65 / (1 + (((%1*t - 2550)/280) * ((%1*t - 2550)/280)))").arg(n);
        QString f4 = QString("0.45 / (1 + (((%1*t - 3700)/380) * ((%1*t - 3700)/380)))").arg(n);

        QString tilt = QString("exp(-1.15 * log(%1))").arg(n);

        QString term = QString("%1 * (%2 + %3 + %4 + %5) * %6(%7t)")
                           .arg(tilt)
                           .arg(f1).arg(f2).arg(f3).arg(f4)
                           .arg(sinFunc)
                           .arg(piConst);

        if (n > 1) w1 += " + ";
        w1 += term;
    }


    QString pitchMod = QString("(%1 * exp(0.693147 * (%2 * %3(%4t * %5))))")
                           .arg(baseHz)
                           .arg(vibDepth)
                           .arg(sinFunc)
                           .arg(piConst)
                           .arg(vibRate);

    QString phase = QString("integrate(%1 - %2 * exp(-t * 72.0))")   // slightly faster glottal click
                        .arg(pitchMod)
                        .arg(scoop);

    QString gate       = QString("(t < %1)").arg(dur);
    QString shimmerEnv = QString("(1.0 + %1 * (randv(t*2100) - 0.5))").arg(shimmer);
    QString aspiration = QString("randv(t*4800) * %1").arg(breath);

    QString o1 = QString("clamp(-1.0, (W1(%1) * %2 * %3 + %4) * exp(-t * (7.2 + %5*19.0)), 1.0)")
                     .arg(phase)
                     .arg(gate)
                     .arg(shimmerEnv)
                     .arg(aspiration)
                     .arg(bright);

    w1Output->setText(w1);
    o1Output->setText(o1);
    QApplication::clipboard()->setText(o1);

    return o1;
}

void HouseVocalStabsTab::generateStab() {
    bool nightly = nightlyCheckBox->isChecked();
    buildAdvancedVocalExpression(nightly);
}
