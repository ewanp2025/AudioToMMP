#ifndef HOUSEPIANOSTABIFIER_H
#define HOUSEPIANOSTABIFIER_H

#include <QWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QTableWidget>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <vector>
#include <QtXml>

struct PianoNote {
    int key = 60;
    int pos = 0;
    int len = 24;
    int vol = 100;
    int pan = 0;
};

class HousePianoStabifier : public QWidget
{
    Q_OBJECT

public:
    explicit HousePianoStabifier(QWidget *parent = nullptr);

private slots:
    void onLoadClicked();
    void onGenerateStabsClicked();
    void onSaveClicked();
    void onClearClicked();

private:
    void setupUI();
    void loadXpt(const QString &filePath);
    void saveXpt(const QString &filePath);
    void updateTable();
    std::vector<int> getChordNotes(int rootMidi, const QString& type);
    std::vector<int> getTriad(const std::vector<int>& fullChord);

    std::vector<PianoNote> m_notes;


    QComboBox *m_comboRoot1;
    QComboBox *m_comboType1;
    QSpinBox *m_spinPos1;

    QComboBox *m_comboRoot2;
    QComboBox *m_comboType2;
    QSpinBox *m_spinPos2;

    QComboBox *m_comboGenMode;
    QSlider *m_sliderRhythmDensity;
    QSlider *m_sliderVoicingThinning;
    QSlider *m_sliderArticulation;

    QCheckBox *m_chkTriadAnchors;
    QCheckBox *m_chkDynamicLengths;
    QComboBox *m_comboGroove;

    QPushButton *m_btnLoad;
    QPushButton *m_btnGenerate;
    QPushButton *m_btnSave;
    QPushButton *m_btnClear;
    QTableWidget *m_table;
};

#endif // HOUSEPIANOSTABIFIER_H
