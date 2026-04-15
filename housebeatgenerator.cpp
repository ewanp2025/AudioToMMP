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

    m_shuffleDial = new QSpinBox(); m_shuffleDial->setRange(1,7); m_shuffleDial->setValue(1); m_shuffleDial->setSuffix(" Swing");
    connect(m_shuffleDial, QOverload<int>::of(&QSpinBox::valueChanged), this, &HouseBeatGenerator::onSwingGlobalChanged);

    m_ghostIntensity = new QDoubleSpinBox(); m_ghostIntensity->setRange(30,100); m_ghostIntensity->setValue(72); m_ghostIntensity->setSuffix("% Ghost");

    topRow1->addWidget(new QLabel("Preset:")); topRow1->addWidget(m_presetCombo);
    topRow1->addWidget(new QLabel("BPM:")); topRow1->addWidget(m_bpmSpin);
    topRow1->addWidget(new QLabel("Song Length:")); topRow1->addWidget(m_songLengthSpin);
    topRow1->addWidget(new QLabel("Shuffle:")); topRow1->addWidget(m_shuffleDial);
    topRow1->addWidget(new QLabel("Ghost Vol:")); topRow1->addWidget(m_ghostIntensity);
    topRow1->addStretch();
    mainLayout->addLayout(topRow1);


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
    m_presets.resize(30, std::vector<std::vector<float>>(m_drums.size(), std::vector<float>(64, 0.0f)));


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


    m_presets[0] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int i = 0; i < 64; i += 4) { m_presets[0][0][i] = 1.0f; m_presets[0][1][i+2] = 1.0f; }
    for (int i = 0; i < 64; ++i) m_presets[0][3][i] = 1.0f;


    m_presets[1] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int i = 0; i < 64; i += 4) { m_presets[1][0][i] = 1.0f; m_presets[1][4][i+2] = 1.0f; }
    setNotes(1, 1, {4,12,20,28,36,44,52,60, 15,31,47,63});
    setNotes(1, 1, {59,62});
    setNotes(1, 3, {0,1,4,7,8,12,13,15,16,17,20,23,24,28,29,31,32,33,36,39,40,44,45,47,48,49,52,55,56,60,61,63});
    setNotes(1, 5, {12, 60}, 0.72f);


    m_presets[2] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for(int i = 0; i < 64; i += 4) m_presets[2][0][i] = 1.0f;
    setNotes(2, 1, {2,10,18,22,26,30,34,38,42,46,50,52,53,54,55,56,58,59,60,61});
    setNotes(2, 1, {3,7,11,15,63}, 0.72f);
    setNotes(2, 2, {2,6,10,14,18,22,26,30,34,38,42,46,58,60,61});
    setNotes(2, 3, {0,1,4,7,8,12,13,15,16,17,20,23,24,28,29,31,32,33,36,39,40,44,45,47,48,49,52,55,56,60,61,63});
    setNotes(2, 4, {3,7,10,11,15,19,23,27,31,35,39,43,47,51,55,59,63});
    setNotes(2, 5, {1,5,8,9,13,17,21,25,29,33,37,41,45,49,53,57,61});
    for(int i = 6; i < 64; i += 16) m_presets[2][6][i] = 1.0f;
    for(int i = 7; i < 64; i += 16) m_presets[2][7][i] = 1.0f;
    for(int i = 8; i < 64; i += 16) m_presets[2][8][i] = 1.0f;
    for(int i = 1; i < 64; i += 2) m_presets[2][9][i] = 1.0f;
    for(int b = 0; b < 64; b += 16) setNotes(2, 10, {b+2, b+3, b+6, b+8, b+10, b+13});
    for(int i = 3; i < 64; i += 4) m_presets[2][11][i] = 1.0f;


    m_presets[3] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int b = 0; b < 64; b += 16) {
        setNotes(3, 0, {b, b+4, b+8, b+12});
        setNotes(3, 1, {b+4, b+12});
        for (int i = 0; i < 16; ++i) m_presets[3][3][b + i] = 1.0f;
        setNotes(3, 4, {b+3}, 0.9f); setNotes(3, 4, {b+11}, 0.85f);
        setNotes(3, 6, {b+5}, 0.72f); setNotes(3, 6, {b+21}, 0.68f);
        setNotes(3, 11, {b+10}, 0.65f); setNotes(3, 11, {b+26}, 0.62f);
    }
    m_presets[3][12][63] = 1.0f;


    m_presets[4] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int i = 0; i < 64; i += 4) m_presets[4][0][i] = 1.0f;
    setNotes(4, 1, {4, 12, 20, 28, 34, 38, 42, 46, 50, 54, 58, 62});
    for (int i = 0; i < 64; ++i) m_presets[4][3][i] = 1.0f;
    for (int i = 7; i < 64; i += 16) setNotes(4, 4, {i, i+8});


    m_presets[5] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(5, 0, {0,4,8,12});
    setNotes(5, 1, {4,8,12});
    setNotes(5, 2, {8});
    for(int i=0; i<=14; i+=2) m_presets[5][3][i] = 1.0f;
    setNotes(5, 4, {5,11,15});
    setNotes(5, 5, {3,7,11,15});
    fill64(5);


    m_presets[6] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(6, 0, {0,3,6,8,11,14});
    setNotes(6, 1, {4,12});
    setNotes(6, 2, {8});
    for(int i=0; i<16; ++i) m_presets[6][3][i] = 1.0f;
    setNotes(6, 4, {7,11,15});
    setNotes(6, 5, {1,5,9,13});
    setNotes(6, 6, {10});
    fill64(6);


    m_presets[7] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(7, 0, {0,4,8,12});
    setNotes(7, 1, {4,12});
    setNotes(7, 2, {8});
    for(int i=0; i<16; ++i) m_presets[7][3][i] = 1.0f;
    setNotes(7, 4, {3,7,11,15});
    setNotes(7, 5, {15});
    fill64(7);


    m_presets[8] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(8, 0, {0,8,11,16,19,25,32});
    setNotes(8, 1, {1,2,9,10});
    setNotes(8, 2, {4,12,20,28,30});
    setNotes(8, 2, {6,9,15,23,27,29,31}, 0.72f);
    setNotes(8, 3, {0,1,3,4,5,7,8,9,11,12,13,15,16,17,19,20,21,23,24,25,27,28,29,31});
    for(int i=2; i<=30; i+=4) m_presets[8][4][i] = 1.0f;


    m_presets[9] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for(int i=0; i<64; i+=4) m_presets[9][0][i] = 1.0f;
    setNotes(9, 0, {10,26,42,58}, 0.72f);
    setNotes(9, 1, {4,7,9,12,15,20,23,25,28,31,36,39,41,44,47,52,55,57,60,63});
    for(int i=1; i<64; i+=2) m_presets[9][3][i] = 1.0f;
    for(int b=0; b<64; b+=16) setNotes(9, 4, {b+2, b+6, b+10, b+13});

      m_presets[10] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(10, 0, {0, 4, 8, 12});
    setNotes(10, 1, {4, 12});
    setNotes(10, 1, {7, 10, 15}, 0.65f);
    setNotes(10, 2, {4, 12}, 0.8f);
    setNotes(10, 3, {0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15}, 0.7f);
    setNotes(10, 4, {2, 6, 10, 14});
    setNotes(10, 6, {11, 14});
    setNotes(10, 7, {8});
    fill64(10);

    m_presets[11] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(11, 0, {0, 4, 8, 12});
    setNotes(11, 2, {4, 12});
    setNotes(11, 5, {3, 8, 11}, 0.85f);
    setNotes(11, 3, {2, 6, 10, 14}, 0.6f);
    setNotes(11, 4, {2, 6, 10, 14}, 0.9f);
    setNotes(11, 9, {0, 3, 6, 9, 12, 15}, 0.65f);
    fill64(11);
    setNotes(11, 0, {60, 63});
    setNotes(11, 5, {58, 61, 62});

    m_presets[12] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    setNotes(12, 0, {0, 4, 8, 12, 15});
    setNotes(12, 1, {4, 12});
    setNotes(12, 3, {1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15}, 0.65f);
    setNotes(12, 4, {2, 6, 10, 14}, 0.85f);
    setNotes(12, 10, {7, 15}, 0.75f);
    fill64(12);

    m_presets[13] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int s = 0; s < 16; ++s) {
        m_presets[13][0][s*4] = 1.0f;
        m_presets[13][1][s*4 + 2] = 1.0f;
        m_presets[13][3][s*4] = 1.0f;
    }


    m_presets[14] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(14, 0, {b+0, b+4, b+8, b+12});
        setNotes(14, 1, {b+4, b+12});
        setNotes(14, 2, {b+12}, 0.82f);


        for (int i = 0; i < 16; ++i) m_presets[14][3][b+i] = 0.88f;


        if (bar % 2 == 1) m_presets[14][4][b+14] = 0.92f;


        setNotes(14, 11, {b+2, b+6, b+10, b+14}, 0.78f);


        for (int i = 1; i < 16; i += 2) m_presets[14][9][b+i] = 0.62f;
    }


    m_presets[15] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(15, 0, {b+0, b+4, b+8, b+12});
        setNotes(15, 1, {b+4, b+12});
        setNotes(15, 2, {b+4, b+12}, 0.75f);


        for (int i = 0; i < 16; ++i) {
            m_presets[15][3][b+i] = (i % 4 == 0 || i % 4 == 2) ? 0.92f : 0.68f;
        }


        m_presets[15][4][b+14] = 0.88f;


        setNotes(15, 5, {b+2, b+10}, 0.72f);


        setNotes(15, 11, {b+1, b+5, b+9, b+13}, 0.82f);
        setNotes(15, 11, {b+3, b+7, b+11}, 0.58f);
        for (int i = 1; i < 16; i += 2) m_presets[15][9][b+i] = 0.65f;
    }


    m_presets[16] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;
        setNotes(16, 0, {b+0, b+4, b+8, b+12});
        setNotes(16, 1, {b+12});
        setNotes(16, 2, {b+4}, 0.85f);


        for (int i = 2; i < 16; i += 2) m_presets[16][3][b+i] = 0.85f;


        if (bar == 3) m_presets[16][4][b+14] = 0.95f;


        setNotes(16, 10, {b+6, b+14}, 0.55f);
        setNotes(16, 11, {b+3, b+11}, 0.78f);
    }


    m_presets[17] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));
    for (int bar = 0; bar < 4; ++bar) {
        int b = bar * 16;


        setNotes(17, 0, {b+0, b+4, b+8, b+12}, 0.86f);


        setNotes(17, 1, {b+4, b+12}, 0.90f);


        m_presets[17][3][b+0]  = 0.55f; // Vel 70
        m_presets[17][3][b+2]  = 0.78f; // Vel 100
        m_presets[17][3][b+4]  = 0.55f; // Vel 70
        m_presets[17][3][b+6]  = 0.82f; // Vel 105
        m_presets[17][3][b+8]  = 0.59f; // Vel 75
        m_presets[17][3][b+10] = 0.78f; // Vel 100
        m_presets[17][3][b+12] = 0.55f; // Vel 70
        m_presets[17][3][b+14] = 0.86f; // Vel 110


        m_presets[17][1][b+1]  = 0.20f;
        m_presets[17][5][b+3]  = 0.23f;
        m_presets[17][1][b+5]  = 0.31f;
        m_presets[17][4][b+7]  = 0.27f;
        m_presets[17][1][b+9]  = 0.15f;
        m_presets[17][6][b+15] = 0.31f;
    }

     m_presets[18] = std::vector<std::vector<float>>(13, std::vector<float>(64, 0.0f));


    for (int bar = 0; bar < 3; ++bar) {
        int b = bar * 16;
        setNotes(18, 0, {b+0, b+4, b+8, b+12}, 0.86f);
        setNotes(18, 1, {b+4, b+12}, 0.90f);
        setNotes(18, 4, {b+2, b+6, b+10, b+14}, 0.80f);
        for(int i = 0; i < 16; ++i) if(i % 4 != 2) m_presets[18][3][b+i] = 0.60f;
    }


    int b4 = 48;


    m_presets[18][0][b4+0] = 0.86f;
    m_presets[18][4][b4+2] = 0.80f;


    m_presets[18][0][b4+4] = 0.86f;
    m_presets[18][1][b4+4] = 0.90f;
    m_presets[18][4][b4+6] = 0.80f;


    m_presets[18][0][b4+8] = 0.86f;
    m_presets[18][8][b4+8] = 0.70f;
    m_presets[18][4][b4+10] = 0.80f;
    m_presets[18][7][b4+10] = 0.75f;
    m_presets[18][1][b4+11] = 0.86f;


    m_presets[18][6][b4+13] = 0.80f;
    m_presets[18][4][b4+14] = 0.80f;
    m_presets[18][2][b4+14] = 0.95f;
    m_presets[18][3][b4+15] = 0.31f;


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

        int drumVolId = baseVolId + d;
        if (m_automationLanes[d]->hasAutomation()) {
            instrTrack.setAttribute("id", QString::number(drumVolId));
        }

        drumTrack.appendChild(instrTrack);

        QDomElement instr = doc.createElement("instrument"); instr.setAttribute("name", "xpressive"); instrTrack.appendChild(instr);

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

    QDomElement bbtco = doc.createElement("bbtco");
    bbtco.setAttribute("len", QString::number(totalBars * 768)); bbtco.setAttribute("pos", "0");
    bbTrack.appendChild(bbtco);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file); stream << "<?xml version=\"1.0\"?>\n<!DOCTYPE lmms-project>\n" << doc.toString(2); file.close();
    }
}

void HouseBeatGenerator::onRandomSnareBuild() { applyPreset(m_presetCombo->currentIndex()); }
void HouseBeatGenerator::onRandomSnarePattern() { applyPreset(m_presetCombo->currentIndex()); }
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
