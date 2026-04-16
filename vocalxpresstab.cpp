#include "vocalxpresstab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDomDocument>
#include <QFile>
#include <cmath>

VocalXpressTab::VocalXpressTab(QWidget *parent) : QWidget(parent) {
    initSamLibrary();
    initNRLRules();
    setupUI();
}

QString VocalXpressTab::compileNRLRegex(const QString& context, bool isLeft) {
    if (context.isEmpty() || context == " ") return ".*";
    QString rx = context;
    rx.replace("#", "[AEIOUY]+");
    rx.replace(":", "[B-DF-HJ-NP-TV-Z]*");
    rx.replace("^", "[B-DF-HJ-NP-TV-Z]");
    rx.replace(".", "[BDGJLMNRVWZ]");
    rx.replace("%", "(ER|E|ES|ED|ING|ELY)");
    if (isLeft) return rx + "$";
    return "^" + rx;
}

void VocalXpressTab::initNRLRules() {
    auto addRule = [&](const QString& l, const QString& m, const QString& r, const QString& out) {
        m_nrlRules.append({compileNRLRegex(l, true), m, compileNRLRegex(r, false), out});
    };


    addRule("", "A", "RE", "EH R*"); addRule("", "A", "R#", "EH R*");
    addRule("", "A", "R", "AA R*"); addRule("", "A", "S#", "EY S*");
    addRule("", "A", "WA", "AX W AX"); addRule("", "A", "^I#", "EY");
    addRule(" ", "A", "^E%", "EY"); addRule("", "A", "Y", "EY");
    addRule(":", "A", "L", "AO L*"); addRule("", "A", "", "AE");

    addRule("", "CH", "", "CH"); addRule("", "C", "E", "S*");
    addRule("", "C", "I", "S*"); addRule("", "C", "Y", "S*");
    addRule("", "C", "", "K*");

    addRule("", "EE", "", "IY"); addRule("", "EA", "", "IY");
    addRule("", "ER", "", "ER"); addRule("", "E", "", "EH");

    addRule("", "G", "E", "J*"); addRule("", "G", "I", "J*");
    addRule("", "G", "", "G*");

    addRule("", "I", "R", "ER"); addRule("", "I", "GH", "AY");
    addRule("", "I", "", "IH");

    addRule("", "OO", "", "UW"); addRule("", "OR", "", "AO R*");
    addRule("", "OU", "", "AW"); addRule("", "O", "", "AA");

    addRule("", "PH", "", "F*"); addRule("", "P", "", "P*");

    addRule("", "SH", "", "SH"); addRule("", "S", "ION", "ZH");
    addRule("", "S", "", "S*");

    addRule("", "TH", "", "TH"); addRule("", "T", "ION", "SH");
    addRule("", "T", "", "T*");

    addRule("", "UR", "", "ER"); addRule("", "U", "", "AH");

    addRule("", "WH", "", "W*"); addRule("", "W", "", "W*");

    addRule("", "B", "", "B*"); addRule("", "D", "", "D*");
    addRule("", "F", "", "F*"); addRule("", "H", "", "H");
    addRule("", "J", "", "J*"); addRule("", "K", "", "K*");
    addRule("", "L", "", "L*"); addRule("", "M", "", "M*");
    addRule("", "N", "", "N*"); addRule("", "R", "", "R*");
    addRule("", "V", "", "V*"); addRule("", "X", "", "K* S*");
    addRule("", "Y", "", "Y*"); addRule("", "Z", "", "Z*");
}

void VocalXpressTab::initSamLibrary() {
    m_samLibrary["IY"] = {"IY", 10,84,110,true,15,10,5,18};
    m_samLibrary["IH"] = {"IH", 14,73,93,true,15,10,5,15};
    m_samLibrary["EH"] = {"EH", 19,67,91,true,15,10,5,16};
    m_samLibrary["AE"] = {"AE", 24,63,88,true,15,10,5,18};
    m_samLibrary["AA"] = {"AA", 27,40,89,true,15,10,5,18};
    m_samLibrary["AH"] = {"AH", 23,44,87,true,15,10,5,16};
    m_samLibrary["AO"] = {"AO", 21,31,88,true,15,10,5,18};
    m_samLibrary["UH"] = {"UH", 16,37,82,true,15,10,5,15};
    m_samLibrary["AX"] = {"AX", 20,45,89,true,15,10,5,12};
    m_samLibrary["IX"] = {"IX", 14,73,93,true,15,10,5,12};
    m_samLibrary["ER"] = {"ER", 18,49,62,true,15,10,5,18};
    m_samLibrary["UX"] = {"UX", 14,36,82,true,15,10,5,15};
    m_samLibrary["OH"] = {"OH", 18,30,88,true,15,10,5,18};
    m_samLibrary["EY"] = {"EY", 19,72,90,true,15,10,5,20};
    m_samLibrary["AY"] = {"AY", 27,39,88,true,15,10,5,22};
    m_samLibrary["OY"] = {"OY", 21,31,88,true,15,10,5,22};
    m_samLibrary["AW"] = {"AW", 27,43,88,true,15,10,5,22};
    m_samLibrary["OW"] = {"OW", 18,30,88,true,15,10,5,20};
    m_samLibrary["UW"] = {"UW", 13,34,82,true,15,10,5,18};

    m_samLibrary["M*"] = {"M*",6,46,81,true,12,8,4,15};
    m_samLibrary["N*"] = {"N*",6,54,121,true,12,8,4,15};
    m_samLibrary["R*"] = {"R*",18,50,60,true,14,9,5,18};
    m_samLibrary["L*"] = {"L*",14,30,110,true,14,9,5,18};
    m_samLibrary["W*"] = {"W*",11,24,90,true,12,8,4,12};
    m_samLibrary["Y*"] = {"Y*",9,83,110,true,12,8,4,12};
    m_samLibrary["Z*"] = {"Z*",9,51,93,true,12,8,4,12};
    m_samLibrary["V*"] = {"V*",8,40,76,true,12,8,4,10};

    m_samLibrary["S*"] = {"S*",6,73,99,false,14,0,0,14};
    m_samLibrary["SH"] = {"SH",6,79,106,false,13,0,0,14};
    m_samLibrary["F*"] = {"F*",6,26,81,false,12,0,0,12};
    m_samLibrary["TH"] = {"TH",6,66,121,false,12,0,0,12};
    m_samLibrary["P*"] = {"P*",6,26,81,false,13,0,0,6};
    m_samLibrary["T*"] = {"T*",6,66,121,false,13,0,0,6};
    m_samLibrary["K*"] = {"K*",6,85,101,false,14,0,0,7};
}

QString VocalXpressTab::textToPhonemes(const QString& englishText) {
    QString word = englishText.toUpper();
    word.remove(QRegularExpression("[^A-Z]"));

    QString phonemes = "";
    int pos = 0;

    while (pos < word.length()) {
        bool matched = false;
        for (const SamNRLRule& rule : m_nrlRules) {
            if (word.mid(pos).startsWith(rule.match)) {
                QString leftStr = word.left(pos);
                QString rightStr = word.mid(pos + rule.match.length());
                if (QRegularExpression(rule.leftRx).match(leftStr).hasMatch() &&
                    QRegularExpression(rule.rightRx).match(rightStr).hasMatch()) {
                    phonemes += rule.phonemes + " ";
                    pos += rule.match.length();
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) {
            QString letter = word.mid(pos, 1);
            phonemes += letter + "* ";
            pos++;
        }
    }
    return phonemes.simplified();
}

void VocalXpressTab::setupUI() {
    QVBoxLayout *main = new QVBoxLayout(this);

    // Lyrics
    QGroupBox *lyricsGroup = new QGroupBox("Lyrics → Phonemes");
    QHBoxLayout *ly = new QHBoxLayout(lyricsGroup);
    m_lyricsInput = new QLineEdit(); m_lyricsInput->setPlaceholderText("e.g. HELLO WORLD YEAH");
    m_phonemizeBtn = new QPushButton("Phonemize");
    ly->addWidget(new QLabel("Lyrics:")); ly->addWidget(m_lyricsInput); ly->addWidget(m_phonemizeBtn);
    main->addWidget(lyricsGroup);

    // Syllable pool
    QGroupBox *poolGroup = new QGroupBox("Syllable Pool (drag or double-click)");
    QVBoxLayout *pl = new QVBoxLayout(poolGroup);
    m_syllablePool = new QListWidget(); m_syllablePool->setDragEnabled(true);
    pl->addWidget(m_syllablePool);
    main->addWidget(poolGroup);


    QHBoxLayout *ctrl = new QHBoxLayout();
    m_loadPatternBtn = new QPushButton("Load Pattern (.xpt)");
    m_bpmSpin = new QSpinBox(); m_bpmSpin->setRange(60,200); m_bpmSpin->setValue(128); m_bpmSpin->setSuffix(" BPM");
    m_stepsCombo = new QComboBox(); m_stepsCombo->addItems({"16 steps","32 steps","64 steps"}); m_stepsCombo->setCurrentIndex(1);
    m_durationLabel = new QLabel("Bar duration: 2.00 s");

    ctrl->addWidget(m_loadPatternBtn);
    ctrl->addWidget(new QLabel("BPM:")); ctrl->addWidget(m_bpmSpin);
    ctrl->addWidget(new QLabel("Length:")); ctrl->addWidget(m_stepsCombo);
    ctrl->addWidget(m_durationLabel); ctrl->addStretch();
    main->addLayout(ctrl);


    m_patternTable = new QTableWidget(0,5);
    m_patternTable->setHorizontalHeaderLabels({"Step","MIDI Note","Syllable","Duration (steps)","Stretch %"});
    m_patternTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    main->addWidget(m_patternTable);

    QPushButton *assignBtn = new QPushButton("Assign selected syllable to selected step");
    connect(assignBtn, &QPushButton::clicked, this, &VocalXpressTab::assignSyllable);
    main->addWidget(assignBtn);


    QGroupBox *voiceGroup = new QGroupBox("Voice Engine");
    QGridLayout *vg = new QGridLayout(voiceGroup);
    m_mouthSlider = new QSlider(Qt::Horizontal); m_mouthSlider->setRange(50,200); m_mouthSlider->setValue(128);
    m_throatSlider = new QSlider(Qt::Horizontal); m_throatSlider->setRange(50,200); m_throatSlider->setValue(128);
    m_pitchSlider = new QSlider(Qt::Horizontal); m_pitchSlider->setRange(50,200); m_pitchSlider->setValue(100);
    m_speedSlider = new QSlider(Qt::Horizontal); m_speedSlider->setRange(50,200); m_speedSlider->setValue(100);
    vg->addWidget(new QLabel("Mouth"),0,0); vg->addWidget(m_mouthSlider,0,1);
    vg->addWidget(new QLabel("Throat"),0,2); vg->addWidget(m_throatSlider,0,3);
    vg->addWidget(new QLabel("Pitch"),1,0); vg->addWidget(m_pitchSlider,1,1);
    vg->addWidget(new QLabel("Speed"),1,2); vg->addWidget(m_speedSlider,1,3);
    m_nightlyCheckBox = new QCheckBox("Nightly Build (ExprTk radians)"); m_nightlyCheckBox->setChecked(true);
    vg->addWidget(m_nightlyCheckBox,2,0,1,4);
    main->addWidget(voiceGroup);


    m_renderBtn = new QPushButton("RENDER FULL BAR EXPRESSION");
    m_renderBtn->setStyleSheet("font-weight: bold; height: 42px; background-color: #c026d3; color: white;");
    main->addWidget(m_renderBtn);

    QGroupBox *outGroup = new QGroupBox("Xpressive Output (paste into LMMS)");
    QVBoxLayout *ol = new QVBoxLayout(outGroup);
    ol->addWidget(new QLabel("W1 (glottal carrier):"));
    m_w1Output = new QTextEdit(); m_w1Output->setMaximumHeight(60); m_w1Output->setReadOnly(true);
    ol->addWidget(m_w1Output);
    ol->addWidget(new QLabel("O1 (full singing expression):"));
    m_o1Output = new QTextEdit(); m_o1Output->setReadOnly(true);
    ol->addWidget(m_o1Output);
    m_copyBtn = new QPushButton("COPY O1 TO CLIPBOARD");
    m_copyBtn->setStyleSheet("font-weight:bold; background:#c026d3; color:white;");
    ol->addWidget(m_copyBtn);
    main->addWidget(outGroup);


    connect(m_phonemizeBtn, &QPushButton::clicked, this, &VocalXpressTab::phonemizeLyrics);
    connect(m_stepsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VocalXpressTab::onPatternLengthChanged);
    connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &VocalXpressTab::onPatternLengthChanged);
    connect(m_renderBtn, &QPushButton::clicked, this, &VocalXpressTab::renderExpression);
    connect(m_copyBtn, &QPushButton::clicked, [this](){ QApplication::clipboard()->setText(m_o1Output->toPlainText()); });
    connect(m_loadPatternBtn, &QPushButton::clicked, this, &VocalXpressTab::loadPattern);

    onPatternLengthChanged();
}

void VocalXpressTab::loadPattern() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load LMMS Pattern", "", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file");
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        QMessageBox::warning(this, "Error", "Invalid .xpt file");
        file.close();
        return;
    }
    file.close();

    QDomNodeList notes = doc.elementsByTagName("note");
    if (notes.isEmpty()) {
        QMessageBox::warning(this, "Error", "No notes found in pattern");
        return;
    }


    onPatternLengthChanged();

    int steps = m_patternTable->rowCount();
    for (int i = 0; i < notes.count(); ++i) {
        QDomElement note = notes.at(i).toElement();
        int pos = note.attribute("pos").toInt();
        int key = note.attribute("key").toInt();
        int len = note.attribute("len").toInt();

        int step = (pos / 24) % steps;
        if (step >= 0 && step < steps) {
            m_patternTable->item(step, 0)->setText(QString::number(step+1));
            m_patternTable->item(step, 1)->setText(QString::number(key));
            m_patternTable->item(step, 2)->setText("");
            int durSteps = qMax(1, len / 24);
            m_patternTable->item(step, 3)->setText(QString::number(durSteps));
        }
    }

    QMessageBox::information(this, "Loaded", "Pattern loaded successfully!\nAssign syllables to the steps.");
}

void VocalXpressTab::onPatternLengthChanged() {
    int steps = m_stepsCombo->currentText().split(" ").first().toInt();
    double barSec = (60.0 / m_bpmSpin->value()) * (steps / 16.0);
    m_durationLabel->setText(QString("Bar duration: %1 s").arg(barSec, 0, 'f', 2));

    m_patternTable->setRowCount(steps);
    for (int i = 0; i < steps; ++i) {
        m_patternTable->setItem(i, 0, new QTableWidgetItem(QString::number(i+1)));
        m_patternTable->setItem(i, 1, new QTableWidgetItem(""));
        m_patternTable->setItem(i, 2, new QTableWidgetItem(""));
        m_patternTable->setItem(i, 3, new QTableWidgetItem("1"));
        m_patternTable->setItem(i, 4, new QTableWidgetItem("100"));
    }
}

void VocalXpressTab::phonemizeLyrics() {
    m_syllablePool->clear();
    QString phonemes = textToPhonemes(m_lyricsInput->text());
    QStringList tokens = phonemes.split(" ", Qt::SkipEmptyParts);
    for (const QString &t : tokens) {
        QListWidgetItem *item = new QListWidgetItem(t);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        m_syllablePool->addItem(item);
    }
}

void VocalXpressTab::assignSyllable() {
    if (!m_syllablePool->currentItem() || m_patternTable->currentRow() < 0) return;
    int row = m_patternTable->currentRow();
    m_patternTable->item(row, 2)->setText(m_syllablePool->currentItem()->text());
}

QString VocalXpressTab::buildTimedExpression(bool nightly) {
    m_timedSequence.clear();
    int steps = m_patternTable->rowCount();
    double stepSec = (60.0 / m_bpmSpin->value()) / 16.0;

    double currentTime = 0.0;
    for (int i = 0; i < steps; ++i) {
        QString syl = m_patternTable->item(i, 2) ? m_patternTable->item(i, 2)->text() : "";
        if (syl.isEmpty()) { currentTime += stepSec; continue; }

        int durSteps = m_patternTable->item(i, 3)->text().toInt();
        double stretch = m_patternTable->item(i, 4)->text().toDouble() / 100.0;
        double dur = durSteps * stepSec * stretch;

        m_timedSequence.push_back({syl, currentTime, dur});
        currentTime += dur;
    }

    QString o1 = "";
    QString pi = nightly ? "6.28318 * " : "";
    QString sinf = nightly ? "sin" : "sinew";

    for (const auto &node : m_timedSequence) {
        QString phonemeMath = QString("0.8 * %1(%2integrate(f))").arg(sinf).arg(pi);
        QString gate = QString("(t >= %1 && t < %2)").arg(node.startSec).arg(node.startSec + node.durationSec);
        if (!o1.isEmpty()) o1 += " + ";
        o1 += QString("%1 * %2").arg(gate).arg(phonemeMath);
    }

    QString final = QString("clamp(-1.0, (%1) * exp(-max(0.0, t - %2) * 12.0), 1.0)").arg(o1).arg(currentTime);
    return final;
}

void VocalXpressTab::renderExpression() {
    bool nightly = m_nightlyCheckBox->isChecked();
    QString o1 = buildTimedExpression(nightly);
    m_o1Output->setText(o1);
    m_w1Output->setText(nightly ? "(0.5 + 0.5 * cos(6.28318 * t * (pitch/100.0)))" : "(0.5 + 0.5 * sinew(t * (pitch/100.0)))");
    QApplication::clipboard()->setText(o1);
    QMessageBox::information(this, "Success", "O1 expression copied to clipboard!\nPaste directly into any Xpressive track.");
}
