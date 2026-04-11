#include "smartpatterneditor.h"

SmartPatternEditor::SmartPatternEditor(QWidget *parent) : QWidget(parent)
{
    setupUI();
}

void SmartPatternEditor::setupUI()
{
    QVBoxLayout *smartLayout = new QVBoxLayout(this);


    QHBoxLayout *topToolbar = new QHBoxLayout();
    QPushButton *btnLoad = new QPushButton("Load .xpt");
    QPushButton *btnSave = new QPushButton("Save .xpt"); // RESTORED!

    topToolbar->addWidget(btnLoad);
    topToolbar->addWidget(btnSave);
    topToolbar->addWidget(new QLabel(" | <b>Basic:</b>"));

    QPushButton *btnReverse = new QPushButton("Reverse");
    QPushButton *btnModulate = new QPushButton("Modulate");
    btnReverse->setStyleSheet("background-color: #555; color: white;");
    btnModulate->setStyleSheet("background-color: #8b008b; color: white;");

    topToolbar->addWidget(btnReverse);
    topToolbar->addWidget(btnModulate);


    topToolbar->addStretch();
    m_lblDetectedScale = new QLabel("<b>Scale:</b> N/A");
    m_lblDetectedScale->setStyleSheet("background-color: #222; color: #00ff00; padding: 5px; border-radius: 3px; font-family: monospace; font-size: 14px;");
    topToolbar->addWidget(m_lblDetectedScale);

    smartLayout->addLayout(topToolbar);


    QHBoxLayout *midiToolbar = new QHBoxLayout();
    midiToolbar->addWidget(new QLabel("<b>Pro Tools:</b>"));

    QPushButton *btnStrum = new QPushButton("Strum (Alt+S)");
    QPushButton *btnChop = new QPushButton("Chopper (Alt+U)");
    QPushButton *btnArp = new QPushButton("Arpeggiate (Alt+A)");
    QPushButton *btnFlam = new QPushButton("Flam (Alt+F)");
    QPushButton *btnRandom = new QPushButton("Humanize (Alt+R)");
    QPushButton *btnRiff = new QPushButton("Riff Machine (Alt+M)");
    QPushButton *btnApi = new QPushButton("Python API...");

    QString btnStyle = "background-color: #d2691e; color: white; font-weight: bold;";
    btnStrum->setStyleSheet(btnStyle); btnChop->setStyleSheet(btnStyle);
    btnArp->setStyleSheet(btnStyle); btnFlam->setStyleSheet(btnStyle);
    btnRandom->setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold;");
    btnRiff->setStyleSheet("background-color: #00557f; color: white; font-weight: bold;");

    midiToolbar->addWidget(btnStrum); midiToolbar->addWidget(btnChop);
    midiToolbar->addWidget(btnArp); midiToolbar->addWidget(btnFlam);
    midiToolbar->addWidget(btnRandom); midiToolbar->addWidget(btnRiff);
    midiToolbar->addWidget(btnApi);
    midiToolbar->addStretch();
    smartLayout->addLayout(midiToolbar);


    m_scene = new QGraphicsScene(this);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &SmartPatternEditor::onSelectionChanged);

    m_pianoView = new QGraphicsView(m_scene);
    m_pianoView->setRenderHint(QPainter::Antialiasing, false);
    m_pianoView->setBackgroundBrush(QColor(30, 30, 30));
    m_pianoView->setDragMode(QGraphicsView::RubberBandDrag);

    m_pianoView->installEventFilter(this);
    m_pianoView->viewport()->installEventFilter(this);

    smartLayout->addWidget(new QLabel("<i>(<b>Double-Click</b> to add notes. <b>Right-Click</b> to delete. <b>Drag</b> to move to 1/16th Grid)</i>"));
    smartLayout->addWidget(m_pianoView);


    connect(btnLoad, &QPushButton::clicked, this, &SmartPatternEditor::onLoadClicked);
    connect(btnSave, &QPushButton::clicked, this, &SmartPatternEditor::onSaveClicked);
    connect(btnReverse, &QPushButton::clicked, this, &SmartPatternEditor::onReverseClicked);
    connect(btnModulate, &QPushButton::clicked, this, &SmartPatternEditor::onModulateClicked);

    connect(btnStrum, &QPushButton::clicked, this, &SmartPatternEditor::onStrumClicked);
    connect(btnChop, &QPushButton::clicked, this, &SmartPatternEditor::onChopperClicked);
    connect(btnArp, &QPushButton::clicked, this, &SmartPatternEditor::onArpeggiatorClicked);
    connect(btnFlam, &QPushButton::clicked, this, &SmartPatternEditor::onFlamClicked);
    connect(btnRandom, &QPushButton::clicked, this, &SmartPatternEditor::onRandomizerClicked);
    connect(btnRiff, &QPushButton::clicked, this, &SmartPatternEditor::onRiffMachineClicked);
    connect(btnApi, &QPushButton::clicked, this, &SmartPatternEditor::onScriptingApiClicked);

    updatePianoRoll();
}

int SmartPatternEditor::getNextId() { return ++m_highestId; }

void SmartPatternEditor::onLoadClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load LMMS Pattern", "", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QDomDocument doc;
    if (!doc.setContent(&file)) { file.close(); return; }
    file.close();

    m_smartNotes.clear();
    m_highestId = 0;

    QDomNodeList notes = doc.elementsByTagName("note");
    for (int i = 0; i < notes.count(); ++i) {
        QDomElement nElem = notes.at(i).toElement();
        SmartNote sn = {getNextId(), nElem.attribute("key").toInt(), nElem.attribute("pos").toInt(),
                        nElem.attribute("len").toInt(), nElem.attribute("vol").toInt(), true};
        m_smartNotes.push_back(sn);
    }
    updatePianoRoll();
}

void SmartPatternEditor::onSaveClicked()
{
    if (m_smartNotes.empty()) {
        QMessageBox::warning(this, "Empty", "No notes to save!");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Save LMMS Pattern", "SmartPattern.xpt", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file for writing.");
        return;
    }


    int maxEnd = 0;
    for (const auto& note : m_smartNotes) {
        if (note.pos + note.len > maxEnd) maxEnd = note.pos + note.len;
    }
    int totalSteps = std::max(16, (int)std::ceil((double)maxEnd / 48.0));

    QTextStream out(&file);
    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project creator=\"LMMS\" type=\"pattern\" version=\"20\" creatorversion=\"1.3.0\">\n";
    out << "  <head/>\n";
    out << "  <pattern muted=\"0\" steps=\"" << totalSteps << "\" type=\"1\" pos=\"0\" name=\"SmartPattern\">\n";

    for (const auto& note : m_smartNotes) {
        out << "    <note pan=\"0\" key=\"" << note.key
            << "\" len=\"" << note.len
            << "\" pos=\"" << note.pos
            << "\" vol=\"" << note.vol << "\"/>\n";
    }

    out << "  </pattern>\n";
    out << "</lmms-project>\n";

    file.close();
    QMessageBox::information(this, "Success", "Pattern saved successfully!");
}

void SmartPatternEditor::detectAndDisplayScale()
{
    if (m_smartNotes.empty()) {
        m_lblDetectedScale->setText("<b>Scale:</b> N/A");
        return;
    }

    double chroma[12] = {0};
    for (const auto& note : m_smartNotes) {
        if (note.key >= 0) chroma[note.key % 12] += 1.0;
    }

    int majorTemplate[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    int minorTemplate[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};

    double bestScore = -1.0;
    QString bestScale = "Unknown";
    const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    for (int root = 0; root < 12; ++root) {
        double majScore = 0, minScore = 0;
        for (int i = 0; i < 12; ++i) {
            int templateIndex = (i - root + 12) % 12;
            majScore += chroma[i] * majorTemplate[templateIndex];
            minScore += chroma[i] * minorTemplate[templateIndex];
        }

        if (majScore > bestScore) {
            bestScore = majScore;
            bestScale = noteNames[root] + " Major";
        }
        if (minScore > bestScore) {
            bestScore = minScore;
            bestScale = noteNames[root] + " Minor";
        }
    }

    m_lblDetectedScale->setText(QString("<b>Scale:</b> %1").arg(bestScale));
}

void SmartPatternEditor::updatePianoRoll()
{
    m_scene->clear();

    int keyHeight = 12;
    int gridWidth = 3072;
    int gridStartX = 60;

    m_scene->setSceneRect(0, 0, gridWidth + gridStartX, 128 * keyHeight);

    const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    QFont labelFont; labelFont.setPixelSize(10);

    for (int i = 0; i < 128; ++i) {
        int midiKey = 127 - i;
        int noteIdx = midiKey % 12;
        bool isBlackKey = (noteIdx == 1 || noteIdx == 3 || noteIdx == 6 || noteIdx == 8 || noteIdx == 10);

        QColor rowColor = isBlackKey ? QColor(20, 20, 20) : QColor(40, 40, 40);
        if (noteIdx == 0) rowColor = QColor(60, 60, 60);

        QGraphicsRectItem* row = m_scene->addRect(gridStartX, i * keyHeight, gridWidth, keyHeight, Qt::NoPen, rowColor);
        row->setZValue(-2);

        QGraphicsTextItem* text = m_scene->addText(noteNames[noteIdx] + QString::number((midiKey/12)-1), labelFont);
        text->setDefaultTextColor(isBlackKey ? QColor(150, 150, 150) : QColor(220, 220, 220));
        text->setPos(5, (i * keyHeight) - 3);
    }

    for (int x = 0; x <= gridWidth; x += 48) {
        QPen linePen;
        if (x % 768 == 0) {
            linePen = QPen(QColor(180, 180, 180, 200), 2);
        } else if (x % 192 == 0) {
            linePen = QPen(QColor(120, 120, 120, 180), 1);
        } else {
            linePen = QPen(QColor(70, 70, 70, 100), 1);
        }
        m_scene->addLine(gridStartX + x, 0, gridStartX + x, 128 * keyHeight, linePen)->setZValue(-1);
    }

    for (const auto& note : m_smartNotes) {
        QColor noteColor = note.selected ? QColor(0, 255, 0) : QColor(0, 150, 255);
        QGraphicsRectItem* item = m_scene->addRect(gridStartX + note.pos, (127 - note.key) * keyHeight, note.len, keyHeight, QPen(Qt::black), noteColor);
        item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        item->setData(0, note.id);
        item->setSelected(note.selected);
    }


    detectAndDisplayScale();
}


bool SmartPatternEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_pianoView->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPointF scenePos = m_pianoView->mapToScene(mouseEvent->pos());
            int key = 127 - (int)(scenePos.y() / 12);
            int pos = (int)((scenePos.x() - 60) / 48) * 48;

            if (key >= 0 && key <= 127 && pos >= 0) {
                m_smartNotes.push_back({getNextId(), key, pos, 48, 100, true});
                updatePianoRoll();
            }
            return true;
        }
    }
    else if (obj == m_pianoView->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            QPointF scenePos = m_pianoView->mapToScene(mouseEvent->pos());
            QGraphicsItem *item = m_scene->itemAt(scenePos, QTransform());
            if (item && item->data(0).isValid()) {
                int id = item->data(0).toInt();
                m_smartNotes.erase(std::remove_if(m_smartNotes.begin(), m_smartNotes.end(),
                                                  [id](const SmartNote& n){ return n.id == id; }), m_smartNotes.end());
                updatePianoRoll();
                return true;
            }
        }
    }
    else if (obj == m_pianoView->viewport() && event->type() == QEvent::MouseButtonRelease) {

        bool res = QWidget::eventFilter(obj, event);
        bool requiresRedraw = false;


        for (QGraphicsItem* item : m_scene->selectedItems()) {
            if (item->data(0).isValid()) {
                int id = item->data(0).toInt();


                double droppedX = item->sceneBoundingRect().x();
                double droppedY = item->sceneBoundingRect().y();

                for (auto& note : m_smartNotes) {
                    if (note.id == id) {


                        if (std::abs((droppedX - 60) - note.pos) > 2.0 || std::abs(droppedY - (127 - note.key) * 12.0) > 2.0) {


                            int snappedX = std::max(0, (int)std::round((droppedX - 60) / 48.0) * 48);
                            int newKey = std::clamp(127 - (int)std::round(droppedY / 12.0), 0, 127);

                            if (note.pos != snappedX || note.key != newKey) {
                                note.pos = snappedX;
                                note.key = newKey;
                                requiresRedraw = true;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (requiresRedraw) updatePianoRoll();
        return res;
    }
    else if (obj == m_pianoView && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            m_smartNotes.erase(std::remove_if(m_smartNotes.begin(), m_smartNotes.end(),
                                              [](const SmartNote& n){ return n.selected; }), m_smartNotes.end());
            updatePianoRoll(); return true;
        }
        else if (ke->matches(QKeySequence::Copy)) {
            m_clipboard.clear();
            int minPos = 2147483647;
            for (const auto& n : m_smartNotes) { if (n.selected) { m_clipboard.push_back(n); if (n.pos < minPos) minPos = n.pos; } }
            for (auto& c : m_clipboard) c.pos -= minPos;
            return true;
        }
        else if (ke->matches(QKeySequence::Paste) && !m_clipboard.empty()) {
            QPointF scenePos = m_pianoView->mapToScene(m_pianoView->mapFromGlobal(QCursor::pos()));
            int pasteX = std::max(0, (int)((scenePos.x() - 60) / 48) * 48);
            for (auto& note : m_smartNotes) note.selected = false;
            for (auto c : m_clipboard) { c.id = getNextId(); c.pos += pasteX; c.selected = true; m_smartNotes.push_back(c); }
            updatePianoRoll(); return true;
        }
        else if (ke->modifiers() == Qt::AltModifier) {
            if (ke->key() == Qt::Key_S) { onStrumClicked(); return true; }
            if (ke->key() == Qt::Key_U) { onChopperClicked(); return true; }
            if (ke->key() == Qt::Key_A) { onArpeggiatorClicked(); return true; }
            if (ke->key() == Qt::Key_F) { onFlamClicked(); return true; }
            if (ke->key() == Qt::Key_R) { onRandomizerClicked(); return true; }
            if (ke->key() == Qt::Key_M) { onRiffMachineClicked(); return true; }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SmartPatternEditor::onSelectionChanged() {
    for (auto& note : m_smartNotes) note.selected = false;
    for (QGraphicsItem* item : m_scene->selectedItems()) {
        int id = item->data(0).toInt();
        for (auto& note : m_smartNotes) { if (note.id == id) { note.selected = true; break; } }
    }
}


void SmartPatternEditor::onReverseClicked() {
    int minPos = 2147483647, maxEnd = 0;
    for (const auto& n : m_smartNotes) { if (n.selected) { if (n.pos < minPos) minPos = n.pos; if (n.pos + n.len > maxEnd) maxEnd = n.pos + n.len; } }
    for (auto& n : m_smartNotes) { if (n.selected) n.pos = maxEnd - (n.pos - minPos) - n.len; }
    updatePianoRoll();
}

void SmartPatternEditor::onModulateClicked() {
    bool ok; int semi = QInputDialog::getInt(this, "Modulate", "Semitones:", 2, -24, 24, 1, &ok);
    if (!ok) return;
    for (auto& n : m_smartNotes) { if (n.selected) n.key = std::clamp(n.key + semi, 0, 127); }
    updatePianoRoll();
}

void SmartPatternEditor::onStrumClicked() {
    std::vector<SmartNote*> sel;
    for (auto& n : m_smartNotes) if (n.selected) sel.push_back(&n);
    if (sel.empty()) return;
    std::sort(sel.begin(), sel.end(), [](SmartNote* a, SmartNote* b) { return a->key < b->key; });

    int strumDelay = 12, currentStart = sel[0]->pos, idx = 0;
    for (SmartNote* note : sel) {
        if (std::abs(note->pos - currentStart) > 24) { currentStart = note->pos; idx = 0; }
        note->pos += (idx * strumDelay);
        note->len = std::max(12, note->len - (idx * strumDelay));
        idx++;
    }
    updatePianoRoll();
}

void SmartPatternEditor::onChopperClicked() {
    std::vector<SmartNote> newNotes;
    for (const auto& note : m_smartNotes) {
        if (note.selected && note.len >= 24) {
            int chops = 4;
            int newLen = note.len / chops;
            for (int i = 0; i < chops; ++i) {
                SmartNote sn = note;
                sn.id = getNextId();
                sn.pos = note.pos + (i * newLen);
                sn.len = newLen;
                newNotes.push_back(sn);
            }
        } else {
            newNotes.push_back(note);
        }
    }
    m_smartNotes = newNotes;
    updatePianoRoll();
}

void SmartPatternEditor::onArpeggiatorClicked() {
    std::vector<SmartNote*> sel;
    for (auto& n : m_smartNotes) if (n.selected) sel.push_back(&n);
    if (sel.empty()) return;
    std::sort(sel.begin(), sel.end(), [](SmartNote* a, SmartNote* b) { return a->key < b->key; });

    int arpStep = 48;
    int startPos = sel[0]->pos;
    for (int i = 0; i < sel.size(); ++i) {
        sel[i]->pos = startPos + (i * arpStep);
        sel[i]->len = arpStep;
    }
    updatePianoRoll();
}

void SmartPatternEditor::onFlamClicked() {
    std::vector<SmartNote> flams;
    for (auto& note : m_smartNotes) {
        if (note.selected) {
            SmartNote flamNote = note;
            flamNote.id = getNextId();
            flamNote.pos = std::max(0, note.pos - 12);
            flamNote.len = 12;
            flamNote.vol = std::max(10, note.vol - 40);
            flams.push_back(flamNote);
        }
    }
    m_smartNotes.insert(m_smartNotes.end(), flams.begin(), flams.end());
    updatePianoRoll();
}

void SmartPatternEditor::onRandomizerClicked() {
    for (auto& note : m_smartNotes) {
        if (note.selected) {
            int jitter = QRandomGenerator::global()->bounded(-6, 6);
            note.pos = std::max(0, note.pos + jitter);
            int volJitter = QRandomGenerator::global()->bounded(-15, 15);
            note.vol = std::clamp(note.vol + volJitter, 10, 127);
        }
    }
    updatePianoRoll();
}

void SmartPatternEditor::onRiffMachineClicked() {
    int startPos = 0;
    if (!m_smartNotes.empty()) {
        for (const auto& n : m_smartNotes) if (n.pos + n.len > startPos) startPos = n.pos + n.len;
    }
    startPos = std::ceil((double)startPos / 768.0) * 768;

    std::vector<int> scale = {60, 63, 65, 67, 70, 72, 75};
    for (auto& note : m_smartNotes) note.selected = false;

    for (int i = 0; i < 8; ++i) {
        int randKey = scale[QRandomGenerator::global()->bounded((int)scale.size())];
        int randPos = startPos + (QRandomGenerator::global()->bounded(16) * 48);
        int randLen = (QRandomGenerator::global()->bounded(1, 4)) * 48;

        m_smartNotes.push_back({getNextId(), randKey, randPos, randLen, 100, true});
    }
    updatePianoRoll();
}

void SmartPatternEditor::onScriptingApiClicked() {
    QMessageBox::information(this, "API Integration", "The internal data structures are ready. WIP");
}
