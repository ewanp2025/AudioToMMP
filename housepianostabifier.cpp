#include "housepianostabifier.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>
#include <QRandomGenerator>
#include <QFormLayout>
#include <map>
#include <algorithm>

HousePianoStabifier::HousePianoStabifier(QWidget *parent) : QWidget(parent)
{
    setupUI();
}

void HousePianoStabifier::setupUI()
{
    QVBoxLayout *main = new QVBoxLayout(this);

    QGroupBox *ctrlGroup = new QGroupBox("House Piano Stabifier 2.0");
    QVBoxLayout *ctrl = new QVBoxLayout(ctrlGroup);


    QHBoxLayout *chordLayout = new QHBoxLayout();


    QGroupBox *grpChord1 = new QGroupBox("Chord 1 (Anchor)");
    QFormLayout *form1 = new QFormLayout(grpChord1);
    m_comboRoot1 = new QComboBox();
    m_comboRoot1->addItems({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
    m_comboType1 = new QComboBox();
    m_comboType1->addItems({"m7", "m9", "m11", "Maj7", "Maj9", "add9", "dom7", "m (triad)", "Maj (triad)"});
    m_comboType1->setCurrentText("m9");
    m_spinPos1 = new QSpinBox(); m_spinPos1->setRange(1, 32); m_spinPos1->setValue(1);
    form1->addRow("Root:", m_comboRoot1);
    form1->addRow("Type:", m_comboType1);
    form1->addRow("Start Step:", m_spinPos1);


    QGroupBox *grpChord2 = new QGroupBox("Chord 2 (Response)");
    QFormLayout *form2 = new QFormLayout(grpChord2);
    m_comboRoot2 = new QComboBox();
    m_comboRoot2->addItems({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
    m_comboRoot2->setCurrentText("F");
    m_comboType2 = new QComboBox();
    m_comboType2->addItems({"m7", "m9", "m11", "Maj7", "Maj9", "add9", "dom7", "m (triad)", "Maj (triad)"});
    m_comboType2->setCurrentText("Maj9");
    m_spinPos2 = new QSpinBox(); m_spinPos2->setRange(1, 32); m_spinPos2->setValue(26);
    form2->addRow("Root:", m_comboRoot2);
    form2->addRow("Type:", m_comboType2);
    form2->addRow("Start Step:", m_spinPos2);

    chordLayout->addWidget(grpChord1);
    chordLayout->addWidget(grpChord2);
    ctrl->addLayout(chordLayout);


    QHBoxLayout *grooveLayout = new QHBoxLayout();

    QVBoxLayout *slideLayout1 = new QVBoxLayout();
    m_sliderRhythmDensity = new QSlider(Qt::Horizontal);
    m_sliderRhythmDensity->setRange(0, 100);
    m_sliderRhythmDensity->setValue(40);
    slideLayout1->addWidget(new QLabel("Rhythm Density (More Triggers):"));
    slideLayout1->addWidget(m_sliderRhythmDensity);

    QVBoxLayout *slideLayout2 = new QVBoxLayout();
    m_sliderVoicingThinning = new QSlider(Qt::Horizontal);
    m_sliderVoicingThinning->setRange(0, 100);
    m_sliderVoicingThinning->setValue(60);
    slideLayout2->addWidget(new QLabel("Voicing Thinning (Remove Notes):"));
    slideLayout2->addWidget(m_sliderVoicingThinning);

    QVBoxLayout *optLayout = new QVBoxLayout();
    m_chkTriadAnchors = new QCheckBox("Force Triads on Anchors");
    m_chkTriadAnchors->setChecked(true);

    m_comboGroove = new QComboBox();
    m_comboGroove->addItems({"Classic House", "UK Garage (Swung)"});

    optLayout->addWidget(m_chkTriadAnchors);
    optLayout->addWidget(m_comboGroove);

    grooveLayout->addLayout(slideLayout1);
    grooveLayout->addLayout(slideLayout2);
    grooveLayout->addLayout(optLayout);
    ctrl->addLayout(grooveLayout);


    QHBoxLayout *top = new QHBoxLayout();
    m_btnLoad = new QPushButton("Load .xpt (Disabled for Gen)");
    m_btnGenerate = new QPushButton("GENERATE 4-BAR STABS");
    m_btnGenerate->setStyleSheet("background-color:#c026d3; color:white; font-weight:bold; padding:12px; font-size:15px;");
    m_btnSave = new QPushButton("Save Stabbed .xpt");
    m_btnClear = new QPushButton("Clear");

    top->addWidget(m_btnLoad);
    top->addWidget(m_btnGenerate);
    top->addWidget(m_btnSave);
    top->addWidget(m_btnClear);
    ctrl->addLayout(top);


    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({"Step", "Key", "Pos", "Len", "Vel"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ctrl->addWidget(m_table);

    main->addWidget(ctrlGroup);


    connect(m_btnLoad, &QPushButton::clicked, this, &HousePianoStabifier::onLoadClicked);
    connect(m_btnGenerate, &QPushButton::clicked, this, &HousePianoStabifier::onGenerateStabsClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &HousePianoStabifier::onSaveClicked);
    connect(m_btnClear, &QPushButton::clicked, this, &HousePianoStabifier::onClearClicked);
}

std::vector<int> HousePianoStabifier::getChordNotes(int rootMidi, const QString& type)
{
    std::vector<int> notes = {rootMidi};

    if (type == "m7") notes.insert(notes.end(), {rootMidi + 3, rootMidi + 7, rootMidi + 10});
    else if (type == "m9") notes.insert(notes.end(), {rootMidi + 3, rootMidi + 7, rootMidi + 10, rootMidi + 14});
    else if (type == "m11") notes.insert(notes.end(), {rootMidi + 3, rootMidi + 7, rootMidi + 10, rootMidi + 14, rootMidi + 17});
    else if (type == "Maj7") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7, rootMidi + 11});
    else if (type == "Maj9") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7, rootMidi + 11, rootMidi + 14});
    else if (type == "add9") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7, rootMidi + 14});
    else if (type == "dom7") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7, rootMidi + 10});
    else if (type == "m (triad)") notes.insert(notes.end(), {rootMidi + 3, rootMidi + 7});
    else if (type == "Maj (triad)") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7});

    return notes;
}

std::vector<int> HousePianoStabifier::getTriad(const std::vector<int>& fullChord)
{
    if (fullChord.size() <= 3) return fullChord;
    return {fullChord[0], fullChord[1], fullChord[2]};
}

void HousePianoStabifier::onGenerateStabsClicked()
{
    m_notes.clear();

    int root1 = 60 + m_comboRoot1->currentIndex(); // C4 baseline
    int root2 = 60 + m_comboRoot2->currentIndex();
    int pos1 = m_spinPos1->value() - 1; // Convert to 0-indexed steps
    int pos2 = m_spinPos2->value() - 1;

    double rhythmDensity = m_sliderRhythmDensity->value() / 100.0;
    double voicingThinning = m_sliderVoicingThinning->value() / 100.0;
    bool forceTriads = m_chkTriadAnchors->isChecked();
    int grooveType = m_comboGroove->currentIndex();

    std::vector<int> chord1Notes = getChordNotes(root1, m_comboType1->currentText());
    std::vector<int> chord2Notes = getChordNotes(root2, m_comboType2->currentText());

    int ticksPerStep = 12;


    for (int step = 0; step < 64; ++step) {

        int relStep = step % 32;
        bool isAnchor = (relStep == pos1 || relStep == pos2);

        bool playStep = false;
        int velocity = 100;
        int tickOffset = 0;


        if (isAnchor) {
            playStep = true;
            velocity = 120 + QRandomGenerator::global()->bounded(8);
        } else {
            double chance = QRandomGenerator::global()->generateDouble();

            if (step % 4 == 0) {
                if (chance < (rhythmDensity * 1.5)) { playStep = true; velocity = 90 + QRandomGenerator::global()->bounded(20); }
            } else {
                if (chance < rhythmDensity) { playStep = true; velocity = 70 + QRandomGenerator::global()->bounded(30); }
            }
        }

        if (!playStep) continue;


        if (grooveType == 1 && step % 2 != 0) tickOffset = 3;


        std::vector<int> activeChord = (relStep >= pos2 && pos2 > pos1) ? chord2Notes : chord1Notes;
        if (pos1 > pos2 && (relStep >= pos1 || relStep < pos2)) activeChord = chord1Notes;

        std::vector<int> finalVoicing = activeChord;


        if (isAnchor && forceTriads) {
            finalVoicing = getTriad(activeChord);
        }
        else if (!isAnchor && finalVoicing.size() > 2) {

            finalVoicing.erase(finalVoicing.begin());


            while (finalVoicing.size() > 1 && QRandomGenerator::global()->generateDouble() < voicingThinning) {
                int dropIdx = QRandomGenerator::global()->bounded(finalVoicing.size() - 1);
                finalVoicing.erase(finalVoicing.begin() + dropIdx);
            }
        }


        int actualTick = (step * ticksPerStep) + tickOffset;
        int len = isAnchor ? 12 + QRandomGenerator::global()->bounded(6) : 6 + QRandomGenerator::global()->bounded(8);

        for (int key : finalVoicing) {
            PianoNote n;
            n.key = key;
            n.pos = actualTick;
            n.len = len;
            n.vol = std::clamp(velocity + QRandomGenerator::global()->bounded(-5, 5), 0, 127);
            n.pan = 0;
            m_notes.push_back(n);
        }
    }

    updateTable();
}

void HousePianoStabifier::onLoadClicked() {
    QString file = QFileDialog::getOpenFileName(this, "Load LMMS Pattern", "", "LMMS Pattern (*.xpt)");
    if (!file.isEmpty()) loadXpt(file);
}

void HousePianoStabifier::loadXpt(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QDomDocument doc;
    if (!doc.setContent(&file)) { file.close(); return; }
    file.close();

    m_notes.clear();
    QDomNodeList notes = doc.elementsByTagName("note");
    for (int i = 0; i < notes.count(); ++i) {
        QDomElement e = notes.at(i).toElement();
        PianoNote n;
        n.key = e.attribute("key").toInt();
        n.pos = e.attribute("pos").toInt();
        n.len = e.attribute("len").toInt();
        n.vol = e.attribute("vol", "100").toInt();
        m_notes.push_back(n);
    }
    updateTable();
}

void HousePianoStabifier::onSaveClicked() {
    if (m_notes.empty()) return;
    QString file = QFileDialog::getSaveFileName(this, "Save Stabbed Pattern", "stabified.xpt", "LMMS Pattern (*.xpt)");
    if (!file.isEmpty()) saveXpt(file);
}

void HousePianoStabifier::saveXpt(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project creator=\"LMMS\" type=\"pattern\" version=\"20\">\n";
    out << "  <head/>\n";
    out << "  <pattern muted=\"0\" steps=\"64\" type=\"1\" pos=\"0\" name=\"House Stab 2.0\">\n"; // Set to 64 steps (4 bars)

    for (const auto& n : m_notes) {
        out << "    <note key=\"" << n.key
            << "\" vol=\"" << n.vol
            << "\" pos=\"" << n.pos
            << "\" pan=\"0\" len=\"" << n.len << "\"/>\n";
    }

    out << "  </pattern>\n";
    out << "</lmms-project>\n";
    file.close();

    QMessageBox::information(this, "Saved", "Dynamic 4-bar stabs generated and saved!");
}

void HousePianoStabifier::onClearClicked() {
    m_notes.clear();
    updateTable();
}

void HousePianoStabifier::updateTable() {
    m_table->setRowCount(m_notes.size());
    for (int i = 0; i < m_notes.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(i+1)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(m_notes[i].key)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(m_notes[i].pos)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(m_notes[i].len)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(m_notes[i].vol)));
    }
}
