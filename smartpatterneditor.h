#ifndef SMARTPATTERNEDITOR_H
#define SMARTPATTERNEDITOR_H

//#pragma push_macro("slots")
//#undef slots
//#include <pybind11/pybind11.h>
//#include <pybind11/embed.h>
//#include <pybind11/stl.h>
//#pragma pop_macro("slots")

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtXml>
#include <QRandomGenerator>
#include <QDialog>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSplitter>
#include <QDir>
#include <QScrollBar>
#include <QSpinBox>
#include <QCoreApplication>
#include <QDebug>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

//namespace py = pybind11;

struct SmartNote {
    int id;
    int number;
    int time;
    int length;
    int group = 0;
    float pan = 0.0f;
    float velocity = 0.8f;
    float release = 0.5f;
    int color = 0;
    float fcut = 0.5f;
    float fres = 0.5f;
    int pitchofs = 0;       // Cents (-1200 to 1200)
    bool slide = false;
    bool porta = false;
    bool muted = false;
    bool selected = false;
    int repeats = 0;
};

struct SmartMarker {
    int time;
    std::string name;
    int mode = 0;
    int tsnum = 4;
    int tsden = 4;
    int scale_root = 0;
    std::string scale_helper = "0,1,0,1,0,0,1,0,1,0,1,0";
};

class SmartPatternEditor;

class ScoreManager {
public:
    SmartPatternEditor* editor = nullptr;
    int getPPQ() const { return 96; }

    int getNoteCount() const;
    SmartNote* getNote(int index);
    void addNote(SmartNote note);
    void deleteNote(int index);
    void clearNotes(bool all = false);

    int getMarkerCount() const;
    SmartMarker* getMarker(int index);
    void addMarker(SmartMarker marker);
    void deleteMarker(int index);
    void clearMarkers(bool all = false);
    int getLength() const;

    std::tuple<int, int> getTimelineSelection();
};

class ScriptDialogWrapper {
public:
    QDialog* qtDialog;
    QVBoxLayout* layout;
    std::map<std::string, QWidget*> widgets;
    SmartPatternEditor* editor = nullptr;

//    py::object pythonForm;
//    py::function pythonApply;

    ScriptDialogWrapper(std::string title);
    ~ScriptDialogWrapper() { delete qtDialog; }

    void addInput(std::string name, double defaultVal);
    void addInputKnobInt(std::string name, int defaultVal, int min, int max);
    void addInputCombo(std::string name, std::string valueList, int defaultIndex);
    void addInputCheckbox(std::string name, bool defaultVal);
    void addInputText(std::string name, std::string defaultText);

//    py::object getInputValue(std::string name);
    void triggerPythonApply();
    void AddInputSurface(std::string name);
};

class SmartPatternEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SmartPatternEditor(QWidget *parent = nullptr);
    ~SmartPatternEditor();

    std::vector<SmartNote> m_smartNotes;
    std::vector<SmartMarker> m_smartMarkers;

    void appendLog(const QString& msg);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onLoadClicked();
    void onSaveClicked();
    void onSelectionChanged();

    void onReverseClicked();
    void onModulateClicked();

    void onStrumClicked();
    void onChopperClicked();
    void onArpeggiatorClicked();
    void onFlamClicked();
    void onRandomizerClicked();
    void onRiffMachineClicked();

    void scanScriptsFolder();
    void onRunSelectedScript();
    void onToggleConsole();
    void onGridSettingsChanged();

private:
    void setupUI();
    void updatePianoRoll();
    void updateAutoPane();
    void detectAndDisplayScale();
    int getNextId();
    int getSnapTicks();

    QGraphicsView *m_pianoView;
    QGraphicsScene *m_scene;

    QGraphicsView *m_autoView;
    QGraphicsScene *m_autoScene;
    QSplitter *m_editorSplitter;

    QLabel *m_lblDetectedScale;

    QSpinBox *m_spinBars;
    QComboBox *m_comboSnap;
    QComboBox *m_comboNoteLength;
    QComboBox *m_comboAutoMode;
    QComboBox *m_comboScripts;
    QTextEdit *m_scriptConsole;
    QSplitter *m_mainSplitter;

    std::vector<SmartNote> m_clipboard;
    std::vector<SmartNote> m_backupNotes;

    int m_highestId = 0;
    ScoreManager m_scoreManager;

    friend class ScoreManager;
    friend class ScriptDialogWrapper;
};

#endif // SMARTPATTERNEDITOR_H
