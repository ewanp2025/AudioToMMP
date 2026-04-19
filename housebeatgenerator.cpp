#include "housebeatgenerator.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QRandomGenerator>
#include <QApplication>
#include <QPainterPath>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr bool ENABLE_DEV_MODE = true;


VolumeAutomationLane::VolumeAutomationLane(int bars, const QString& name, QWidget *parent)
    : QWidget(parent), m_name(name), m_bars(bars), m_isAutomated(false)
{
    setFixedHeight(35);
    setMinimumWidth(800);
    m_points.resize(bars * 16, 1.0f);
}

void VolumeAutomationLane::setBars(int bars) {
    m_bars = bars; m_points.resize(bars * 16, 1.0f); m_isAutomated = false; update();
}

float VolumeAutomationLane::getVolumeAtRatio(float ratio) const {
    if (m_points.empty()) return 1.0f;
    int index = qBound(0, (int)(ratio * (m_points.size() - 1)), (int)m_points.size() - 1);
    return m_points[index];
}

bool VolumeAutomationLane::hasAutomation() const { return m_isAutomated; }

void VolumeAutomationLane::updateValueFromMouse(QMouseEvent *event) {
    if (m_points.empty()) return;
    m_isAutomated = true;
    int ptIndex = qBound(0, (int)((event->pos().x() / (float)width()) * m_points.size()), (int)m_points.size() - 1);
    float val = 1.0f - qBound(0.0f, (event->pos().y() / (float)height()), 1.0f);
    m_points[ptIndex] = val;

    if (ptIndex > 0 && m_points[ptIndex-1] != val) m_points[ptIndex-1] = (m_points[ptIndex-1] + val) / 2.0f;
    if (ptIndex < m_points.size()-1 && m_points[ptIndex+1] != val) m_points[ptIndex+1] = (m_points[ptIndex+1] + val) / 2.0f;
    update();
}

void VolumeAutomationLane::mousePressEvent(QMouseEvent *event) { updateValueFromMouse(event); }
void VolumeAutomationLane::mouseMoveEvent(QMouseEvent *event) { if (event->buttons() & Qt::LeftButton) updateValueFromMouse(event); }

void VolumeAutomationLane::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DashLine));
    for (int b = 1; b < m_bars; ++b) {
        int x = (b / (float)m_bars) * width();
        painter.drawLine(x, 0, x, height());
    }

    painter.setPen(QColor(150, 150, 150));
    QFont font = painter.font(); font.setPointSize(8); painter.setFont(font);
    painter.drawText(5, 20, m_name);

    painter.setPen(QPen(QColor(0, 200, 100), 2));
    if (m_points.empty()) return;

    QPainterPath path;
    path.moveTo(0, height() - (m_points[0] * height()));
    for (size_t i = 1; i < m_points.size(); ++i) {
        float x = (i / (float)(m_points.size() - 1)) * width();
        float y = height() - (m_points[i] * height());
        path.lineTo(x, y);
    }
    painter.drawPath(path);
}


HouseBeatGenerator::HouseBeatGenerator(QWidget *parent) : QWidget(parent)
{
    m_drums = {
        {"Kick", "((t<0.006)*sinew(2*pi*6800*t)*exp(-t*1250) + sinew(integrate(f*(52/440)*(1+A2*0.85*exp(-t*58)))) * exp(-t*(3.1+A1*13)) * (0.88 + 0.12*(v>0.65)))", "A1=0.28 A2=0.65 A3=0.18", 200, 0, false, false},
        {"Snare", "(  randsv(t * srate, 0) * exp(-t * (A1 * 60 + 10)) * (t < 0.04) + sinew(integrate(200)) * exp(-t * (A1 * 40 + 10)))", "A1=0.65 A2=0.45 A3=0.55", 180, 0, false, true},
        {"Clap", "(t<0.003)*sinew(2*pi*3100*t)*exp(-t*2800) + randsv(t*srate*1.8,0)*exp(-t*32)*(t<0.055) + randsv(t*srate*0.9,0)*exp(-t*26)*(t<0.11)", "A1=0.72 A2=0.38 A3=0.65", 155, 12, false, true},
        {"ClosedHat", "randsv(t*srate*(2+A2*3),0)*exp(-t*(18+A1*28))*(t<0.042)", "A1=0.35 A2=0.55 A3=0.75", 105, -18, true, false},
        {"OpenHat", "randsv(t*srate*(1.6+A2*2.2),0)*exp(-t*(9+A1*14))*(t<0.135)", "A1=0.82 A2=0.45 A3=0.65", 92, 18, false, false},
        {"RimShot", "(t<0.007)*sinew(2*pi*2800*t)*exp(-t*1350) + sinew(integrate(380*(1+A2*0.45*exp(-t*38))))*exp(-t*(A1*13+4))*0.65", "A1=0.45 A2=0.72 A3=0.28", 125, -8, false, true},
        {"LowTom", "sinew(integrate(f*(92/440)*(1+A2*1.1*exp(-t*28))))*exp(-t*(5+A1*16))", "A1=0.55 A2=0.75 A3=0.22", 140, 10, false, false},
        {"MidTom", "sinew(integrate(f*(138/440)*(1+A2*0.95*exp(-t*32))))*exp(-t*(4.8+A1*14))", "A1=0.48 A2=0.82 A3=0.25", 135, 5, false, false},
        {"HighTom", "sinew(integrate(f*(185/440)*(1+A2*1.05*exp(-t*35))))*exp(-t*(4.2+A1*12))", "A1=0.42 A2=0.88 A3=0.20", 130, 0, false, false},
        {"Maracas", "randsv(t*srate*(4+A2*2),0)*exp(-t*(A1*65+12))*(t<0.035)", "A1=0.42 A2=0.65 A3=0.88", 82, 25, true, false},
        {"Cowbell", "(t<0.008)*sinew(2*pi*2400*t)*exp(-t*920) + sinew(integrate(520*(1+A2*0.6*exp(-t*28))))*exp(-t*(A1*18+6))*0.75", "A1=0.38 A2=0.68 A3=0.45", 95, -12, false, false},
        {"Conga", "sinew(integrate(f*(110/440)*(1+A2*0.9*exp(-t*25))))*exp(-t*(6+A1*15)) + randsv(t*srate*2.5,0)*exp(-t*(A1*22+8))*(t<0.045)", "A1=0.52 A2=0.78 A3=0.35", 110, 15, false, false},
        {"Crash", "randsv(t*srate*8,0)*exp(-t*(A1*120+8)) + sinew(integrate(1800*(1+A2*0.3*exp(-t*45))))*exp(-t*(A1*28+12))*0.75", "A1=0.65 A2=0.55 A3=0.80", 95, 8, false, false}
    };

    createPresets();
    setupUI();
    applyPreset(0);
}

HouseBeatGenerator::~HouseBeatGenerator() {}

void HouseBeatGenerator::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);


    QHBoxLayout *topRow1 = new QHBoxLayout();
    m_presetCombo = new QComboBox();


    m_presetCombo = new QComboBox();


    for (int i = 1; i <= 30; ++i) {
        m_presetCombo->addItem(QString("Pattern %1").arg(i));
    }

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HouseBeatGenerator::onPresetChanged);

    m_bpmSpin = new QSpinBox(); m_bpmSpin->setRange(100,140); m_bpmSpin->setValue(123); m_bpmSpin->setSuffix(" BPM");
    m_songLengthSpin = new QSpinBox(); m_songLengthSpin->setRange(1, 64); m_songLengthSpin->setValue(4); m_songLengthSpin->setSuffix(" Bars");
    connect(m_songLengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HouseBeatGenerator::onSongLengthChanged);

    m_basslineToggle = new QCheckBox("Enable Bassline Engine");
    m_basslineToggle->setChecked(false);

    m_basslineSelector = new QComboBox();
    for (int i = 1; i <= 30; ++i) {
        m_basslineSelector->addItem(QString("Pattern %1").arg(i));
    }

    m_pianoTriadsToggle = new QCheckBox("Enable Minor Piano Triads");
    m_pianoTriadsToggle->setChecked(false);
    topRow1->addWidget(m_pianoTriadsToggle);

    m_shuffleDial = new QSpinBox(); m_shuffleDial->setRange(1,7); m_shuffleDial->setValue(1); m_shuffleDial->setSuffix(" Swing");
    connect(m_shuffleDial, QOverload<int>::of(&QSpinBox::valueChanged), this, &HouseBeatGenerator::onSwingGlobalChanged);

    m_ghostIntensity = new QDoubleSpinBox(); m_ghostIntensity->setRange(30,100); m_ghostIntensity->setValue(72); m_ghostIntensity->setSuffix("% Ghost");

    topRow1->addWidget(new QLabel("Preset:")); topRow1->addWidget(m_presetCombo);
    topRow1->addWidget(new QLabel("BPM:")); topRow1->addWidget(m_bpmSpin);
    topRow1->addWidget(new QLabel("Song Length:")); topRow1->addWidget(m_songLengthSpin);
    topRow1->addWidget(new QLabel("Shuffle:")); topRow1->addWidget(m_shuffleDial);
    topRow1->addWidget(new QLabel("Ghost Vol:")); topRow1->addWidget(m_ghostIntensity);
    topRow1->addStretch();
    topRow1->addWidget(m_basslineToggle);
    topRow1->addWidget(m_basslineSelector);


    mainLayout->addLayout(topRow1);
    initializeBasslinePatterns();


    QHBoxLayout *middleRow = new QHBoxLayout();

    m_grid = new QTableWidget(13, 65, this);
    QStringList drumNames; for (auto& d : m_drums) drumNames << d.name;
    m_grid->setVerticalHeaderLabels(drumNames);

    QStringList hHeaders;
    for (int i = 1; i <= 64; ++i) hHeaders << QString::number(i);
    hHeaders << "Swing";
    m_grid->setHorizontalHeaderLabels(hHeaders);

    for (int c = 0; c < 64; ++c) m_grid->setColumnWidth(c, 20);
    m_grid->setColumnWidth(64, 45);
    m_grid->verticalHeader()->setDefaultSectionSize(20);
    m_grid->horizontalHeader()->setDefaultSectionSize(20);

    for (int r = 0; r < m_drums.size(); ++r) {
        QTableWidgetItem *swingItem = new QTableWidgetItem();
        swingItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        swingItem->setCheckState(m_drums[r].swingEnabledByDefault ? Qt::Checked : Qt::Unchecked);
        swingItem->setBackground(QColor(60, 60, 60));
        m_grid->setItem(r, 64, swingItem);
    }
    connect(m_grid, &QTableWidget::cellClicked, this, &HouseBeatGenerator::onGridCellClicked);
    middleRow->addWidget(m_grid, 3);


    QGroupBox *hwGroup = new QGroupBox("909 Hardware Matrix");
    QGridLayout *hwLayout = new QGridLayout(hwGroup);
    hwLayout->setSpacing(2);
    hwLayout->setContentsMargins(5, 10, 5, 5);

    QStringList headers = {"", "Tune", "Attack", "Decay", "Tone", "Snappy"};
    for(int i=1; i<headers.size(); ++i) {
        QLabel *lbl = new QLabel(headers[i]);
        lbl->setAlignment(Qt::AlignCenter);
        QFont f = lbl->font(); f.setPointSize(8); lbl->setFont(f);
        hwLayout->addWidget(lbl, 0, i);
    }

    auto addKnobMatrix = [&](const QString& drum, int row, int col, std::map<QString, QDoubleSpinBox*>& map, double def, double min, double max) {
        QDoubleSpinBox* spin = new QDoubleSpinBox();
        spin->setRange(min, max); spin->setValue(def); spin->setSingleStep(0.05);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setFixedWidth(38);
        QFont f = spin->font(); f.setPointSize(8); spin->setFont(f);
        spin->setToolTip(drum + " " + headers[col]);
        hwLayout->addWidget(spin, row, col, Qt::AlignCenter);
        map[drum] = spin;
    };

    QStringList rDrums = {"Kick", "Snare", "LowTom", "MidTom", "HighTom", "CHat", "OHat", "Crash"};
    for(int i=0; i<rDrums.size(); ++i) {
        QLabel *lbl = new QLabel(rDrums[i]);
        QFont f = lbl->font(); f.setPointSize(8); lbl->setFont(f);
        hwLayout->addWidget(lbl, i+1, 0);
    }

    addKnobMatrix("Kick", 1, 1, m_tuneKnobs, 0.0, -12.0, 12.0); addKnobMatrix("Kick", 1, 2, m_attackKnobs, 0.5, 0.0, 1.0); addKnobMatrix("Kick", 1, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("Snare", 2, 1, m_tuneKnobs, 0.0, -12.0, 12.0); addKnobMatrix("Snare", 2, 4, m_toneKnobs, 0.5, 0.0, 1.0); addKnobMatrix("Snare", 2, 5, m_snappyKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("LowTom", 3, 1, m_tuneKnobs, 0.0, -12.0, 12.0); addKnobMatrix("LowTom", 3, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("MidTom", 4, 1, m_tuneKnobs, 0.0, -12.0, 12.0); addKnobMatrix("MidTom", 4, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("HighTom", 5, 1, m_tuneKnobs, 0.0, -12.0, 12.0); addKnobMatrix("HighTom", 5, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("ClosedHat", 6, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("OpenHat", 7, 3, m_decayKnobs, 0.5, 0.0, 1.0);
    addKnobMatrix("Crash", 8, 1, m_tuneKnobs, 0.0, -12.0, 12.0);
    hwLayout->setRowStretch(9, 1);

    middleRow->addWidget(hwGroup, 1);
    mainLayout->addLayout(middleRow, 2);

    QGroupBox *autoGroup = new QGroupBox("Volume Automation Matrix");
    QVBoxLayout *autoLayout = new QVBoxLayout(autoGroup);
    autoLayout->setContentsMargins(2, 2, 2, 2);

    QWidget *autoContainer = new QWidget();
    QVBoxLayout *autoContainerLayout = new QVBoxLayout(autoContainer);
    autoContainerLayout->setSpacing(2);

    for (const auto& drum : m_drums) {
        VolumeAutomationLane *lane = new VolumeAutomationLane(m_songLengthSpin->value(), drum.name);
        m_automationLanes.push_back(lane);
        autoContainerLayout->addWidget(lane);
    }

    m_automationScrollArea = new QScrollArea();
    m_automationScrollArea->setWidget(autoContainer);
    m_automationScrollArea->setWidgetResizable(true);
    m_automationScrollArea->setMinimumHeight(150);
    autoLayout->addWidget(m_automationScrollArea);
    mainLayout->addWidget(autoGroup, 1);

    QHBoxLayout *randRow = new QHBoxLayout();
    QPushButton *b1 = new QPushButton("Random Snare Build"); QPushButton *b2 = new QPushButton("Random Snare Pattern"); QPushButton *b3 = new QPushButton("Random Rimshot Pattern");
    m_btnDuplicate16 = new QPushButton("Duplicate 1-16 to 64");

    randRow->addWidget(b1); randRow->addWidget(b2); randRow->addWidget(b3); randRow->addWidget(m_btnDuplicate16);
    randRow->addWidget(b1); randRow->addWidget(b2); randRow->addWidget(b3);


    m_btnRandomDeepHouse = new QPushButton("Random Deep House");
    m_btnRandomDeepHouse->setStyleSheet("background-color:#2a4a6e; color:white; font-weight:bold; padding:8px;");
    randRow->addWidget(m_btnRandomDeepHouse);
    connect(m_btnRandomDeepHouse, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomDeepHouseClicked);


    mainLayout->addLayout(randRow);
    connect(b1, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomSnareBuild);
    connect(b2, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomSnarePattern);
    connect(b3, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomRimshotPattern);
    connect(m_btnDuplicate16, &QPushButton::clicked, this, &HouseBeatGenerator::onDuplicate16Clicked);


    QHBoxLayout *bottomRow = new QHBoxLayout();

    m_closedHatVelCombo = new QComboBox(); m_closedHatVelCombo->addItems({"CH Vel: Flat", "CH Vel: Offbeats", "CH Vel: Pumping", "CH Vel: Human", "CH Vel: Semi-Random"});
    m_openHatVelCombo = new QComboBox(); m_openHatVelCombo->addItems({"OH Vel: Flat", "OH Vel: Offbeats", "OH Vel: Pumping", "OH Vel: Human", "OH Vel: Semi-Random"});
    m_velDepthSpin = new QSpinBox(); m_velDepthSpin->setRange(0, 200); m_velDepthSpin->setValue(100); m_velDepthSpin->setSuffix("% Depth");

    m_hatFxModeCombo = new QComboBox(); m_hatFxModeCombo->addItems({"CH Fx: None", "CH Fx: Phaser", "CH Fx: Filter"});
    m_filterTypeCombo = new QComboBox(); m_filterTypeCombo->addItems({"LP", "HP", "BP"}); m_filterTypeCombo->setVisible(false);
    m_filterModCombo = new QComboBox(); m_filterModCombo->addItems({"Mod: LFO", "Mod: Pattern"}); m_filterModCombo->setVisible(false);
    m_lfoBarsSpin = new QDoubleSpinBox(); m_lfoBarsSpin->setRange(0.25, 16.0); m_lfoBarsSpin->setValue(1.0); m_lfoBarsSpin->setSuffix(" Bars"); m_lfoBarsSpin->setVisible(false);
    connect(m_hatFxModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HouseBeatGenerator::onHatFxModeChanged);

    m_btnExport = new QPushButton("Export Song to .mmp");
    m_btnExport->setStyleSheet("background-color:#00557f;color:white;font-weight:bold;padding:12px; font-size: 14px;");
    connect(m_btnExport, &QPushButton::clicked, this, &HouseBeatGenerator::onExportMMPClicked);

    m_btnDevDump = new QPushButton("DEV: Dump Pattern");
    m_btnDevDump->setStyleSheet("background-color:#550000; color:white; font-weight:bold; padding:12px;");
    m_btnDevDump->setVisible(ENABLE_DEV_MODE);
    connect(m_btnDevDump, &QPushButton::clicked, this, &HouseBeatGenerator::onDevDumpClicked);


    m_btnDevLoad = new QPushButton("DEV: Load Pattern");
    m_btnDevLoad->setStyleSheet("background-color:#005500; color:white; font-weight:bold; padding:12px;"); // Dark green to distinguish it
    m_btnDevLoad->setVisible(ENABLE_DEV_MODE);
    connect(m_btnDevLoad, &QPushButton::clicked, this, &HouseBeatGenerator::onDevLoadClicked);

    bottomRow->addWidget(m_closedHatVelCombo); bottomRow->addWidget(m_openHatVelCombo); bottomRow->addWidget(new QLabel("Vel Depth:")); bottomRow->addWidget(m_velDepthSpin);
    bottomRow->addWidget(m_hatFxModeCombo); bottomRow->addWidget(m_filterTypeCombo); bottomRow->addWidget(m_filterModCombo); bottomRow->addWidget(m_lfoBarsSpin);
    bottomRow->addStretch();

    bottomRow->addWidget(m_btnDevLoad);
    bottomRow->addWidget(m_btnDevDump);
    bottomRow->addWidget(m_btnExport);
    mainLayout->addLayout(bottomRow);
}

void HouseBeatGenerator::onSongLengthChanged(int bars) {
    for (auto lane : m_automationLanes) lane->setBars(bars);
}

void HouseBeatGenerator::onHatFxModeChanged(int index) {
    bool isFilter = (index == 2);
    m_filterTypeCombo->setVisible(isFilter);
    m_filterModCombo->setVisible(isFilter);
    m_lfoBarsSpin->setVisible(index > 0);
}

void HouseBeatGenerator::onGridCellClicked(int row, int col) {
    if (col == 64) return;
    float v = m_velocities[row][col];


    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();

    if (mods & Qt::ShiftModifier) {

        m_velocities[row][col] = (v == 0.72f) ? 0.0f : 0.72f;
    } else if (QApplication::mouseButtons() & Qt::RightButton) {
        m_velocities[row][col] = (v == 0.0f) ? 0.72f : (v == 0.72f ? 1.0f : 0.0f);
    } else {
        m_velocities[row][col] = (v == 0.0f || v == 0.72f) ? 1.0f : (v == 1.0f && m_drums[row].canFlam ? 2.0f : 0.0f);
    }
    updateCell(row, col);
}

void HouseBeatGenerator::updateCell(int row, int col) {
    if (col >= 64) return;
    float v = m_velocities[row][col];
    QTableWidgetItem *item = m_grid->item(row, col);
    if (!item) { item = new QTableWidgetItem(); m_grid->setItem(row, col, item); }

    if (v == 2.0f) { item->setBackground(QColor(255, 150, 0)); item->setText("F"); }
    else if (v == 0.0f) { item->setBackground(QColor(40,40,40)); item->setText(""); }
    else if (v < 0.9f) { item->setBackground(QColor(100,180,255)); item->setText("g"); }
    else { item->setBackground(QColor(0,200,100)); item->setText(""); }
}

void HouseBeatGenerator::createPresets() {
    m_presets.resize(30, std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f)));

    auto setNotes = [&](int pIdx, int dIdx, std::initializer_list<int> notes, float vol = 1.0f) {
        for (int n : notes) m_presets[pIdx][dIdx][n] = vol;
    };

    auto fill64 = [&](int pIdx) {
        for (int r = 0; r < 13; ++r) {
            for (int c = 0; c < 16; ++c) {
                float v = m_presets[pIdx][r][c];
                m_presets[pIdx][r][c+16] = v;
                m_presets[pIdx][r][c+32] = v;
                m_presets[pIdx][r][c+48] = v;
            }
        }
    };


    for (int p = 0; p < 25; ++p) {
        int barOffset = (p % 5) * 4;


        for (int i = 0; i < 64; i += 4) m_presets[p][0][i] = 1.0f;
        if (p % 3 == 0) m_presets[p][0][28] = 0.0f;


        for (int b = 0; b < 64; b += 16) {
            m_presets[p][1][b+4] = 1.0f;
            m_presets[p][1][b+12] = 1.0f;
            if (p % 2 == 1) {
                m_presets[p][1][b+2] = 0.65f;
                m_presets[p][1][b+10] = 0.70f;
            }
        }


        for (int i = 1; i < 64; i += 2) {
            float vel = (i % 8 == 0) ? 0.95f : 0.52f + (rand() % 35)/100.0f;
            m_presets[p][3][i] = vel;
        }
        for (int i = 0; i < 64; i += 4) {
            if (p % 4 != 0) m_presets[p][3][i] = 0.42f;
        }


        for (int b = 0; b < 64; b += 16) {
            if (p % 3 != 2) m_presets[p][4][b+14] = 0.88f;
        }


        for (int i = 1; i < 64; i += 2) {
            if ((i % 4) != 0) m_presets[p][9][i] = 0.58f + (rand() % 25)/100.0f;
        }


        for (int b = 0; b < 64; b += 16) {
            m_presets[p][11][b+2] = 0.78f;
            m_presets[p][11][b+6] = 0.65f;
            m_presets[p][11][b+10] = 0.82f;
            m_presets[p][11][b+14] = (p % 3 == 0) ? 0.72f : 0.0f;
        }


        if (p % 4 == 1) {
            m_presets[p][5][10] = 0.75f;
            m_presets[p][5][42] = 0.78f;
        }


        if (p % 7 == 0) m_presets[p][10][52] = 0.62f;

        fill64(p);
    }


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(25, 0, {b+0, b+4, b+8, b+12});
        setNotes(25, 2, {b+4, b+12}, 0.85f);
        setNotes(25, 3, {b+2, b+6, b+10, b+14}, 0.90f);
        setNotes(25, 3, {b+1, b+3, b+7, b+9, b+11, b+15}, 0.35f);
        setNotes(25, 4, {b+2, b+10}, 0.80f);
        setNotes(25, 11, {b+7, b+14, b+15}, 0.75f);
        if (bar % 2 == 1) setNotes(25, 5, {b+15}, 0.85f);
    }


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(26, 0, {b+0, b+4, b+8, b+12});
        setNotes(26, 0, {b+15}, 0.40f);
        setNotes(26, 1, {b+4, b+12}, 0.70f);
        for (int i = 0; i < 16; ++i) m_presets[26][3][b+i] = 0.50f;
        setNotes(26, 3, {b+2, b+6, b+10, b+14}, 0.95f);
        setNotes(26, 6, {b+3, b+8, b+11}, 0.65f);
    }


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(27, 0, {b+0, b+4, b+8, b+12});
        setNotes(27, 2, {b+4, b+12}, 0.80f);
        setNotes(27, 4, {b+2, b+6, b+10, b+14}, 0.90f);
        setNotes(27, 5, {b+3, b+6, b+9, b+14}, 0.75f);
        setNotes(27, 11, {b+5, b+13}, 0.80f);
    }


    for (int i = 0; i < 64; i += 4) m_presets[28][0][i] = 1.0f;
    for (int b = 0; b < 64; b += 16) {
        m_presets[28][1][b+4] = 1.0f; m_presets[28][1][b+12] = 1.0f;
        m_presets[28][4][b+14] = 0.90f;
        m_presets[28][9][b+1] = m_presets[28][9][b+3] = m_presets[28][9][b+5] = m_presets[28][9][b+7] = 0.65f;
        m_presets[28][11][b+2] = 0.78f; m_presets[28][11][b+6] = 0.72f; m_presets[28][11][b+10] = 0.80f;
    }
    for (int i = 1; i < 64; i += 2) m_presets[28][3][i] = (i%8==0) ? 0.92f : 0.58f;


    for (int i = 0; i < 64; i += 4) m_presets[29][0][i] = 1.0f;
    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        m_presets[29][1][b+4] = 1.0f;
        m_presets[29][1][b+12] = 1.0f;
        m_presets[29][5][b+10] = 0.78f;
        m_presets[29][11][b+1] = 0.82f; m_presets[29][11][b+5] = 0.75f;
        m_presets[29][11][b+9] = 0.80f; m_presets[29][11][b+13] = 0.70f;
    }
    for (int i = 1; i < 64; i += 2) {
        m_presets[29][3][i] = (i % 8 == 0) ? 0.92f : 0.68f;
    }
}

void HouseBeatGenerator::applyPreset(int index) {
    if (index < 0 || index >= m_presets.size()) return;
    m_velocities = m_presets[index];

    for (int r = 0; r < m_drums.size(); ++r) {
        for (int c = 0; c < 64; ++c) {
            updateCell(r, c);
        }
    }
}

void HouseBeatGenerator::onPresetChanged(int index) { applyPreset(index); }

void HouseBeatGenerator::onSwingGlobalChanged() {

    for (int r = 0; r < m_drums.size(); ++r) {
        for (int c = 0; c < 64; ++c) {
            updateCell(r, c);
        }
    }
}

void HouseBeatGenerator::onExportMMPClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "Save House", "House_MultiBar.mmp", "LMMS Project (*.mmp)");
    if (path.isEmpty()) return;
    buildMMP(path);
    QMessageBox::information(this, "Success", "Multi-Bar House Sequence exported!\n");
}

void HouseBeatGenerator::buildMMP(const QString &filePath)
{
    int shuffleSetting = m_shuffleDial->value();
    bool globalSwingActive = (shuffleSetting > 1);
    int totalBars = m_songLengthSpin->value();
    int bpm = m_bpmSpin->value();

    double bars = m_lfoBarsSpin->value();
    double lfoHz = bpm / (240.0 * bars);
    int fxMode = m_hatFxModeCombo->currentIndex();
    int filterFreqId = 24671;
    bool hasFilterAutomation = false;

    int baseVolId = 10000;

    QDomDocument doc;
    QDomElement root = doc.createElement("lmms-project");
    root.setAttribute("version", "20"); root.setAttribute("type", "song");
    doc.appendChild(root);

    QDomElement head = doc.createElement("head");
    head.setAttribute("bpm", QString::number(bpm));
    head.setAttribute("mastervol", "100");
    root.appendChild(head);

    QDomElement song = doc.createElement("song"); root.appendChild(song);
    QDomElement trackContainer = doc.createElement("trackcontainer"); trackContainer.setAttribute("type", "song"); song.appendChild(trackContainer);
    QDomElement bbTrack = doc.createElement("track"); bbTrack.setAttribute("type", "1"); bbTrack.setAttribute("name", "House Multi-Bar Generator"); trackContainer.appendChild(bbTrack);
    QDomElement bbContainer = doc.createElement("bbtrack"); bbTrack.appendChild(bbContainer);
    QDomElement innerContainer = doc.createElement("trackcontainer"); innerContainer.setAttribute("type", "bbtrackcontainer"); bbContainer.appendChild(innerContainer);

    double swingFactor = (shuffleSetting - 1) / 6.0;


    struct ExportNote {
        int key;
        int vol;
        QString len;
        int pos;
    };

    for (int d = 0; d < 13; ++d) {
        QDomElement drumTrack = doc.createElement("track");
        drumTrack.setAttribute("type", "0");
        drumTrack.setAttribute("name", m_drums[d].name);
        innerContainer.appendChild(drumTrack);

        QDomElement instrTrack = doc.createElement("instrumenttrack");
        instrTrack.setAttribute("vol", QString::number(m_drums[d].defaultVol));
        instrTrack.setAttribute("pan", "0");                    // ← FIXED: centre panning
        // (you can still use m_drums[d].defaultPan if you want per-drum panning later)

        int drumVolId = baseVolId + d;
        if (m_automationLanes[d]->hasAutomation()) {
            instrTrack.setAttribute("id", QString::number(drumVolId));
        }
        drumTrack.appendChild(instrTrack);

        QDomElement instr = doc.createElement("instrument");
        instr.setAttribute("name", "xpressive");
        instrTrack.appendChild(instr);

        QString dynamicO1 = m_drums[d].xpressiveO1;

        if (m_tuneKnobs.count(m_drums[d].name)) {
            double tuneVal = m_tuneKnobs[m_drums[d].name]->value();
            QString tuneFormula = QString("semitone(%1)*f").arg(tuneVal);
            dynamicO1.replace("f", tuneFormula);
        }

        QString adsrStr = "((t < 0.01)*(t/0.01) + (t >= 0.01 & t < 0.1)*(1 - (t-0.01)/0.09*0.3) + (t >= 0.1 & t < 0.5)*0.7 + (t >= 0.5)*(0.7*exp(-(t-0.5)*10)))";
        dynamicO1 = "(" + dynamicO1 + ")*" + adsrStr;

        QDomElement xp = doc.createElement("xpressive");
        xp.setAttribute("version", "0.1"); xp.setAttribute("O1", dynamicO1);
        instr.appendChild(xp);

        if (d == 3 && fxMode > 0) {
            QDomElement fxchain = doc.createElement("fxchain"); fxchain.setAttribute("numofeffects", "1"); fxchain.setAttribute("enabled", "1");
            instrTrack.appendChild(fxchain);
            QDomElement effect = doc.createElement("effect"); effect.setAttribute("autoquit_numerator", "4"); effect.setAttribute("autoquit_denominator", "4"); effect.setAttribute("autoquit_syncmode", "0"); effect.setAttribute("autoquit", "1"); effect.setAttribute("gate", "0"); effect.setAttribute("on", "1"); effect.setAttribute("wet", "1"); effect.setAttribute("name", "ladspaeffect");
            fxchain.appendChild(effect);
            QDomElement ladspacontrols = doc.createElement("ladspacontrols");

            if (fxMode == 1) {
                ladspacontrols.setAttribute("ports", "8");
                QDomElement port1 = doc.createElement("port01"); QDomElement port1Data = doc.createElement("data"); port1Data.setAttribute("scale_type", "linear"); port1Data.setAttribute("id", "11103"); port1Data.setAttribute("value", QString::number(lfoHz)); port1.appendChild(port1Data); ladspacontrols.appendChild(port1);
                QDomElement port11 = doc.createElement("port11"); port11.setAttribute("data", QString::number(lfoHz)); ladspacontrols.appendChild(port11);
                QDomElement key = doc.createElement("key"); QDomElement attr1 = doc.createElement("attribute"); attr1.setAttribute("name", "file"); attr1.setAttribute("value", "caps"); QDomElement attr2 = doc.createElement("attribute"); attr2.setAttribute("name", "plugin"); attr2.setAttribute("value", "PhaserI"); key.appendChild(attr1); key.appendChild(attr2); effect.appendChild(ladspacontrols); effect.appendChild(key);
            } else if (fxMode == 2) {
                hasFilterAutomation = true;
                ladspacontrols.setAttribute("ports", "7");
                QDomElement port4 = doc.createElement("port04"); QDomElement port4Data = doc.createElement("data"); port4Data.setAttribute("scale_type", "log"); port4Data.setAttribute("id", QString::number(filterFreqId)); port4Data.setAttribute("value", "500"); port4.appendChild(port4Data); ladspacontrols.appendChild(port4);
                QDomElement port5 = doc.createElement("port05"); port5.setAttribute("data", "0.707"); ladspacontrols.appendChild(port5);
                int filterModes[] = {0, 6, 3}; int selectedMode = filterModes[m_filterTypeCombo->currentIndex()];
                QDomElement port6 = doc.createElement("port06"); QDomElement port6Data = doc.createElement("data"); port6Data.setAttribute("scale_type", "linear"); port6Data.setAttribute("id", "25563"); port6Data.setAttribute("value", QString::number(selectedMode)); port6.appendChild(port6Data); ladspacontrols.appendChild(port6);
                QDomElement key = doc.createElement("key"); QDomElement attr1 = doc.createElement("attribute"); attr1.setAttribute("name", "file"); attr1.setAttribute("value", "veal"); QDomElement attr2 = doc.createElement("attribute"); attr2.setAttribute("name", "plugin"); attr2.setAttribute("value", "Filter"); key.appendChild(attr1); key.appendChild(attr2); effect.appendChild(ladspacontrols); effect.appendChild(key);
            }
        }

        QTableWidgetItem* swingBox = m_grid->item(d, 64);
        bool appliesSwing = swingBox && (swingBox->checkState() == Qt::Checked);

        int shuffleSetting = m_shuffleDial->value();
        bool globalSwingActive = (shuffleSetting > 1);
        double swingFactor = (shuffleSetting - 1.0) / 6.0;

        bool usePianoRoll = (globalSwingActive && appliesSwing);

        for (int step = 0; step < 64; ++step) {
            if (m_velocities[d][step] == 2.0f) {
                usePianoRoll = true;
                break;
            }
        }

        QDomElement pattern = doc.createElement("pattern");
        pattern.setAttribute("steps", QString::number(64 * totalBars));
        pattern.setAttribute("pos", "0");
        pattern.setAttribute("type", usePianoRoll ? "1" : "0");
        drumTrack.appendChild(pattern);


        std::vector<ExportNote> trackNotes;

        for (int bar = 0; bar < totalBars; ++bar) {
            for (int step = 0; step < 64; ++step) {
                float v = m_velocities[d][step];
                if (v > 0.0f) {
                    int pos = 0;
                    int noteLenTicks = 12;


                    if (globalSwingActive && appliesSwing) {
                        int swingOffset = (int)(swingFactor * 18.0 + 0.5);
                        int posInBeat = step % 16;
                        int sixteenthBlock = posInBeat / 4;
                        int microStep = posInBeat % 4;
                        int baseTick = (step / 16) * 192 + (sixteenthBlock * 48);

                        if (sixteenthBlock == 0 || sixteenthBlock == 2) {

                            float stepSize = (48.0f + swingOffset) / 4.0f;
                            pos = (bar * 64 * 12) + baseTick + (int)(microStep * stepSize + 0.5f);
                            noteLenTicks = (int)stepSize;
                        } else {

                            float stepSize = (48.0f - swingOffset) / 4.0f;
                            pos = (bar * 64 * 12) + baseTick + swingOffset + (int)(microStep * stepSize + 0.5f);
                            noteLenTicks = (int)stepSize;
                        }
                    } else {
                        pos = (bar * 64 * 12) + (step * 12);
                    }

                    int vol = (int)(v * 100);
                    if (v < 0.9f) {
                        double ghostScale = m_ghostIntensity->value() / 100.0;
                        vol = (int)(vol * ghostScale);
                        if (vol < 1) vol = 1;
                    }

                    if (d == 3 || d == 4) {
                        int hatVelMode = (d == 3) ? m_closedHatVelCombo->currentIndex() : m_openHatVelCombo->currentIndex();
                        if (hatVelMode > 0) {
                            float targetVelMult = 1.0f;
                            int posInBeat = step % 16;
                            if (hatVelMode == 1) {
                                if (posInBeat == 0) targetVelMult = 0.4f;
                                else if (posInBeat == 8) targetVelMult = 1.0f;
                                else targetVelMult = 0.7f;
                            } else if (hatVelMode == 2) {
                                if (posInBeat == 0) targetVelMult = 0.1f;
                                else if (posInBeat == 4) targetVelMult = 0.5f;
                                else if (posInBeat == 8) targetVelMult = 1.0f;
                                else if (posInBeat == 12) targetVelMult = 0.8f;
                                else targetVelMult = 0.6f;
                            } else if (hatVelMode == 3) {
                                if (posInBeat == 8) targetVelMult = 1.0f;
                                else targetVelMult = 0.85f + (QRandomGenerator::global()->generateDouble() * 0.15f);
                            } else if (hatVelMode == 4) {
                                if (posInBeat == 8) targetVelMult = 0.8f + (QRandomGenerator::global()->generateDouble() * 0.2f);
                                else targetVelMult = 0.3f + (QRandomGenerator::global()->generateDouble() * 0.6f);
                            }
                            float depth = m_velDepthSpin->value() / 100.0f;
                            float finalVelMult = 1.0f - ((1.0f - targetVelMult) * depth);
                            if (finalVelMult < 0.05f) finalVelMult = 0.05f;
                            if (finalVelMult > 1.0f) finalVelMult = 1.0f;
                            vol = (int)(vol * finalVelMult);
                            if (vol < 1) vol = 1;
                        }
                    }

                    bool isFlam = (v == 2.0f);
                    if (isFlam && usePianoRoll) {

                        QString flamLen = QString::number(noteLenTicks / 2);
                        trackNotes.push_back({69, (int)(vol * 0.5), flamLen, pos});
                        trackNotes.push_back({69, vol, flamLen, pos + (noteLenTicks / 2)});
                    } else {
                        QString finalLen = usePianoRoll ? QString::number(noteLenTicks) : "-192";
                        trackNotes.push_back({69, vol, finalLen, pos});
                    }
                }
            }
        }

        std::sort(trackNotes.begin(), trackNotes.end(), [](const ExportNote& a, const ExportNote& b) {
            return a.pos < b.pos;
        });

        for (const auto& note : trackNotes) {
            QDomElement noteElem = doc.createElement("note");
            noteElem.setAttribute("key", QString::number(note.key));
            noteElem.setAttribute("vol", QString::number(note.vol));
            noteElem.setAttribute("len", note.len);
            noteElem.setAttribute("pos", QString::number(note.pos));
            pattern.appendChild(noteElem);
        }

        if (m_automationLanes[d]->hasAutomation()) {
            QDomElement autoTrack = doc.createElement("track");
            autoTrack.setAttribute("type", "5");
            autoTrack.setAttribute("name", m_drums[d].name + " Volume");
            innerContainer.appendChild(autoTrack);

            QDomElement atrackNode = doc.createElement("automationtrack");
            autoTrack.appendChild(atrackNode);

            QDomElement apatternNode = doc.createElement("automationpattern");
            apatternNode.setAttribute("pos", "0");
            apatternNode.setAttribute("mute", "0");
            apatternNode.setAttribute("tens", "1");
            apatternNode.setAttribute("name", m_drums[d].name + " Vol Curve");
            apatternNode.setAttribute("prog", "2");

            int totalTicksLength = totalBars * 64 * 12;
            apatternNode.setAttribute("len", QString::number(totalTicksLength));
            autoTrack.appendChild(apatternNode);

            for (int t = 0; t <= totalTicksLength; t += 48) {
                float ratio = (float)t / totalTicksLength;
                float volScale = m_automationLanes[d]->getVolumeAtRatio(ratio);
                float finalVol = m_drums[d].defaultVol * volScale;

                QDomElement timeNode = doc.createElement("time");
                timeNode.setAttribute("pos", QString::number(t));
                timeNode.setAttribute("value", QString::number(finalVol));
                timeNode.setAttribute("outValue", QString::number(finalVol));
                apatternNode.appendChild(timeNode);
            }

            QDomElement objNode = doc.createElement("object");
            objNode.setAttribute("id", QString::number(drumVolId));
            apatternNode.appendChild(objNode);
        }
    }

    if (hasFilterAutomation) {
        QDomElement autoTrack = doc.createElement("track"); autoTrack.setAttribute("type", "5"); autoTrack.setAttribute("name", "ClosedHat Filter Freq"); innerContainer.appendChild(autoTrack);
        QDomElement atrackNode = doc.createElement("automationtrack"); autoTrack.appendChild(atrackNode);
        QDomElement apatternNode = doc.createElement("automationpattern"); apatternNode.setAttribute("pos", "0"); apatternNode.setAttribute("mute", "0"); apatternNode.setAttribute("tens", "1"); apatternNode.setAttribute("name", "Calf Filter LADSPA>>>Frequency"); apatternNode.setAttribute("prog", "2"); apatternNode.setAttribute("len", "768"); autoTrack.appendChild(apatternNode);

        int modMode = m_filterModCombo->currentIndex();
        int durationTicks = 768;

        if (modMode == 0) {
            for (int t = 0; t <= durationTicks; t += 12) {
                float phase = (t / (float)durationTicks) * (4.0 / bars) * 2.0 * M_PI;
                float val = 200.0f + (8000.0f - 200.0f) * (0.5f + 0.5f * std::sin(phase));
                QDomElement timeNode = doc.createElement("time"); timeNode.setAttribute("pos", QString::number(t)); timeNode.setAttribute("value", QString::number(val)); timeNode.setAttribute("outValue", QString::number(val)); apatternNode.appendChild(timeNode);
            }
        } else if (modMode == 1) {
            float patternFreqs[4] = {400.0f, 2500.0f, 800.0f, 6000.0f};
            for (int t = 0; t <= durationTicks; t += 48) {
                float val = patternFreqs[(t / 48) % 4];
                QDomElement timeNode = doc.createElement("time"); timeNode.setAttribute("pos", QString::number(t)); timeNode.setAttribute("value", QString::number(val)); timeNode.setAttribute("outValue", QString::number(val)); apatternNode.appendChild(timeNode);
            }
        }

        QDomElement objNode = doc.createElement("object"); objNode.setAttribute("id", QString::number(filterFreqId)); apatternNode.appendChild(objNode);
    }


    if (m_basslineToggle->isChecked()) {
        appendBasslineTrackToMMP(doc, trackContainer, totalBars);
    }

    // ← NEW PIANO BLOCK
    if (m_pianoTriadsToggle && m_pianoTriadsToggle->isChecked()) {
        appendPianoTriadsToMMP(doc, trackContainer, totalBars);
    }


    QDomElement bbtco = doc.createElement("bbtco");
    bbtco.setAttribute("len", QString::number(totalBars * 768)); bbtco.setAttribute("pos", "0");
    bbTrack.appendChild(bbtco);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file); stream << "<?xml version=\"1.0\"?>\n<!DOCTYPE lmms-project>\n" << doc.toString(2); file.close();
    }
}

void HouseBeatGenerator::onRandomSnareBuild() { applyPreset(m_presetCombo->currentIndex()); }

void HouseBeatGenerator::onRandomSnarePattern() {

    std::vector<int> fillSteps = {23, 47, 54, 57, 58, 61, 63};


    for (int step : fillSteps) {
        m_velocities[1][step] = 1.00f;
        updateCell(1, step);
    }


    int idx = m_presetCombo->currentIndex();
    if (idx >= 0 && idx < (int)m_presets.size()) {
        m_presets[idx] = m_velocities;
    }
}

void HouseBeatGenerator::onRandomRimshotPattern() { applyPreset(m_presetCombo->currentIndex()); }

void HouseBeatGenerator::onDevDumpClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Pattern Dump", "NewPresetCode.txt", "Text Files (*.txt)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "m_presets[X] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));\n\n";
        for (int d = 0; d < 13; ++d) {
            bool hasNotes = false; QString drumCode = "// " + m_drums[d].name + "\n";
            for (int s = 0; s < 64; ++s) {
                if (m_velocities[d][s] > 0.0f) { hasNotes = true; drumCode += QString("m_presets[X][%1][%2] = %3f;\n").arg(d).arg(s).arg(m_velocities[d][s], 0, 'f', 2); }
            }
            if (hasNotes) stream << drumCode << "\n";
        }
        file.close(); QMessageBox::information(this, "", "C++ Code dumped!");
    }
}
void HouseBeatGenerator::onDuplicate16Clicked() {
    for (int r = 0; r < 13; ++r) {
        for (int c = 0; c < 16; ++c) {
            float val = m_velocities[r][c];
            m_velocities[r][c + 16] = val;
            m_velocities[r][c + 32] = val;
            m_velocities[r][c + 48] = val;
        }
    }

    for (int r = 0; r < 13; ++r) {
        for (int c = 16; c < 64; ++c) {
            updateCell(r, c);
        }
    }
}

void HouseBeatGenerator::onDevLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(this, "Load LMMS Project", "", "LMMS Project (*.mmp)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open .mmp file.");
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        QMessageBox::warning(this, "Error", "Could not parse XML structure. Is this a valid .mmp file?");
        file.close();
        return;
    }
    file.close();


    for (int r = 0; r < m_drums.size(); ++r) {
        for (int c = 0; c < 64; ++c) {
            m_velocities[r][c] = 0.0f;
            updateCell(r, c);
        }
    }


    QDomNodeList tracks = doc.elementsByTagName("track");
    for (int i = 0; i < tracks.count(); ++i) {
        QDomElement trackElem = tracks.at(i).toElement();
        QString trackName = trackElem.attribute("name");


        int drumIdx = -1;
        for (int d = 0; d < m_drums.size(); ++d) {
            if (m_drums[d].name == trackName) {
                drumIdx = d;
                break;
            }
        }


        if (drumIdx == -1) continue;


        QDomNodeList notes = trackElem.elementsByTagName("note");
        for (int j = 0; j < notes.count(); ++j) {
            QDomElement noteElem = notes.at(j).toElement();
            int pos = noteElem.attribute("pos").toInt();
            int vol = noteElem.attribute("vol").toInt();
            int len = noteElem.attribute("len").toInt();


            int step = qRound(pos / 12.0f) % 64;

            if (step >= 0 && step < 64) {
                float mappedVelocity = 1.0f;


                if (vol <= 85) mappedVelocity = 0.72f;


                if (len > 0 && len < 12) mappedVelocity = 2.0f;

                m_velocities[drumIdx][step] = mappedVelocity;
                updateCell(drumIdx, step);
            }
        }
    }


    int currentIndex = m_presetCombo->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_presets.size()) {
        m_presets[currentIndex] = m_velocities;
    }

    QMessageBox::information(this, "Success", "MMP pattern loaded and snapped to grid!");
}
void HouseBeatGenerator::onRandomDeepHouseClicked()
{
    m_velocities = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    auto rng = QRandomGenerator::global();


    for (int i = 0; i < 64; i += 4) {
        m_velocities[0][i] = 1.0f;
    }
    if (rng->bounded(100) < 35) m_velocities[0][28] = 0.0f;


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        m_velocities[1][b + 4] = 1.0f;
        m_velocities[1][b + 12] = 1.0f;
        if (rng->bounded(100) < 45) m_velocities[1][b + 2] = 0.68f;  // ghost
        if (rng->bounded(100) < 30) m_velocities[1][b + 10] = 0.72f; // ghost
    }


    for (int i = 1; i < 64; i += 2) {
        float vel = (i % 8 == 0) ? 0.95f : 0.55f + rng->generateDouble() * 0.35f;
        m_velocities[3][i] = vel;
    }
    for (int i = 0; i < 64; i += 4) {
        if (rng->bounded(100) < 65) m_velocities[3][i] = 0.45f;
    }


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        if (rng->bounded(100) < 80) m_velocities[4][b + 14] = 0.88f;
    }


    for (int i = 1; i < 64; i += 2) {
        if (rng->bounded(100) < 75) m_velocities[9][i] = 0.62f + rng->generateDouble() * 0.25f;
    }


    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        m_velocities[11][b + 2] = 0.75f;
        m_velocities[11][b + 6] = 0.68f;
        m_velocities[11][b + 10] = 0.82f;
        if (rng->bounded(100) < 60) m_velocities[11][b + 14] = 0.70f;
    }


    if (rng->bounded(100) < 70) {
        m_velocities[5][10] = 0.78f;
        m_velocities[5][42] = 0.75f;
    }


    if (rng->bounded(100) < 40) m_velocities[10][52] = 0.65f;


    for (int r = 0; r < 13; ++r) {
        for (int c = 0; c < 64; ++c) {
            updateCell(r, c);
        }
    }

    int idx = m_presetCombo->currentIndex();
    if (idx >= 0 && idx < (int)m_presets.size()) {
        m_presets[idx] = m_velocities;
    }
}
void HouseBeatGenerator::initializeBasslinePatterns() {
    m_basslinePatterns.clear();

    const int R  = 33;  // A1 (Root)
    const int M2 = 35;  // B1
    const int b3 = 36;  // C2 (Minor 3rd - Essential for deep house)
    const int p4 = 38;  // D2 (Perfect 4th)
    const int p5 = 39;  // E2 (Perfect 5th - Powerful anchor)
    const int M6 = 41;  // F#2
    const int b7 = 43;  // G2 (Minor 7th)
    const int O  = 45;  // A2 (Octave)
    const int _  = -1;  // Rest

    // Pattern 1:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,O,_, _,_,_,_, b3,_,_,_,
        _,_,p5,_, _,_,_,_, R,_,_,_, _,_,b7,_,
        R,_,_,_, _,_,O,_, _,_,_,_, b3,_,_,_,
        _,_,p5,_, _,_,_,_, R,_,_,_, _,O,_,_
    });

    // Pattern 2:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,_,_, _,_,p5,_, _,_,_,_,
        _,_,R,_, _,_,_,_, _,_,p5,_, _,_,b7,_,
        _,_,R,_, _,_,_,_, _,_,p5,_, _,_,_,_,
        _,_,R,_, _,_,_,_, _,_,p5,_, _,_,O,_
    });

    // Pattern 3:
    m_basslinePatterns.push_back({
        R,_,_,_, _,b3,_,_, _,_,p4,_, p5,_,_,_,
        _,_,_,_, b7,_,_,_, O,_,_,_, _,_,_,_,
        R,_,_,_, _,b3,_,_, _,_,p4,_, p5,_,_,_,
        _,_,_,_, b7,_,_,_, p5,_,_,_, b3,_,_,_
    });

    // Pattern 4:
    m_basslinePatterns.push_back({
        _,_,_,_, R,_,_,_, _,_,_,_, p5,_,_,_,
        _,_,_,_, b3,_,_,_, _,_,_,_, b7,_,_,_,
        _,_,_,_, R,_,_,_, _,_,_,_, p5,_,_,_,
        _,_,_,_, O,_,_,_, _,_,_,_, b7,_,p5,_
    });

    // Pattern 5:
    m_basslinePatterns.push_back({
        _,R,_,_, R,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,p4,_, _,_,p5,_, _,_,_,_, _,_,_,_,
        _,R,_,_, R,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,_,_, _,_,b7,_, _,O,_,_
    });

    // Pattern 6:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        _,_,b3,_, _,_,p5,_, _,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        _,_,b7,_, _,_,_,_, p5,_,_,_, _,_,_,_
    });
    // Pattern 7:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,R,_, _,_,_,_, _,_,p4,_, _,_,_,_,
        _,_,R,_, _,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,R,_, _,_,_,_, _,_,b7,_, O,_,_,_
    });

    // Pattern 8:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        _,_,_,_, R,_,_,_, p5,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        _,_,_,_, b3,_,_,_, _,_,b7,_, _,_,_,_
    });

    // Pattern 9:
    m_basslinePatterns.push_back({
        R,_,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        b7,_,_,_, p5,_,_,_, p4,_,_,_, b3,_,_,_,
        R,_,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        O,_,_,_, b7,_,_,_, p5,_,_,_, _,_,_,_
    });

    // Pattern 10:
    m_basslinePatterns.push_back({
        _,R,_,_, _,R,_,_, _,R,_,_, _,R,_,_,
        _,b3,_,_, _,b3,_,_, _,b3,_,_, _,p5,_,_,
        _,R,_,_, _,R,_,_, _,R,_,_, _,R,_,_,
        _,b7,_,_, _,O,_,_, _,b7,_,_, _,p5,_,_
    });

    // Pattern 11:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,R,_, _,_,_,_, p5,_,_,_,
        _,_,_,_, _,_,_,_, R,_,_,R, _,_,_,_,
        R,_,_,_, _,_,R,_, _,_,_,_, p5,_,_,_,
        _,_,_,_, _,_,O,_, b7,_,p5,_, b3,_,_,_
    });

    // Pattern 12:
    m_basslinePatterns.push_back({
        O,_,_,_, _,_,R,_, _,_,_,_, _,_,R,_,
        _,_,p5,_, _,_,_,_, _,_,b3,_, _,_,_,_,
        O,_,_,_, _,_,R,_, _,_,_,_, _,_,R,_,
        _,_,b7,_, _,_,O,_, _,_,_,_, p5,_,_,_
    });

    // Pattern 13:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,R,_, _,_,R,_, _,_,R,_,
        _,_,R,_, _,_,R,_, _,_,p5,_, _,_,_,_,
        _,_,R,_, _,_,R,_, _,_,R,_, _,_,R,_,
        _,_,R,_, _,_,b3,_, _,_,b7,_, _,_,O,_
    });

    // Pattern 14:
    m_basslinePatterns.push_back({
        b3,_,_,_, _,_,_,_, p4,_,_,_, _,_,_,_,
        b3,_,_,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        b3,_,_,_, _,_,_,_, p4,_,_,_, _,_,_,_,
        _,_,_,_, p5,_,_,_, b7,_,_,_, R,_,_,_
    });

    // Pattern 15:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,b3,_, _,_,_,_, p4,_,_,_,
        _,_,M6,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        R,_,_,_, _,_,b3,_, _,_,_,_, p4,_,_,_,
        _,_,M6,_, _,_,_,_, O,_,_,_, b7,_,_,_
    });

    // Pattern 16:
    m_basslinePatterns.push_back({
        R,_,_,R, _,_,p5,_, _,_,R,_, _,_,b3,_,
        R,_,_,R, _,_,p5,_, _,_,b7,_, _,_,_,_,
        R,_,_,R, _,_,p5,_, _,_,R,_, _,_,b3,_,
        _,_,p4,_, _,_,p5,_, _,_,b7,_, O,_,_,_
    });
    // Pattern 17:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,_,_, p5,_,_,_,
        R,_,_,_, _,_,_,_, _,_,_,_, p5,_,_,_,
        R,_,_,_, _,_,_,_, _,_,_,_, p5,_,_,_,
        R,_,_,_, _,_,_,_, _,_,b7,_, O,_,_,_
    });

    // Pattern 18:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,O,_, _,_,_,_, _,_,_,_,
        R,_,_,_, _,_,O,_, _,_,_,_, b7,_,p5,_,
        R,_,_,_, _,_,O,_, _,_,_,_, _,_,_,_,
        _,_,b3,_, _,_,R,_, _,_,b7,_, _,_,_,_
    });

    // Pattern 19:
    m_basslinePatterns.push_back({
        _,_,R,_, R,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,_,_, _,_,p4,_, _,_,_,_,
        _,_,R,_, R,_,_,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,_,_, b7,_,_,_, O,_,_,_
    });

    // Pattern 20:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_,
        p4,_,_,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_,
        b7,_,_,_, _,_,_,_, O,_,_,_, _,_,_,_
    });

    // Pattern 21:
    m_basslinePatterns.push_back({
        _,_,_,R, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,b3, _,_,_,_, _,_,p5,_, _,_,_,_,
        _,_,_,R, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,b7, _,_,_,_, _,_,O,_, _,_,_,_
    });

    // Pattern 22:
    m_basslinePatterns.push_back({
        p5,_,_,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        p4,_,_,_, _,_,_,_, p4,_,_,_, _,_,_,_,
        b3,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_,
        R,_,_,_, _,_,O,_, _,_,b7,_, p5,_,b3,_
    });
    // Pattern 23:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,_, R,_,_,_, _,_,_,_, p5,_,_,_,
        R,_,_,_, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,_, R,_,_,_, p4,_,_,_, b3,_,_,_
    });

    // Pattern 24:
    m_basslinePatterns.push_back({
        _,R,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        _,_,_,_, b7,_,_,_, p5,_,_,_, _,_,_,_,
        _,R,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        _,_,_,_, O,_,_,_, b7,_,p5,_, b3,_,_,_
    });

    // Pattern 25:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, b7,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, p4,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, b7,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_
    });

    // Pattern 26:
    m_basslinePatterns.push_back({
        R,_,O,_, R,_,p5,_, _,_,b7,_, _,_,_,_,
        R,_,O,_, R,_,p5,_, _,_,b3,_, _,_,_,_,
        R,_,O,_, R,_,p5,_, _,_,b7,_, _,_,_,_,
        R,_,O,_, R,_,_,_, p5,_,b3,_, _,_,_,_
    });

    // Pattern 27:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        O,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        b7,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_
    });

    // Pattern 28:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,R,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,_,_, _,_,p4,_, _,_,_,_,
        _,_,R,_, _,_,R,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,b7,_, _,_,O,_, _,_,_,_
    });
    // Pattern 23:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,_, R,_,_,_, _,_,_,_, p5,_,_,_,
        R,_,_,_, _,_,_,_, _,_,R,_, _,_,_,_,
        _,_,_,_, R,_,_,_, p4,_,_,_, b3,_,_,_
    });

    // Pattern 24:
    m_basslinePatterns.push_back({
        _,R,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        _,_,_,_, b7,_,_,_, p5,_,_,_, _,_,_,_,
        _,R,_,_, b3,_,_,_, p4,_,_,_, p5,_,_,_,
        _,_,_,_, O,_,_,_, b7,_,p5,_, b3,_,_,_
    });

    // Pattern 25:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, b7,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, p4,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, b7,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_
    });

    // Pattern 26:
    m_basslinePatterns.push_back({
        R,_,O,_, R,_,p5,_, _,_,b7,_, _,_,_,_,
        R,_,O,_, R,_,p5,_, _,_,b3,_, _,_,_,_,
        R,_,O,_, R,_,p5,_, _,_,b7,_, _,_,_,_,
        R,_,O,_, R,_,_,_, p5,_,b3,_, _,_,_,_
    });

    // Pattern 27:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        O,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        b7,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,
        p5,_,_,_, _,_,_,_, b3,_,_,_, _,_,_,_
    });

    // Pattern 28:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,R,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,_,_, _,_,p4,_, _,_,_,_,
        _,_,R,_, _,_,R,_, _,_,b3,_, _,_,_,_,
        _,_,p5,_, _,_,b7,_, _,_,O,_, _,_,_,_
    });
    // Pattern 29:
    m_basslinePatterns.push_back({
        _,_,R,_, _,_,b3,_, _,_,R,_, _,_,b7,_,
        _,_,p5,_, _,_,_,_, _,_,p4,_, _,_,_,_,
        _,_,R,_, _,_,b3,_, _,_,R,_, _,_,b7,_,
        _,_,p5,_, _,_,_,_, _,_,b3,_, _,_,_,_
    });

    // Pattern 30:
    m_basslinePatterns.push_back({
        R,_,_,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        _,_,_,_, R,_,_,_, b3,_,_,_, _,_,_,_,
        R,_,_,_, _,_,_,_, p5,_,_,_, _,_,_,_,
        _,_,_,_, b7,_,_,_, O,_,_,_, p5,_,b3,_
    });

}



void HouseBeatGenerator::appendBasslineTrackToMMP(QDomDocument &doc, QDomElement &trackContainer, int songBars) {
    int patternIndex = m_basslineSelector->currentIndex();
    if (patternIndex < 0 || patternIndex >= m_basslinePatterns.size()) return;

    const std::vector<int>& pattern = m_basslinePatterns[patternIndex];
    int stepsInPattern = pattern.size();
    int barsInPattern = stepsInPattern / 16;


    int loops = songBars / barsInPattern;
    if (loops == 0) loops = 1;

    QDomElement track = doc.createElement("track");
    track.setAttribute("type", "0");
    track.setAttribute("name", "Deep House Sub");
    track.setAttribute("muted", "0");
    track.setAttribute("solo", "0");
    trackContainer.appendChild(track);


    QDomElement instrTrack = doc.createElement("instrumenttrack");
    instrTrack.setAttribute("vol", "100");
    instrTrack.setAttribute("pan", "0");
    instrTrack.setAttribute("basenote", "57"); // Standard LMMS root alignment
    track.appendChild(instrTrack);

    QDomElement instr = doc.createElement("instrument");
    instr.setAttribute("name", "xpressive");
    instrTrack.appendChild(instr);


    QDomElement xp = doc.createElement("xpressive");
    xp.setAttribute("version", "0.1");

    QString stingrayFormula = "((  1.0 * sinew(integrate(f)) * exp(-t*0.5) +   0.6 * sinew(integrate(f*2)) * exp(-t*1.5) +   0.3 * sinew(integrate(f*3)) * exp(-t*6.0) +   0.15 * sinew(integrate(f*4)) * exp(-t*12.0) +  0.05 * sinew(integrate(f*8)) * exp(-t*20.0)) * v)";

    xp.setAttribute("O1", stingrayFormula);
    instr.appendChild(xp);


    QDomElement eldata = doc.createElement("eldata");
    QDomElement elvol = doc.createElement("elvol");
    elvol.setAttribute("att", "0");
    elvol.setAttribute("dec", "0.3");
    elvol.setAttribute("sus", "0");
    elvol.setAttribute("rel", "0.1");
    elvol.setAttribute("amt", "1");
    eldata.appendChild(elvol);
    instrTrack.appendChild(eldata);

    int speedMultiplier = 4; // Set to 2 for double speed, 4 for quadruple speed
    int tickSpacing = 48 / speedMultiplier;
    int noteLength = 36 / speedMultiplier; // Shorten the note length so they don't overlap when sped up


    for (int l = 0; l < loops; ++l) {
        QDomElement patternElem = doc.createElement("pattern");
        int patternPosOffset = l * barsInPattern * 768;

        patternElem.setAttribute("pos", QString::number(patternPosOffset));
        patternElem.setAttribute("type", "0"); // type="0" is Piano Roll / Main Editor
        patternElem.setAttribute("len", QString::number(barsInPattern * 768)); // Absolute length in ticks
        track.appendChild(patternElem);


        for (int repeat = 0; repeat < speedMultiplier; ++repeat) {
            int repeatOffset = repeat * (stepsInPattern * tickSpacing);

            for (int step = 0; step < stepsInPattern; ++step) {
                if (pattern[step] != -1) {
                    QDomElement note = doc.createElement("note");

                    int notePos = repeatOffset + (step * tickSpacing);

                    note.setAttribute("pos", QString::number(notePos));
                    note.setAttribute("key", QString::number(pattern[step]));
                    note.setAttribute("vol", "100");
                    note.setAttribute("len", QString::number(noteLength));

                    patternElem.appendChild(note);
                }
            }
        }
    }

}
void HouseBeatGenerator::appendPianoTriadsToMMP(QDomDocument &doc, QDomElement &trackContainer, int songBars)
{
    int patternIndex = m_basslineSelector->currentIndex();
    if (patternIndex < 0 || patternIndex >= m_basslinePatterns.size()) return;

    const std::vector<int>& pattern = m_basslinePatterns[patternIndex];
    int stepsInPattern = pattern.size();
    int barsInPattern = stepsInPattern / 16;

    int loops = songBars / barsInPattern;
    if (loops == 0) loops = 1;

    QDomElement track = doc.createElement("track");
    track.setAttribute("type", "0");
    track.setAttribute("name", "Deep House Piano Triads");
    track.setAttribute("muted", "0");
    track.setAttribute("solo", "0");
    trackContainer.appendChild(track);

    QDomElement instrTrack = doc.createElement("instrumenttrack");
    instrTrack.setAttribute("vol", "85");
    instrTrack.setAttribute("pan", "8");
    instrTrack.setAttribute("basenote", "69");
    track.appendChild(instrTrack);

    QDomElement instr = doc.createElement("instrument");
    instr.setAttribute("name", "xpressive");
    instrTrack.appendChild(instr);

    QDomElement xp = doc.createElement("xpressive");
    xp.setAttribute("version", "0.1");
    // Bright piano/rhodes-style sound perfect for deep house
    QString pianoFormula = "((sinew(integrate(f)) + 0.65*sinew(integrate(f*2)) + 0.35*sinew(integrate(f*4)) + 0.2*sinew(integrate(f*8))) * exp(-t*14) * v)";
    xp.setAttribute("O1", pianoFormula);
    instr.appendChild(xp);


    QDomElement eldata = doc.createElement("eldata");
    QDomElement elvol = doc.createElement("elvol");
    elvol.setAttribute("att", "0.01");
    elvol.setAttribute("dec", "0.4");
    elvol.setAttribute("sus", "0.15");
    elvol.setAttribute("rel", "0.6");
    elvol.setAttribute("amt", "1");
    eldata.appendChild(elvol);
    instrTrack.appendChild(eldata);

    int speedMultiplier = 4;
    int tickSpacing = 48 / speedMultiplier;
    int noteLength = 36 / speedMultiplier;

    for (int l = 0; l < loops; ++l) {
        QDomElement patternElem = doc.createElement("pattern");
        int patternPosOffset = l * barsInPattern * 768;
        patternElem.setAttribute("pos", QString::number(patternPosOffset));
        patternElem.setAttribute("type", "0");
        patternElem.setAttribute("len", QString::number(barsInPattern * 768));
        track.appendChild(patternElem);

        for (int repeat = 0; repeat < speedMultiplier; ++repeat) {
            int repeatOffset = repeat * (stepsInPattern * tickSpacing);

            for (int step = 0; step < stepsInPattern; ++step) {
                int root = pattern[step];
                if (root != -1) {

                    int notes[3] = { root + 24, root + 27, root + 31 };

                    for (int n = 0; n < 3; ++n) {
                        QDomElement note = doc.createElement("note");
                        note.setAttribute("pos", QString::number(repeatOffset + (step * tickSpacing)));
                        note.setAttribute("key", QString::number(notes[n]));
                        note.setAttribute("vol", "92");
                        note.setAttribute("len", QString::number(noteLength));
                        patternElem.appendChild(note);
                    }
                }
            }
        }
    }
}
