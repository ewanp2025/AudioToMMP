#include "smartpatterneditor.h"
//#include <pybind11/eval.h>

ScoreManager* g_scoreManager = nullptr;
SmartPatternEditor* g_currentEditor = nullptr;

//PYBIND11_EMBEDDED_MODULE(flpianoroll, m) {
//    py::class_<SmartNote>(m, "Note")
//    .def(py::init<>())
//        .def_readwrite("number", &SmartNote::number)
//        .def_readwrite("time", &SmartNote::time)
//        .def_readwrite("length", &SmartNote::length)
//        .def_readwrite("group", &SmartNote::group)
//        .def_readwrite("pan", &SmartNote::pan)
//        .def_readwrite("velocity", &SmartNote::velocity)
//        .def_readwrite("release", &SmartNote::release)
//        .def_readwrite("color", &SmartNote::color)
//        .def_readwrite("fcut", &SmartNote::fcut)
//        .def_readwrite("fres", &SmartNote::fres)
//        .def_readwrite("pitchofs", &SmartNote::pitchofs)
//        .def_readwrite("slide", &SmartNote::slide)
//        .def_readwrite("porta", &SmartNote::porta)
//        .def_readwrite("muted", &SmartNote::muted)
//        .def_readwrite("selected", &SmartNote::selected)
//        .def_readwrite("repeats", &SmartNote::repeats)
//        .def("clone", [](const SmartNote& self) {
//            SmartNote copy = self;
//            return copy;
//        });

//    py::class_<SmartMarker>(m, "Marker")
//        .def(py::init<>())
//        .def_readwrite("time", &SmartMarker::time)
//        .def_readwrite("name", &SmartMarker::name)
//        .def_readwrite("mode", &SmartMarker::mode)
//        .def_readwrite("tsnum", &SmartMarker::tsnum)
//        .def_readwrite("tsden", &SmartMarker::tsden)
//        .def_readwrite("scale_root", &SmartMarker::scale_root)
//        .def_readwrite("scale_helper", &SmartMarker::scale_helper);

//    py::class_<ScoreManager>(m, "Score")
//        .def_property_readonly("PPQ", &ScoreManager::getPPQ)
//        .def_property_readonly("noteCount", &ScoreManager::getNoteCount)
//        .def("getNote", &ScoreManager::getNote, py::return_value_policy::reference)
//        .def("addNote", &ScoreManager::addNote)
//        .def("deleteNote", &ScoreManager::deleteNote)
//        .def("clearNotes", &ScoreManager::clearNotes)
//        .def_property_readonly("markerCount", &ScoreManager::getMarkerCount)
//        .def("getMarker", &ScoreManager::getMarker, py::return_value_policy::reference)
//        .def("addMarker", &ScoreManager::addMarker)
//        .def("deleteMarker", &ScoreManager::deleteMarker)
//        .def("clearMarkers", &ScoreManager::clearMarkers)
//        .def("getTimelineSelection", &ScoreManager::getTimelineSelection)
//        .def_property_readonly("length", &ScoreManager::getLength);

//    py::class_<ScriptDialogWrapper>(m, "ScriptDialog")
//        .def(py::init<std::string>())
//        .def("addInput", &ScriptDialogWrapper::addInput)
//        .def("addInputKnobInt", &ScriptDialogWrapper::addInputKnobInt)
//        .def("addInputCombo", &ScriptDialogWrapper::addInputCombo)
//        .def("addInputCheckbox", &ScriptDialogWrapper::addInputCheckbox)
//        .def("addInputText", &ScriptDialogWrapper::addInputText)
//        .def("getInputValue", &ScriptDialogWrapper::getInputValue)
//        .def("AddInputSurface", &ScriptDialogWrapper::AddInputSurface);
//
//    auto utils = m.def_submodule("Utils");
//    utils.def("ShowMessage", [](std::string msg) {
//        QMessageBox::information(nullptr, "Script Message", QString::fromStdString(msg));
//    });
//    utils.def("log", [](std::string msg) {
//        if (g_currentEditor) g_currentEditor->appendLog(QString::fromStdString(msg));
//        qDebug() << "Python Script Log:" << QString::fromStdString(msg);
//    });
//    utils.def("ProgressMsg", [](std::string msg, int pos, int total) {
//        if (g_currentEditor) g_currentEditor->appendLog(QString("[%1/%2] %3").arg(pos).arg(total).arg(QString::fromStdString(msg)));
//    });
//
//    m.def("get_score", []() { return g_scoreManager; }, py::return_value_policy::reference);
//}

ScriptDialogWrapper::ScriptDialogWrapper(std::string title) {
    qtDialog = new QDialog();
    qtDialog->setWindowTitle(QString::fromStdString(title));
    qtDialog->setMinimumWidth(300);
    layout = new QVBoxLayout(qtDialog);

    QPushButton* btnAccept = new QPushButton("Accept");
    QPushButton* btnCancel = new QPushButton("Cancel");
    QHBoxLayout* btns = new QHBoxLayout();
    btns->addWidget(btnAccept); btns->addWidget(btnCancel);
    layout->addLayout(btns);

    QObject::connect(btnAccept, &QPushButton::clicked, qtDialog, &QDialog::accept);
    QObject::connect(btnCancel, &QPushButton::clicked, qtDialog, &QDialog::reject);
}

void ScriptDialogWrapper::addInput(std::string name, double defaultVal) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QString::fromStdString(name)));
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 1000);
    slider->setValue(defaultVal * 1000);
    row->addWidget(slider);
    layout->insertLayout(layout->count() - 1, row);
    widgets[name] = slider;
    QObject::connect(slider, &QSlider::valueChanged, [this]() { this->triggerPythonApply(); });
}

void ScriptDialogWrapper::addInputKnobInt(std::string name, int defaultVal, int min, int max) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QString::fromStdString(name)));
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(defaultVal);
    row->addWidget(slider);
    layout->insertLayout(layout->count() - 1, row);
    widgets[name] = slider;
    QObject::connect(slider, &QSlider::valueChanged, [this]() { this->triggerPythonApply(); });
}

void ScriptDialogWrapper::addInputCombo(std::string name, std::string valueList, int defaultIndex) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QString::fromStdString(name)));
    QComboBox* combo = new QComboBox();
    QStringList items = QString::fromStdString(valueList).split(",");
    combo->addItems(items);
    combo->setCurrentIndex(defaultIndex);
    row->addWidget(combo);
    layout->insertLayout(layout->count() - 1, row);
    widgets[name] = combo;
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this]() { this->triggerPythonApply(); });
}

void ScriptDialogWrapper::addInputCheckbox(std::string name, bool defaultVal) {
    QCheckBox* cb = new QCheckBox(QString::fromStdString(name));
    cb->setChecked(defaultVal);
    layout->insertWidget(layout->count() - 1, cb);
    widgets[name] = cb;

    QObject::connect(cb, &QCheckBox::checkStateChanged, [this]() { this->triggerPythonApply(); });
}

void ScriptDialogWrapper::addInputText(std::string name, std::string defaultText) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QString::fromStdString(name)));
    QLineEdit* lineEdit = new QLineEdit(QString::fromStdString(defaultText));
    row->addWidget(lineEdit);
    layout->insertLayout(layout->count() - 1, row);
    widgets[name] = lineEdit;
    QObject::connect(lineEdit, &QLineEdit::textChanged, [this]() { this->triggerPythonApply(); });
}

//py::object ScriptDialogWrapper::getInputValue(std::string name) {

//    if (widgets.find(name) == widgets.end()) {
//        return py::cast(0.0);
//    }

//    QWidget* w = widgets[name];
//    if (auto* slider = qobject_cast<QSlider*>(w)) {
//        if (slider->maximum() == 1000 && slider->minimum() == 0) return py::cast(slider->value() / 1000.0);
//        return py::cast(slider->value());
//    }
//    if (auto* cb = qobject_cast<QCheckBox*>(w)) return py::cast(cb->isChecked());
//    if (auto* combo = qobject_cast<QComboBox*>(w)) return py::cast(combo->currentIndex());
//    if (auto* lineEdit = qobject_cast<QLineEdit*>(w)) return py::cast(lineEdit->text().toStdString());
//    return py::cast(0.0);
//}

void ScriptDialogWrapper::triggerPythonApply() {
 //   if (!pythonApply.is_none() && editor) {
//        editor->m_smartNotes = editor->m_backupNotes;
//        try {
//            pythonApply(pythonForm);
//            editor->updatePianoRoll();
//        } catch(py::error_already_set& e) {

//        }
//    }
}


int ScoreManager::getNoteCount() const { return editor->m_smartNotes.size(); }
SmartNote* ScoreManager::getNote(int index) {
    if(index >= 0 && index < editor->m_smartNotes.size()) return &editor->m_smartNotes[index];
    return nullptr;
}
void ScoreManager::addNote(SmartNote note) { editor->m_smartNotes.push_back(note); }
void ScoreManager::deleteNote(int index) {
    if(index >= 0 && index < editor->m_smartNotes.size()) editor->m_smartNotes.erase(editor->m_smartNotes.begin() + index);
}
void ScoreManager::clearNotes(bool all) {
    if (all) { editor->m_smartNotes.clear(); }
    else {
        editor->m_smartNotes.erase(
            std::remove_if(editor->m_smartNotes.begin(), editor->m_smartNotes.end(), [](const SmartNote& n){ return n.selected; }),
            editor->m_smartNotes.end()
            );
    }
}

int ScoreManager::getMarkerCount() const { return editor->m_smartMarkers.size(); }
SmartMarker* ScoreManager::getMarker(int index) {
    if(index >= 0 && index < editor->m_smartMarkers.size()) return &editor->m_smartMarkers[index];
    return nullptr;
}
void ScoreManager::addMarker(SmartMarker marker) { editor->m_smartMarkers.push_back(marker); }
void ScoreManager::deleteMarker(int index) {
    if(index >= 0 && index < editor->m_smartMarkers.size()) editor->m_smartMarkers.erase(editor->m_smartMarkers.begin() + index);
}
void ScoreManager::clearMarkers(bool all) { editor->m_smartMarkers.clear(); }

std::tuple<int, int> ScoreManager::getTimelineSelection() {
    int minT = 2147483647, maxT = -1;
    for(auto& n : editor->m_smartNotes) {
        if(n.selected) {
            if(n.time < minT) minT = n.time;
            if(n.time + n.length > maxT) maxT = n.time + n.length;
        }
    }
    if (maxT == -1) return std::make_tuple(0, -1);
    return std::make_tuple(minT, maxT);
}

int ScoreManager::getLength() const {
    int maxL = 0;
    for (const auto& n : editor->m_smartNotes) {
        if (n.time + n.length > maxL) maxL = n.time + n.length;
    }
    return maxL > 0 ? maxL : 384;
}

void ScriptDialogWrapper::AddInputSurface(std::string name) {
    if (editor) editor->appendLog("Notice: .fst Control Surfaces are ignored. Running with default values.");
}

SmartPatternEditor::SmartPatternEditor(QWidget *parent) : QWidget(parent)
{
    m_scoreManager.editor = this;
    g_currentEditor = this;

//    static bool py_initialized = false;
//    if (!py_initialized) {
//        py::initialize_interpreter();
//        py_initialized = true;
//    }
    setupUI();
}

SmartPatternEditor::~SmartPatternEditor() {
    if (g_currentEditor == this) g_currentEditor = nullptr;
}

void SmartPatternEditor::setupUI()
{
    QVBoxLayout *smartLayout = new QVBoxLayout(this);


    QHBoxLayout *row1 = new QHBoxLayout();
    QPushButton *btnLoad = new QPushButton("Load .xpt");
    QPushButton *btnSave = new QPushButton("Save .xpt");

    m_spinBars = new QSpinBox();
    m_spinBars->setRange(1, 128);
    m_spinBars->setValue(4);
    m_spinBars->setPrefix("Bars: ");

    m_comboSnap = new QComboBox();
    m_comboSnap->addItem("Snap: Line (Auto)", -1);
    m_comboSnap->addItem("Snap: None", 1);
    m_comboSnap->addItem("Snap: 1/64", 6);
    m_comboSnap->addItem("Snap: 1/32", 12);
    m_comboSnap->addItem("Snap: 1/16", 24);
    m_comboSnap->addItem("Snap: 1/8", 48);
    m_comboSnap->addItem("Snap: 1/4 (Beat)", 96);
    m_comboSnap->addItem("Snap: 1 Bar", 384);
    m_comboSnap->setCurrentIndex(4); // Default 1/16

    m_comboNoteLength = new QComboBox();
    m_comboNoteLength->addItem("Len: 1/32", 12);
    m_comboNoteLength->addItem("Len: 1/16", 24);
    m_comboNoteLength->addItem("Len: 1/8", 48);
    m_comboNoteLength->addItem("Len: 1/4", 96);
    m_comboNoteLength->addItem("Len: 1/2", 192);
    m_comboNoteLength->addItem("Len: 1 Bar", 384);
    m_comboNoteLength->setCurrentIndex(1); // Default 1/16

    row1->addWidget(btnLoad); row1->addWidget(btnSave);
    row1->addSpacing(20);
    row1->addWidget(m_spinBars);
    row1->addWidget(m_comboSnap);
    row1->addWidget(m_comboNoteLength);
    row1->addStretch();

    m_lblDetectedScale = new QLabel("<b>Scale:</b> N/A");
    m_lblDetectedScale->setStyleSheet("background-color: #222; color: #00ff00; padding: 5px; border-radius: 3px; font-family: monospace; font-size: 14px;");
    row1->addWidget(m_lblDetectedScale);
    smartLayout->addLayout(row1);


    QHBoxLayout *row2 = new QHBoxLayout();
    QPushButton *btnReverse = new QPushButton("Reverse");
    QPushButton *btnModulate = new QPushButton("Modulate");
    btnReverse->setStyleSheet("background-color: #555; color: white;");
    btnModulate->setStyleSheet("background-color: #8b008b; color: white;");

    QPushButton *btnStrum = new QPushButton("Strum (Alt+S)");
    QPushButton *btnChop = new QPushButton("Chopper (Alt+U)");
    QPushButton *btnArp = new QPushButton("Arpeggiate (Alt+A)");
    QPushButton *btnFlam = new QPushButton("Flam (Alt+F)");
    QPushButton *btnRandom = new QPushButton("Humanize (Alt+R)");
    QPushButton *btnRiff = new QPushButton("Riff Machine (Alt+M)");

    QString btnStyle = "background-color: #d2691e; color: white; font-weight: bold;";
    btnStrum->setStyleSheet(btnStyle); btnChop->setStyleSheet(btnStyle);
    btnArp->setStyleSheet(btnStyle); btnFlam->setStyleSheet(btnStyle);
    btnRandom->setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold;");
    btnRiff->setStyleSheet("background-color: #00557f; color: white; font-weight: bold;");

    m_comboAutoMode = new QComboBox();

    m_comboAutoMode->addItems({"Event: Velocity", "Event: Pan"});
    connect(m_comboAutoMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){ updateAutoPane(); });

    row2->addWidget(btnReverse); row2->addWidget(btnModulate);
    row2->addSpacing(10);
    row2->addWidget(btnStrum); row2->addWidget(btnChop);
    row2->addWidget(btnArp); row2->addWidget(btnFlam);
    row2->addWidget(btnRandom); row2->addWidget(btnRiff);
    row2->addStretch();
    row2->addWidget(m_comboAutoMode);
    smartLayout->addLayout(row2);

    // --- ROW 3: Python Scripting Engine ---
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->addWidget(new QLabel("<b>Python Macros:</b>"));

    m_comboScripts = new QComboBox();
    m_comboScripts->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPushButton *btnRefreshScripts = new QPushButton("↻");


    QPushButton *btnRunScript = new QPushButton("SCRIPT WIP");
    btnRunScript->setStyleSheet("background-color: #222; color: #888; font-family: monospace; border: 2px solid #555; font-weight: bold;");

    QPushButton *btnToggleConsole = new QPushButton("Toggle Console");

    row3->addWidget(m_comboScripts);
    row3->addWidget(btnRefreshScripts);
    row3->addWidget(btnRunScript);
    row3->addSpacing(20);
    row3->addWidget(btnToggleConsole);
    smartLayout->addLayout(row3);



    m_editorSplitter = new QSplitter(Qt::Vertical);

    m_scene = new QGraphicsScene(this);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &SmartPatternEditor::onSelectionChanged);

    m_pianoView = new QGraphicsView(m_scene);
    m_pianoView->setRenderHint(QPainter::Antialiasing, false);
    m_pianoView->setBackgroundBrush(QColor(30, 30, 30));
    m_pianoView->setDragMode(QGraphicsView::RubberBandDrag);
    m_pianoView->installEventFilter(this);
    m_pianoView->viewport()->installEventFilter(this);

    m_autoScene = new QGraphicsScene(this);
    m_autoView = new QGraphicsView(m_autoScene);
    m_autoView->setRenderHint(QPainter::Antialiasing, false);
    m_autoView->setBackgroundBrush(QColor(20, 20, 20));
    m_autoView->setMinimumHeight(60);
    m_autoView->installEventFilter(this);
    m_autoView->viewport()->installEventFilter(this);


    m_autoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_pianoView->horizontalScrollBar(), &QScrollBar::valueChanged, m_autoView->horizontalScrollBar(), &QScrollBar::setValue);
    connect(m_autoView->horizontalScrollBar(), &QScrollBar::valueChanged, m_pianoView->horizontalScrollBar(), &QScrollBar::setValue);

    m_editorSplitter->addWidget(m_pianoView);
    m_editorSplitter->addWidget(m_autoView);
    m_editorSplitter->setStretchFactor(0, 1);
    m_editorSplitter->setStretchFactor(1, 0);


    m_mainSplitter = new QSplitter(Qt::Vertical);
    m_scriptConsole = new QTextEdit();
    m_scriptConsole->setReadOnly(true);
    m_scriptConsole->setStyleSheet("background-color: #111; color: #00ff00; font-family: monospace;");
    m_scriptConsole->setVisible(false);

    m_mainSplitter->addWidget(m_editorSplitter);
    m_mainSplitter->addWidget(m_scriptConsole);
    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 1);

    smartLayout->addWidget(new QLabel("<i>(<b>Double-Click</b> to add notes. <b>Right-Click</b> to delete. <b>Drag bottom pane</b> to adjust Note Events)</i>"));
    smartLayout->addWidget(m_mainSplitter);


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

    connect(btnRefreshScripts, &QPushButton::clicked, this, &SmartPatternEditor::scanScriptsFolder);
    connect(btnToggleConsole, &QPushButton::clicked, this, &SmartPatternEditor::onToggleConsole);
    connect(m_spinBars, &QSpinBox::valueChanged, this, &SmartPatternEditor::onGridSettingsChanged);



    scanScriptsFolder();
    updatePianoRoll();
}

void SmartPatternEditor::appendLog(const QString& msg) {
    m_scriptConsole->append(msg);
    if (!m_scriptConsole->isVisible()) {
        m_scriptConsole->setVisible(true);
    }
}

void SmartPatternEditor::onToggleConsole() {
    m_scriptConsole->setVisible(!m_scriptConsole->isVisible());
}

void SmartPatternEditor::scanScriptsFolder() {
    m_comboScripts->clear();
    QString scriptsPath = QCoreApplication::applicationDirPath() + "/scripts";
    QDir dir(scriptsPath);

    if (!dir.exists()) {
        dir.mkpath(".");
        m_comboScripts->addItem("No scripts found (Place .py in /scripts)", "");
        return;
    }

    QStringList filters; filters << "*.pyscript" << "*.py";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty()) {
        m_comboScripts->addItem("No scripts found in /scripts", "");
    } else {
        for (const QFileInfo& file : files) {
            m_comboScripts->addItem(file.baseName(), file.absoluteFilePath());
        }
    }
}

void SmartPatternEditor::onRunSelectedScript() {
    QString fileName = m_comboScripts->currentData().toString();
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getOpenFileName(this, "Load FL Python Script", "", "FL Studio Python Script (*.pyscript *.py)");
        if (fileName.isEmpty()) return;
    }

    appendLog(QString("--- Running Script: %1 ---").arg(QFileInfo(fileName).fileName()));

    g_scoreManager = &m_scoreManager;
    m_backupNotes = m_smartNotes;

 //   try {
//        py::module_ flp = py::module_::import("flpianoroll");
//        flp.attr("score") = flp.attr("get_score")();
//
//        py::dict globals = py::globals();
//        globals["score"] = flp.attr("score"); // Inject globally for scripts

//        py::dict locals;
//        py::eval_file(fileName.toStdString(), globals, locals);

//        if (locals.contains("createDialog")) {
//            py::function createDialog = locals["createDialog"];
//            py::object formObj = createDialog();

//            ScriptDialogWrapper* form = formObj.cast<ScriptDialogWrapper*>();
//            form->editor = this;

//            if (locals.contains("apply")) {
//                form->pythonApply = locals["apply"];
//                form->pythonForm = formObj;
//                form->triggerPythonApply();
//            }

//            if (form->qtDialog->exec() == QDialog::Accepted) {
//                updatePianoRoll();
//                appendLog("Script applied successfully.");
//            } else {
//                m_smartNotes = m_backupNotes;
//                updatePianoRoll();
//                appendLog("Script canceled.");
//            }
//        } else {
//            if (locals.contains("apply")) {
//                py::function apply = locals["apply"];
//                apply(py::none());
//                updatePianoRoll();
//                appendLog("Script applied (No UI).");
//            }
//        }
//    } catch (py::error_already_set& e) {
//       appendLog(QString("<span style='color:red;'>Python Error: %1</span>").arg(e.what()));
//        m_smartNotes = m_backupNotes;
//        updatePianoRoll();
//    }
}

void SmartPatternEditor::onGridSettingsChanged() {
    updatePianoRoll();
}

int SmartPatternEditor::getNextId() { return ++m_highestId; }

int SmartPatternEditor::getSnapTicks() {
    int snap = m_comboSnap->currentData().toInt();
    if (snap == -1) {
        snap = m_comboNoteLength->currentData().toInt();
    }
    return snap > 0 ? snap : 1;
}

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
    int maxPos = 0;

    for (int i = 0; i < notes.count(); ++i) {
        QDomElement nElem = notes.at(i).toElement();
        SmartNote sn;
        sn.id = getNextId();
        sn.number = nElem.attribute("key").toInt();
        sn.time = nElem.attribute("pos").toInt() * 2;
        sn.length = nElem.attribute("len").toInt() * 2;
        sn.velocity = nElem.attribute("vol").toFloat() / 127.0f;

        sn.pan = nElem.attribute("pan", "0.0").toFloat();
        sn.color = nElem.attribute("fl_color", "0").toInt();
        sn.slide = nElem.attribute("fl_slide", "0").toInt() != 0;
        sn.pitchofs = nElem.attribute("fl_pitchofs", "0").toInt();
        sn.porta = nElem.attribute("fl_porta", "0").toInt() != 0;
        sn.muted = nElem.attribute("fl_muted", "0").toInt() != 0;

        sn.selected = true;
        m_smartNotes.push_back(sn);

        if (sn.time + sn.length > maxPos) maxPos = sn.time + sn.length;
    }

    int requiredBars = std::max(1, (int)std::ceil((double)maxPos / 384.0));
    m_spinBars->setValue(requiredBars);

    updatePianoRoll();
}

void SmartPatternEditor::onSaveClicked()
{
    if (m_smartNotes.empty()) { QMessageBox::warning(this, "Empty", "No notes to save!"); return; }

    QString fileName = QFileDialog::getSaveFileName(this, "Save LMMS Pattern", "SmartPattern.xpt", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    int maxEnd = 0;
    for (const auto& note : m_smartNotes) {
        if (note.time + note.length > maxEnd) maxEnd = note.time + note.length;
    }
    int totalSteps = std::max(16, (int)std::ceil((double)maxEnd / 48.0));

    QTextStream out(&file);
    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project creator=\"LMMS\" type=\"pattern\" version=\"20\" creatorversion=\"1.3.0\">\n";
    out << "  <head/>\n";
    out << "  <pattern muted=\"0\" steps=\"" << totalSteps << "\" type=\"1\" pos=\"0\" name=\"SmartPattern\">\n";

    for (const auto& note : m_smartNotes) {
        out << "    <note pan=\"" << note.pan
            << "\" key=\"" << note.number
            << "\" len=\"" << (note.length / 2)  // DOWN-SCALE
            << "\" pos=\"" << (note.time / 2)    // DOWN-SCALE
            << "\" vol=\"" << (int)(note.velocity * 127)
            << "\" fl_color=\"" << note.color
            << "\" fl_slide=\"" << (note.slide ? 1 : 0)
            << "\" fl_pitchofs=\"" << note.pitchofs
            << "\" fl_porta=\"" << (note.porta ? 1 : 0)
            << "\" fl_muted=\"" << (note.muted ? 1 : 0) << "\"/>\n";
    }

    out << "  </pattern>\n";
    out << "</lmms-project>\n";
    file.close();
}

void SmartPatternEditor::detectAndDisplayScale()
{
    if (m_smartNotes.empty()) { m_lblDetectedScale->setText("<b>Scale:</b> N/A"); return; }

    double chroma[12] = {0};
    for (const auto& note : m_smartNotes) { if (note.number >= 0) chroma[note.number % 12] += 1.0; }

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

        if (majScore > bestScore) { bestScore = majScore; bestScale = noteNames[root] + " Major"; }
        if (minScore > bestScore) { bestScore = minScore; bestScale = noteNames[root] + " Minor"; }
    }
    m_lblDetectedScale->setText(QString("<b>Scale:</b> %1").arg(bestScale));
}

void SmartPatternEditor::updateAutoPane()
{
    m_autoScene->clear();
    int totalBars = m_spinBars->value();
    int gridWidth = totalBars * 384;
    int gridStartX = 60;
    int paneHeight = 100;

    m_autoScene->setSceneRect(0, 0, gridWidth + gridStartX, paneHeight);

    for (int x = 0; x <= gridWidth; x += 24) {
        QPen linePen;
        if (x % 384 == 0) linePen = QPen(QColor(180, 180, 180, 150), 2);
        else if (x % 96 == 0) linePen = QPen(QColor(120, 120, 120, 100), 1);
        else linePen = QPen(QColor(70, 70, 70, 50), 1);
        m_autoScene->addLine(gridStartX + x, 0, gridStartX + x, paneHeight, linePen)->setZValue(-1);
    }

    int mode = m_comboAutoMode->currentIndex();


    if (mode == 1) {
        m_autoScene->addLine(gridStartX, paneHeight/2, gridStartX + gridWidth, paneHeight/2, QPen(QColor(255,255,255,100)))->setZValue(-1);
    }

    for (const auto& note : m_smartNotes) {
        double val = 0;
        if (mode == 0) val = note.velocity;
        else if (mode == 1) val = (note.pan + 1.0f) / 2.0f;


        val = std::clamp(val, 0.0, 1.0);
        double y = paneHeight - (val * paneHeight);

        QColor barColor = note.selected ? QColor(0, 255, 0, 180) : QColor(0, 150, 255, 180);
        int drawX = gridStartX + note.time;

        if (mode == 0) {
            m_autoScene->addRect(drawX, y, note.length > 10 ? 8 : 4, paneHeight - y, QPen(Qt::black), barColor);
        } else {
            double center = paneHeight / 2.0;
            double h = std::abs(center - y);
            double startY = y < center ? y : center;
            m_autoScene->addRect(drawX, startY, note.length > 10 ? 8 : 4, h < 2 ? 2 : h, QPen(Qt::black), barColor);
        }
    }
}

void SmartPatternEditor::updatePianoRoll()
{
    m_scene->clear();

    int keyHeight = 12;
    int totalBars = m_spinBars->value();
    int gridWidth = totalBars * 384;
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

    for (int x = 0; x <= gridWidth; x += 24) {
        QPen linePen;
        if (x % 384 == 0) linePen = QPen(QColor(180, 180, 180, 200), 2);
        else if (x % 96 == 0) linePen = QPen(QColor(120, 120, 120, 180), 1);
        else linePen = QPen(QColor(70, 70, 70, 100), 1);
        m_scene->addLine(gridStartX + x, 0, gridStartX + x, 128 * keyHeight, linePen)->setZValue(-1);
    }

    for (const auto& note : m_smartNotes) {
        QColor noteColor = note.selected ? QColor(0, 255, 0) : QColor(0, 150, 255);
        if (note.slide || note.porta) noteColor = note.selected ? QColor(255, 255, 0) : QColor(200, 100, 0);

        QGraphicsRectItem* item = m_scene->addRect(gridStartX + note.time, (127 - note.number) * keyHeight, note.length, keyHeight, QPen(Qt::black), noteColor);
        item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        item->setData(0, note.id);
        item->setSelected(note.selected);
    }

    detectAndDisplayScale();
    updateAutoPane();
}

bool SmartPatternEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_autoView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                QPointF scenePos = m_autoView->mapToScene(mouseEvent->pos());
                int sceneX = scenePos.x() - 60;
                int paneHeight = 100;

                double normalizedY = std::clamp(1.0 - (scenePos.y() / paneHeight), 0.0, 1.0);

                int mode = m_comboAutoMode->currentIndex();
                bool changed = false;

                for (auto& note : m_smartNotes) {
                    if (sceneX >= note.time - 4 && sceneX <= note.time + note.length + 4) {
                        if (mode == 0) {
                            note.velocity = normalizedY;
                        } else if (mode == 1) {
                            note.pan = (normalizedY * 2.0f) - 1.0f;
                        }

                        changed = true;
                    }
                }

                if (changed) updateAutoPane();
                return true;
            }
        }
    }
    else if (obj == m_pianoView->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPointF scenePos = m_pianoView->mapToScene(mouseEvent->pos());
            int num = 127 - (int)(scenePos.y() / 12);

            int snap = getSnapTicks();
            int defaultLen = m_comboNoteLength->currentData().toInt();
            int timePos = std::max(0, (int)std::round((scenePos.x() - 60) / snap) * snap);

            if (num >= 0 && num <= 127 && timePos >= 0) {
                SmartNote sn;
                sn.id = getNextId(); sn.number = num; sn.time = timePos;
                sn.length = defaultLen > 0 ? defaultLen : 24;
                sn.selected = true;
                m_smartNotes.push_back(sn);
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
        int snap = getSnapTicks();

        for (QGraphicsItem* item : m_scene->selectedItems()) {
            if (item->data(0).isValid()) {
                int id = item->data(0).toInt();
                double droppedX = item->sceneBoundingRect().x();
                double droppedY = item->sceneBoundingRect().y();

                for (auto& note : m_smartNotes) {
                    if (note.id == id) {
                        if (std::abs((droppedX - 60) - note.time) > 2.0 || std::abs(droppedY - (127 - note.number) * 12.0) > 2.0) {
                            int snappedX = std::max(0, (int)std::round((droppedX - 60) / snap) * snap);
                            int newKey = std::clamp(127 - (int)std::round(droppedY / 12.0), 0, 127);

                            if (note.time != snappedX || note.number != newKey) {
                                note.time = snappedX;
                                note.number = newKey;
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
            m_smartNotes.erase(std::remove_if(m_smartNotes.begin(), m_smartNotes.end(), [](const SmartNote& n){ return n.selected; }), m_smartNotes.end());
            updatePianoRoll(); return true;
        }
        else if (ke->matches(QKeySequence::Copy)) {
            m_clipboard.clear();
            int minPos = 2147483647;
            for (const auto& n : m_smartNotes) { if (n.selected) { m_clipboard.push_back(n); if (n.time < minPos) minPos = n.time; } }
            for (auto& c : m_clipboard) c.time -= minPos;
            return true;
        }
        else if (ke->matches(QKeySequence::Paste) && !m_clipboard.empty()) {
            QPointF scenePos = m_pianoView->mapToScene(m_pianoView->mapFromGlobal(QCursor::pos()));
            int snap = getSnapTicks();
            int pasteX = std::max(0, (int)std::round((scenePos.x() - 60) / snap) * snap);
            for (auto& note : m_smartNotes) note.selected = false;
            for (auto c : m_clipboard) { c.id = getNextId(); c.time += pasteX; c.selected = true; m_smartNotes.push_back(c); }
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
    updateAutoPane();
}

void SmartPatternEditor::onReverseClicked() {
    int minPos = 2147483647, maxEnd = 0;
    for (const auto& n : m_smartNotes) { if (n.selected) { if (n.time < minPos) minPos = n.time; if (n.time + n.length > maxEnd) maxEnd = n.time + n.length; } }
    for (auto& n : m_smartNotes) { if (n.selected) n.time = maxEnd - (n.time - minPos) - n.length; }
    updatePianoRoll();
}

void SmartPatternEditor::onModulateClicked() {
    bool ok; int semi = QInputDialog::getInt(this, "Modulate", "Semitones:", 2, -24, 24, 1, &ok);
    if (!ok) return;
    for (auto& n : m_smartNotes) { if (n.selected) n.number = std::clamp(n.number + semi, 0, 127); }
    updatePianoRoll();
}

void SmartPatternEditor::onStrumClicked() {
    std::vector<SmartNote*> sel;
    for (auto& n : m_smartNotes) if (n.selected) sel.push_back(&n);
    if (sel.empty()) return;
    std::sort(sel.begin(), sel.end(), [](SmartNote* a, SmartNote* b) { return a->number < b->number; });

    int strumDelay = 12, currentStart = sel[0]->time, idx = 0;
    for (SmartNote* note : sel) {
        if (std::abs(note->time - currentStart) > 24) { currentStart = note->time; idx = 0; }
        note->time += (idx * strumDelay);
        note->length = std::max(12, note->length - (idx * strumDelay));
        idx++;
    }
    updatePianoRoll();
}

void SmartPatternEditor::onChopperClicked() {
    std::vector<SmartNote> newNotes;
    for (const auto& note : m_smartNotes) {
        if (note.selected && note.length >= 24) {
            int chops = 4;
            int newLen = note.length / chops;
            for (int i = 0; i < chops; ++i) {
                SmartNote sn = note;
                sn.id = getNextId();
                sn.time = note.time + (i * newLen);
                sn.length = newLen;
                newNotes.push_back(sn);
            }
        } else { newNotes.push_back(note); }
    }
    m_smartNotes = newNotes;
    updatePianoRoll();
}

void SmartPatternEditor::onArpeggiatorClicked() {
    std::vector<SmartNote*> sel;
    for (auto& n : m_smartNotes) if (n.selected) sel.push_back(&n);
    if (sel.empty()) return;
    std::sort(sel.begin(), sel.end(), [](SmartNote* a, SmartNote* b) { return a->number < b->number; });

    int arpStep = 48;
    int startPos = sel[0]->time;
    for (size_t i = 0; i < sel.size(); ++i) {
        sel[i]->time = startPos + (i * arpStep);
        sel[i]->length = arpStep;
    }
    updatePianoRoll();
}

void SmartPatternEditor::onFlamClicked() {
    std::vector<SmartNote> flams;
    for (auto& note : m_smartNotes) {
        if (note.selected) {
            SmartNote flamNote = note;
            flamNote.id = getNextId();
            flamNote.time = std::max(0, note.time - 12);
            flamNote.length = 12;
            flamNote.velocity = std::max(0.1f, note.velocity - 0.3f);
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
            note.time = std::max(0, note.time + jitter);
            float volJitter = QRandomGenerator::global()->bounded(-15, 15) / 100.0f;
            note.velocity = std::clamp(note.velocity + volJitter, 0.1f, 1.0f);
        }
    }
    updatePianoRoll();
}

void SmartPatternEditor::onRiffMachineClicked() {
    int startPos = 0;
    if (!m_smartNotes.empty()) {
        for (const auto& n : m_smartNotes) if (n.time + n.length > startPos) startPos = n.time + n.length;
    }
    startPos = std::ceil((double)startPos / 384.0) * 384;

    std::vector<int> scale = {60, 63, 65, 67, 70, 72, 75};
    for (auto& note : m_smartNotes) note.selected = false;

    for (int i = 0; i < 8; ++i) {
        SmartNote sn;
        sn.id = getNextId();
        sn.number = scale[QRandomGenerator::global()->bounded((int)scale.size())];
        sn.time = startPos + (QRandomGenerator::global()->bounded(16) * 24);
        sn.length = (QRandomGenerator::global()->bounded(1, 4)) * 24;
        sn.selected = true;
        m_smartNotes.push_back(sn);
    }

    int maxEnd = startPos + 384;
    if (m_spinBars->value() * 384 < maxEnd) {
        m_spinBars->setValue((int)std::ceil(maxEnd / 384.0));
    }
    updatePianoRoll();
}


