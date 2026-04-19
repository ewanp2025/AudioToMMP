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

    QGroupBox *ctrlGroup = new QGroupBox("House Piano Stabifier 4.0");
    QVBoxLayout *ctrl = new QVBoxLayout(ctrlGroup);


    QHBoxLayout *chordLayout = new QHBoxLayout();


    chordLayout->addWidget(new QLabel("Root 1:"));
    m_comboRoot1 = new QComboBox();
    m_comboRoot1->addItems({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
    m_comboRoot1->setCurrentText("C");
    chordLayout->addWidget(m_comboRoot1);

    m_comboType1 = new QComboBox();
    m_comboType1->addItems({"m7", "m9", "m11", "Maj7", "Maj9", "add9", "dom7", "dom9", "7sus4", "m (triad)", "Maj (triad)"});
    m_comboType1->setCurrentText("m9");
    chordLayout->addWidget(m_comboType1);

    chordLayout->addWidget(new QLabel("Pos 1:"));
    m_spinPos1 = new QSpinBox();
    m_spinPos1->setRange(1, 32);
    m_spinPos1->setValue(1);
    chordLayout->addWidget(m_spinPos1);


    chordLayout->addSpacing(20);
    chordLayout->addWidget(new QLabel("Root 2:"));
    m_comboRoot2 = new QComboBox();
    m_comboRoot2->addItems({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
    m_comboRoot2->setCurrentText("F");
    chordLayout->addWidget(m_comboRoot2);

    m_comboType2 = new QComboBox();
    m_comboType2->addItems({"m7", "m9", "m11", "Maj7", "Maj9", "add9", "dom7", "dom9", "7sus4", "m (triad)", "Maj (triad)"});
    m_comboType2->setCurrentText("Maj9");
    chordLayout->addWidget(m_comboType2);

    chordLayout->addWidget(new QLabel("Pos 2:"));
    m_spinPos2 = new QSpinBox();
    m_spinPos2->setRange(1, 32);
    m_spinPos2->setValue(26);
    chordLayout->addWidget(m_spinPos2);


    ctrl->addLayout(chordLayout);
    m_comboGenMode = new QComboBox();
    m_comboGenMode->addItems({
        "01. Dynamic Call & Response (Original)",
        "02. Strict Off-Beat (90s )",
        "03. The Push (Anticipation)",
        "04. 3-over-4 Polyrhythm ",
        "05. Korg M1 Syncopation (Classic)"
    });

    m_comboGroove = new QComboBox();
    m_comboGroove->addItems({
        "Classic House",
        "UK Garage (Swung)",
        "Jackin' House (Heavy Swing)",
        "Tech House (Micro-Swing)"
    });

    QHBoxLayout *grooveLayout = new QHBoxLayout();
    QVBoxLayout *col1 = new QVBoxLayout();

    col1->addWidget(new QLabel("Rhythm Mode:"));
    col1->addWidget(m_comboGenMode);

    m_sliderRhythmDensity = new QSlider(Qt::Horizontal);
    m_sliderRhythmDensity->setRange(0, 100);
    m_sliderRhythmDensity->setValue(55);
    col1->addWidget(new QLabel("Rhythm Density (Triggers):"));
    col1->addWidget(m_sliderRhythmDensity);


    QVBoxLayout *col2 = new QVBoxLayout();
    m_sliderVoicingThinning = new QSlider(Qt::Horizontal);
    m_sliderVoicingThinning->setRange(0, 100);
    m_sliderVoicingThinning->setValue(80);
    col2->addWidget(new QLabel("Voicing Thinning (Intensity):"));
    col2->addWidget(m_sliderVoicingThinning);

    m_sliderArticulation = new QSlider(Qt::Horizontal);
    m_sliderArticulation->setRange(0, 100);
    m_sliderArticulation->setValue(80);
    col2->addWidget(new QLabel("Length Variance (Staccato/Tenuto):"));
    col2->addWidget(m_sliderArticulation);


    QVBoxLayout *col3 = new QVBoxLayout();
    m_chkTriadAnchors = new QCheckBox("Force Triads on Anchors");
    m_chkTriadAnchors->setChecked(true);

    m_chkDynamicLengths = new QCheckBox("Enable Dynamic Lengths");
    m_chkDynamicLengths->setChecked(true);

    col3->addWidget(m_chkTriadAnchors);
    col3->addWidget(m_chkDynamicLengths);
    col3->addWidget(new QLabel("Swing:"));
    col3->addWidget(m_comboGroove);

    grooveLayout->addLayout(col1);
    grooveLayout->addLayout(col2);
    grooveLayout->addLayout(col3);
    ctrl->addLayout(grooveLayout);

    QHBoxLayout *top = new QHBoxLayout();
    m_btnLoad = new QPushButton("Load .xpt (Disabled for Gen)");
    m_btnGenerate = new QPushButton("GENERATE 4-BAR STABS");
    m_btnGenerate->setStyleSheet("background-color:#c026d3; color:white; font-weight:bold; padding:12px; font-size:15px;");

    m_btnHit = new QPushButton("TEST MODE ONLY");
    m_btnHit->setStyleSheet("background-color:#22c55e; color:black; font-weight:bold; padding:14px; font-size:16px;");

    m_btnSave = new QPushButton("Save Stabbed .xpt");
    m_btnClear = new QPushButton("Clear");

    top->addWidget(m_btnLoad);
    top->addWidget(m_btnGenerate);
    top->addWidget(m_btnHit);
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
    connect(m_btnHit, &QPushButton::clicked, this, &HousePianoStabifier::onGenerateHitClicked);
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
    else if (type == "dom9") notes.insert(notes.end(), {rootMidi + 4, rootMidi + 7, rootMidi + 10, rootMidi + 14});
    else if (type == "7sus4") notes.insert(notes.end(), {rootMidi + 5, rootMidi + 7, rootMidi + 10});
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

    int root1 = 60 + m_comboRoot1->currentIndex();
    int root2 = 60 + m_comboRoot2->currentIndex();
    int pos1 = m_spinPos1->value() - 1;
    int pos2 = m_spinPos2->value() - 1;

    {
        int diff = std::abs(root2 - root1) % 12;
        std::vector<int> goodIntervals = {0, 3, 4, 5, 7, 8, 9};
        if (std::find(goodIntervals.begin(), goodIntervals.end(), diff) == goodIntervals.end()) {
            QMessageBox::warning(this, "Chord Compatibility Warning",
                                 "The call & response chords you chose have roots that are not the most common for house piano stabs.\n\n"
                                 "They may sound a bit dissonant or less 'hit-like', but I'm generating anyway!");
        }
    }

    int generationMode = m_comboGenMode->currentIndex();
    double rhythmDensity = m_sliderRhythmDensity->value() / 100.0;
    double voicingThinning = m_sliderVoicingThinning->value() / 100.0;
    double articulationVar = m_sliderArticulation->value() / 100.0;

    bool forceTriads = m_chkTriadAnchors->isChecked();
    bool dynamicLengths = m_chkDynamicLengths->isChecked();
    int grooveType = m_comboGroove->currentIndex();

    std::vector<int> chord1Notes = getChordNotes(root1, m_comboType1->currentText());
    std::vector<int> chord2Notes = getChordNotes(root2, m_comboType2->currentText());

    int ticksPerStep = 12;


    for (int step = 0; step < 64; ++step) {

        int relStep = step % 32;
        bool playStep = false;
        bool isAnchor = false;
        int velocity = 100;
        int tickOffset = 0;


        if (generationMode == 0) {
            isAnchor = (relStep == pos1 || relStep == pos2);
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
        }
        else if (generationMode == 1) {
            if (step % 4 == 2) {
                playStep = (QRandomGenerator::global()->generateDouble() < (rhythmDensity + 0.3));
                velocity = 110 + QRandomGenerator::global()->bounded(10);
                isAnchor = (relStep == 2 || relStep == 18);
            }
        }
        else if (generationMode == 2) {
            if (step % 4 == 3) {
                playStep = (QRandomGenerator::global()->generateDouble() < (rhythmDensity + 0.3));
                velocity = 105 + QRandomGenerator::global()->bounded(15);
                isAnchor = (relStep == 15 || relStep == 31);
            }
        }
        else if (generationMode == 3) {
            if (step % 3 == 0) {
                playStep = (QRandomGenerator::global()->generateDouble() < (rhythmDensity + 0.4));
                velocity = 115 + QRandomGenerator::global()->bounded(10);
                isAnchor = (step % 12 == 0);
            }
        }
        else if (generationMode == 4) {

            int patternStep = step % 16;
            if (patternStep == 0 || patternStep == 3 || patternStep == 8 || patternStep == 11) {
                playStep = (QRandomGenerator::global()->generateDouble() < (rhythmDensity + 0.6));
                velocity = 115 + QRandomGenerator::global()->bounded(10);
                isAnchor = (patternStep == 0 || patternStep == 8);
            }
        }

        if (!playStep) continue;

        if (step % 2 != 0) {
            if (grooveType == 1) tickOffset = 3;
            else if (grooveType == 2) tickOffset = 5;
            else if (grooveType == 3) tickOffset = 1;
        }


        std::vector<int> activeChord = (relStep >= pos2 && pos2 > pos1) ? chord2Notes : chord1Notes;
        if (pos1 > pos2 && (relStep >= pos1 || relStep < pos2)) activeChord = chord1Notes;

        std::vector<int> finalVoicing = activeChord;


        if (isAnchor && forceTriads) {
            finalVoicing = getTriad(activeChord);
        }
        else if (!isAnchor && finalVoicing.size() > 2) {
            finalVoicing.erase(finalVoicing.begin());
            while (finalVoicing.size() > 1 && QRandomGenerator::global()->generateDouble() < voicingThinning) {
                int dropIdx = QRandomGenerator::global()->bounded(static_cast<int>(finalVoicing.size() - 1));
                finalVoicing.erase(finalVoicing.begin() + dropIdx);
            }
        }


        int actualTick = (step * ticksPerStep) + tickOffset;
        int len;

        if (dynamicLengths) {

            int baseLong = 12 + (articulationVar * 8);
            int baseShort = 10 - (articulationVar * 6);

            if (isAnchor) {
                len = baseLong + QRandomGenerator::global()->bounded(4);
            } else if (step % 2 != 0) {

                len = (QRandomGenerator::global()->generateDouble() > 0.5) ? baseLong : baseShort;
            } else {
                len = baseShort + QRandomGenerator::global()->bounded(3);
            }
        } else {

            len = isAnchor ? 12 + QRandomGenerator::global()->bounded(4) : 6 + QRandomGenerator::global()->bounded(4);
        }


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

void HousePianoStabifier::onGenerateHitClicked()
{

    QString roots[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int r = QRandomGenerator::global()->bounded(12);
    m_comboRoot1->setCurrentText(roots[r]);
    m_comboRoot2->setCurrentText(roots[(r + 5) % 12]);

    m_comboType1->setCurrentText("Maj9");
    m_comboType2->setCurrentText("Maj9");

    m_spinPos1->setValue(1);
    m_spinPos2->setValue(26);

    m_comboGenMode->setCurrentIndex(0);

    m_chkTriadAnchors->setChecked(true);
    m_chkDynamicLengths->setChecked(true);
    m_comboGroove->setCurrentIndex(1);

    onGenerateStabsClicked();

    QMessageBox::information(this, "GENERATED!",
                             "m9 → Maj9 call & response (perfect 4th apart) with UK Garage swing.\n\n"
                             "Sliders for your own adjustments\n"
                             "Click again for a brand new key.");
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
    out << "  <pattern muted=\"0\" steps=\"64\" type=\"1\" pos=\"0\" name=\"House Stab 4.0\">\n";

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
