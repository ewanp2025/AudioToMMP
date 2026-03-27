#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTableWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include "qcustomplot.h"
#include <QDir>
#include <vector>
#include <QTabWidget>
#include <QProgressBar>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void exportLMMSSurgical();
    void exportLMMSProject();

    void onStepsChanged();
    void onBpmChanged();
    
    void onSpectrogramMousePress(QMouseEvent *event);
    void onSpectrogramMouseMove(QMouseEvent *event);
    void onSpectrogramMouseRelease(QMouseEvent *event);
    void updateBandVisuals();
    void onOffsetChanged();
    void autoDetectBPM();
    void playFilteredSelection();
    void onThresholdChanged();
    void saveFilteredSelection();
    void playSynthesizedPattern();
    void onAddBandClicked();
    void onDeleteBandClicked();
    void autoDetectOffset();
    void nudgeSelectionLeft();
    void nudgeSelectionRight();


    void onLoadSepFileClicked();
    void onProcessSeparationClicked();
    void onSaveSepWavClicked();

    void onPlaySepClicked();
    void onStopSepClicked();
    void updateSepPlaybackLine();


private:
    struct ExtractedNote {
        int midiNote = 0;
        QString chord = "";
        double startTick = 0.0;
        double lengthTicks = 48.0;
        int volume = 100;
        int visualStep = 0;
    };


    std::vector<ExtractedNote> m_surgicalNotes;
    QMediaPlayer *m_player;
    QTableWidget *m_bandTable;
    QProgressBar *m_progressBar;
    QAudioOutput *m_audioOutput;
    std::vector<float> m_lastFilteredAudio;
    float detectPitchHPS(const std::vector<float>& buffer);
    QString detectChordTemplate(const std::vector<float>& buffer);
    QLabel *m_lblInstrumentGuess;
    QString guessInstrument(double lowFreq, double highFreq);
    void setupUI();
    bool loadAudio(const QString &fileName);
    void generateSpectrogram();
    void drawGridLines();
    void processSelection();
    std::vector<double> chordToFrequencies(const QString& chordName);
    int extractADSR(const std::vector<float>& isolatedAudio);
    void updateStepGrid(const std::vector<float>& isolatedAudio);
    std::vector<float> applyBandpassFilter(const std::vector<float>& input, double lowFreq, double highFreq);

    float detectPitchYin(const std::vector<float>& buffer);
    int freqToMidi(float freq);
    QCustomPlot *m_multiSpectrogramPlot;
    QCPColorMap *m_multiSpectrogramMap;
    QList<QCPItemRect*> m_bandVisualRects;
    QCustomPlot *m_spectrogramPlot;
    QCPColorMap *m_spectrogramMap;
    QCPItemRect *m_selectionBox;
    QComboBox *m_comboAlgorithm;
    QComboBox *m_comboBpmType;
    
    QCustomPlot *m_waveformPlot;
    QTableWidget *m_stepTable;
    QTabWidget *m_mainTabs;
    QWidget *m_tabSurgical;
    QWidget *m_tabMultiBand;
    QLabel *m_lblBPM;
    QSpinBox *m_spinBPM;
    QComboBox *m_comboSteps;
    QLabel *m_lblADSR;
    QCheckBox *m_checkSnapToBar;
    QPushButton *m_btnDetectBPM;
    QString midiToNoteName(int midiNote);
    QString detectChord(const std::vector<float>& buffer);

    std::vector<float> m_audioData;
    int m_sampleRate = 44100;
    double m_bpm = 120.0;
    int m_numSteps = 16;
    double snapTimeToGrid(double rawTime);
    void updateSelectionVisuals();
    QPushButton *m_btnDetectOffset;
    QPushButton *m_btnNudgeLeft;
    QPushButton *m_btnNudgeRight;

    QDoubleSpinBox *m_spinThreshold;
    double m_noteThreshold = 40.0;
    
    bool m_isSelecting = false;
    double m_selStartTime = 0;
    double m_selEndTime = 0;
    double m_selLowFreq = 0;
    double m_selHighFreq = 0;
    

    int m_detectedMidiNote = 60;

    QDoubleSpinBox *m_spinOffset;
    double m_gridOffset = 0.0;

    QWidget *m_tabSeparation;
    QComboBox *m_comboTarget;
    QComboBox *m_comboAction;
    QSlider *m_sliderIntensity;
    QLabel *m_lblIntensity;

    QCustomPlot *m_spectrogramBefore;
    QCustomPlot *m_spectrogramAfter;
    QCPColorMap *m_mapBefore;
    QCPColorMap *m_mapAfter;

    QString m_currentSepFilePath;
    std::vector<float> m_separatedAudioData;

    QPushButton *btnPlaySep;
    QPushButton *btnStopSep;

    QMediaPlayer *m_sepPlayer;
    QAudioOutput *m_sepAudioOutput;
    QTimer *m_sepPlaybackTimer;
    QCPItemLine *m_sepPlaybackLine;

    struct Biquad {
        float a0, a1, a2, b1, b2;
        float z1 = 0, z2 = 0;
        void setLPF(float fs, float f0, float Q);
        void setHPF(float fs, float f0, float Q);
        float process(float in);
    };
};

#endif // MAINWINDOW_H
