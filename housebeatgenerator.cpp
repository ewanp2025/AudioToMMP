#include "housebeatgenerator.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QScrollArea>
#include <QRandomGenerator>
#include <QApplication>
#include <QDebug>

HouseBeatGenerator::HouseBeatGenerator(QWidget *parent) : QWidget(parent)
{
    m_drums = {
        {"Kick", "((t<0.006)*sinew(2*pi*6800*t)*exp(-t*1250) + sinew(integrate(f*(52/440)*(1+A2*0.85*exp(-t*58)))) * exp(-t*(3.1+A1*13)) * (0.88 + 0.12*(v>0.65)))", "A1=0.28 A2=0.65 A3=0.18", 200, 0, false},
        {"Snare", "(  randsv(t * srate, 0) * exp(-t * (A1 * 60 + 10)) * (t < 0.04) + sinew(integrate(200)) * exp(-t * (A1 * 40 + 10)))", "A1=0.65 A2=0.45 A3=0.55", 180, 0, false},
        {"Clap", "(t<0.003)*sinew(2*pi*3100*t)*exp(-t*2800) + randsv(t*srate*1.8,0)*exp(-t*32)*(t<0.055) + randsv(t*srate*0.9,0)*exp(-t*26)*(t<0.11)", "A1=0.72 A2=0.38 A3=0.65", 155, 12, false},
        {"ClosedHat", "randsv(t*srate*(2+A2*3),0)*exp(-t*(18+A1*28))*(t<0.042)", "A1=0.35 A2=0.55 A3=0.75", 105, -18, true},
        {"OpenHat", "randsv(t*srate*(1.6+A2*2.2),0)*exp(-t*(9+A1*14))*(t<0.135)", "A1=0.82 A2=0.45 A3=0.65", 92, 18, true},
        {"RimShot", "(t<0.007)*sinew(2*pi*2800*t)*exp(-t*1350) + sinew(integrate(380*(1+A2*0.45*exp(-t*38))))*exp(-t*(A1*13+4))*0.65", "A1=0.45 A2=0.72 A3=0.28", 125, -8, false},
        {"LowTom", "sinew(integrate(f*(92/440)*(1+A2*1.1*exp(-t*28))))*exp(-t*(5+A1*16))", "A1=0.55 A2=0.75 A3=0.22", 140, 10, false},
        {"MidTom", "sinew(integrate(f*(138/440)*(1+A2*0.95*exp(-t*32))))*exp(-t*(4.8+A1*14))", "A1=0.48 A2=0.82 A3=0.25", 135, 5, false},
        {"HighTom", "sinew(integrate(f*(185/440)*(1+A2*1.05*exp(-t*35))))*exp(-t*(4.2+A1*12))", "A1=0.42 A2=0.88 A3=0.20", 130, 0, false},
        {"Maracas", "randsv(t*srate*(4+A2*2),0)*exp(-t*(A1*65+12))*(t<0.035)", "A1=0.42 A2=0.65 A3=0.88", 82, 25, true},
        {"Cowbell", "(t<0.008)*sinew(2*pi*2400*t)*exp(-t*920) + sinew(integrate(520*(1+A2*0.6*exp(-t*28))))*exp(-t*(A1*18+6))*0.75", "A1=0.38 A2=0.68 A3=0.45", 95, -12, false},
        {"Conga", "sinew(integrate(f*(110/440)*(1+A2*0.9*exp(-t*25))))*exp(-t*(6+A1*15)) + randsv(t*srate*2.5,0)*exp(-t*(A1*22+8))*(t<0.045)", "A1=0.52 A2=0.78 A3=0.35", 110, 15, false}
    };

    createPresets();
    setupUI();
    applyPreset(0); // default = Pattern A
}

HouseBeatGenerator::~HouseBeatGenerator() {}

void HouseBeatGenerator::setupUI()
{
    QVBoxLayout *main = new QVBoxLayout(this);

    QHBoxLayout *top = new QHBoxLayout();
    m_presetCombo = new QComboBox();
    m_presetCombo->addItems({
        "Pattern A",
        "Pattern B",
        "Pattern C",
        "Pattern D",
        "Pattern E",
        "Pattern F",
        "Pattern G",
        "Pattern H",
        "Pattern I",
        "Pattern J",
        "Pattern K",
        "Pattern L",
        "Pattern M"
    });
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HouseBeatGenerator::onPresetChanged);

    m_bpmSpin = new QSpinBox(); m_bpmSpin->setRange(100,140); m_bpmSpin->setValue(123); m_bpmSpin->setSuffix(" BPM");

    m_shuffleDial = new QSpinBox();
    m_shuffleDial->setRange(1,7);
    m_shuffleDial->setValue(1);
    m_shuffleDial->setSuffix(" (1=no swing, 7=max)");
    connect(m_shuffleDial, QOverload<int>::of(&QSpinBox::valueChanged), this, &HouseBeatGenerator::onSwingGlobalChanged);

    m_ghostIntensity = new QDoubleSpinBox(); m_ghostIntensity->setRange(30,100); m_ghostIntensity->setValue(72); m_ghostIntensity->setSuffix("% Ghost Vol");

    m_btnExport = new QPushButton("Export to .mmp (64-step Beat/Bassline)");
    m_btnExport->setStyleSheet("background-color:#00557f;color:white;font-weight:bold;padding:8px;");
    connect(m_btnExport, &QPushButton::clicked, this, &HouseBeatGenerator::onExportMMPClicked);

    top->addWidget(new QLabel("Preset:")); top->addWidget(m_presetCombo);
    top->addWidget(new QLabel("BPM:")); top->addWidget(m_bpmSpin);
    top->addWidget(new QLabel("Shuffle:")); top->addWidget(m_shuffleDial);
    top->addWidget(new QLabel("Ghost Vol:")); top->addWidget(m_ghostIntensity);
    top->addStretch(); top->addWidget(m_btnExport);
    main->addLayout(top);

    m_grid = new QTableWidget(12, 64, this);
    QStringList drumNames; for (auto& d : m_drums) drumNames << d.name;
    m_grid->setVerticalHeaderLabels(drumNames);
    m_grid->setHorizontalHeaderLabels({"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40","41","42","43","44","45","46","47","48","49","50","51","52","53","54","55","56","57","58","59","60","61","62","63","64"});

    for (int c = 0; c < 64; ++c) m_grid->setColumnWidth(c, 26);
    m_grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    connect(m_grid, &QTableWidget::cellClicked, this, &HouseBeatGenerator::onGridCellClicked);

    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidget(m_grid);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMinimumHeight(420);
    main->addWidget(m_scrollArea);

    QHBoxLayout *rand = new QHBoxLayout();
    QPushButton *b1 = new QPushButton("Random Snare Build"); QPushButton *b2 = new QPushButton("Random Snare Pattern"); QPushButton *b3 = new QPushButton("Random Rimshot Pattern");
    rand->addWidget(b1); rand->addWidget(b2); rand->addWidget(b3);
    main->addLayout(rand);

    connect(b1, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomSnareBuild);
    connect(b2, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomSnarePattern);
    connect(b3, &QPushButton::clicked, this, &HouseBeatGenerator::onRandomRimshotPattern);
}

void HouseBeatGenerator::onGridCellClicked(int row, int col)
{
    if (QApplication::mouseButtons() & Qt::RightButton) {
        float v = m_velocities[row][col];
        m_velocities[row][col] = (v == 0.0f) ? 0.72f : (v == 0.72f ? 1.0f : 0.0f);
    } else {
        m_velocities[row][col] = (m_velocities[row][col] == 1.0f) ? 0.0f : 1.0f;
    }
    updateCell(row, col);
}

void HouseBeatGenerator::updateCell(int row, int col)
{
    float v = m_velocities[row][col];
    QTableWidgetItem *item = m_grid->item(row, col);
    if (!item) {
        item = new QTableWidgetItem();
        m_grid->setItem(row, col, item);
    }
    if (v == 0.0f) {
        item->setBackground(QColor(40,40,40));
        item->setText("");
    } else if (v < 0.9f) {
        item->setBackground(QColor(100,180,255));
        item->setText("g");
    } else {
        item->setBackground(QColor(0,200,100));
        item->setText("");
    }
}

void HouseBeatGenerator::createPresets()
{
    m_presets.resize(14);

    // Pattern A
    m_presets[0] = std::vector<std::vector<float>>(12, std::vector<float>(64, 0.0f));
    for (int i = 0; i < 16; ++i) m_presets[0][0][i*4] = 1.0f;
    for (int i = 0; i < 16; ++i) m_presets[0][1][i*4 + 2] = 1.0f;
    for (int i = 0; i < 64; ++i) m_presets[0][3][i] = 1.0f;

    // Pattern B
    m_presets[1] = std::vector<std::vector<float>>(12, std::vector<float>(64, 0.0f));

    // Kick every beat
    for (int i = 0; i < 16; ++i) m_presets[1][0][i*4] = 1.0f;

    // Snare on 2 & 4
    for (int i = 0; i < 16; ++i) m_presets[1][1][i*4 + 2] = 1.0f;

    // ClosedHat - EXACT positions (setting 1 = no swing)
    int closedHatSteps[] = {0,1,4,7,8,12,13,15,16,17,20,23,24,28,29,31,32,33,36,39,40,44,45,47,48,49,52,55,56,60,61,63};
    for (int step : closedHatSteps) {
        if (step < 64) m_presets[1][3][step] = 1.0f;
    }

    m_presets[1][4][6] = m_presets[1][4][22] = m_presets[1][4][38] = m_presets[1][4][54] = 1.0f;
    m_presets[1][5][12] = m_presets[1][5][60] = 0.72f;

    // Next presets
    for (int i = 2; i < 14; ++i) {
        m_presets[i] = std::vector<std::vector<float>>(12, std::vector<float>(64, 0.0f));
        for (int s = 0; s < 16; ++s) {
            m_presets[i][0][s*4] = 1.0f;
            m_presets[i][1][s*4 + 2] = 1.0f;
            m_presets[i][3][s*4] = 1.0f;
        }
    }
}

void HouseBeatGenerator::applyPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    m_velocities = m_presets[index];

    for (int r = 0; r < 12; ++r)
        for (int c = 0; c < 64; ++c)
            updateCell(r, c);
}

void HouseBeatGenerator::onPresetChanged(int index) { applyPreset(index); }

void HouseBeatGenerator::onSwingGlobalChanged()
{

    for (int r = 0; r < 12; ++r)
        for (int c = 0; c < 64; ++c)
            updateCell(r, c);
}

void HouseBeatGenerator::onExportMMPClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "Save House Beat", "HouseBeat_64step.mmp", "LMMS Project (*.mmp)");
    if (path.isEmpty()) return;

    buildMMP(path);
    QMessageBox::information(this, "Success", "House Beat saved!\n");
}

void HouseBeatGenerator::buildMMP(const QString &filePath)
{
    int shuffleSetting = m_shuffleDial->value(); // 1-7

    QDomDocument doc;
    QDomElement root = doc.createElement("lmms-project");
    root.setAttribute("version", "20");
    root.setAttribute("creator", "LMMS");
    root.setAttribute("creatorversion", "1.3.0-alpha.1.102+g89fc6c960");
    root.setAttribute("type", "song");
    doc.appendChild(root);

    QDomElement head = doc.createElement("head");
    head.setAttribute("bpm", QString::number(m_bpmSpin->value()));
    head.setAttribute("timesig_numerator", "4");
    head.setAttribute("timesig_denominator", "4");
    head.setAttribute("mastervol", "100");
    root.appendChild(head);

    QDomElement song = doc.createElement("song");
    root.appendChild(song);

    QDomElement trackContainer = doc.createElement("trackcontainer");
    trackContainer.setAttribute("type", "song");
    trackContainer.setAttribute("height", "620");
    trackContainer.setAttribute("width", "1320");
    song.appendChild(trackContainer);

    QDomElement bbTrack = doc.createElement("track");
    bbTrack.setAttribute("type", "1");
    bbTrack.setAttribute("name", "House Beat 64-step");
    trackContainer.appendChild(bbTrack);

    QDomElement bbContainer = doc.createElement("bbtrack");
    bbTrack.appendChild(bbContainer);

    QDomElement innerContainer = doc.createElement("trackcontainer");
    innerContainer.setAttribute("type", "bbtrackcontainer");
    bbContainer.appendChild(innerContainer);

    double swingFactor = (shuffleSetting - 1) / 6.0; // 0.0 to 1.0
    int maxSwingTicks = 6;

    for (int d = 0; d < 12; ++d) {
        QDomElement drumTrack = doc.createElement("track");
        drumTrack.setAttribute("type", "0");
        drumTrack.setAttribute("name", m_drums[d].name);
        innerContainer.appendChild(drumTrack);

        QDomElement instrTrack = doc.createElement("instrumenttrack");
        instrTrack.setAttribute("vol", QString::number(m_drums[d].defaultVol));
        instrTrack.setAttribute("pan", QString::number(m_drums[d].defaultPan));
        drumTrack.appendChild(instrTrack);

        QDomElement instr = doc.createElement("instrument");
        instr.setAttribute("name", "xpressive");
        instrTrack.appendChild(instr);

        QDomElement xp = doc.createElement("xpressive");
        xp.setAttribute("version", "0.1");
        xp.setAttribute("W1sample", "AA");
        xp.setAttribute("O1", m_drums[d].xpressiveO1);
        instr.appendChild(xp);

        QDomElement pattern = doc.createElement("pattern");
        pattern.setAttribute("steps", "64");
        pattern.setAttribute("pos", "0");
        pattern.setAttribute("type", "0");
        drumTrack.appendChild(pattern);

        for (int step = 0; step < 64; ++step) {
            float v = m_velocities[d][step];
            if (v > 0.0f) {
                int pos = step * 12;

                if (m_drums[d].swingEnabledByDefault && (step % 2 == 1)) {
                    int swingTicks = (int)(swingFactor * maxSwingTicks);
                    pos += swingTicks;
                }

                int vol = (int)(v * 100);
                if (v < 0.9f) {
                    double ghostScale = m_ghostIntensity->value() / 100.0;
                    vol = (int)(vol * ghostScale);
                    if (vol < 1) vol = 1;
                }

                QDomElement note = doc.createElement("note");
                note.setAttribute("key", "69");
                note.setAttribute("vol", QString::number(vol));
                note.setAttribute("len", "-192");
                note.setAttribute("pos", QString::number(pos));
                pattern.appendChild(note);
            }
        }
    }

    QDomElement bbtco = doc.createElement("bbtco");
    bbtco.setAttribute("len", "768");
    bbtco.setAttribute("pos", "0");
    bbTrack.appendChild(bbtco);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << doc.toString(2);
        file.close();
    }
}

void HouseBeatGenerator::onRandomSnareBuild() { applyPreset(m_presetCombo->currentIndex()); }
void HouseBeatGenerator::onRandomSnarePattern() { applyPreset(m_presetCombo->currentIndex()); }
void HouseBeatGenerator::onRandomRimshotPattern() { applyPreset(m_presetCombo->currentIndex()); }
