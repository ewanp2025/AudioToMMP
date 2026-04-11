#ifndef SMARTPATTERNEDITOR_H
#define SMARTPATTERNEDITOR_H

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
#include <vector>
#include <algorithm>
#include <cmath>

struct SmartNote {
    int id;
    int key;
    int pos;
    int len;
    int vol;
    bool selected;
};

class SmartPatternEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SmartPatternEditor(QWidget *parent = nullptr);

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
    void onScriptingApiClicked();

private:
    void setupUI();
    void updatePianoRoll();
    void detectAndDisplayScale();
    int getNextId();

    QGraphicsView *m_pianoView;
    QGraphicsScene *m_scene;
    QLabel *m_lblDetectedScale;

    std::vector<SmartNote> m_smartNotes;
    std::vector<SmartNote> m_clipboard;

    int m_highestId = 0;
};

#endif // SMARTPATTERNEDITOR_H
