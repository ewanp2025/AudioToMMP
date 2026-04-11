#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

extern "C" {
#include "kiss_fft.h"
}

#include "mainwindow.h"
#include <cmath>
#include <QtConcurrent>
#include <QInputDialog>
#include "smartpatterneditor.h"


void MainWindow::Biquad::setLPF(float fs, float f0, float Q) {
    float w0 = 2 * M_PI * f0 / fs;
    float alpha = sin(w0) / (2 * Q);
    float cosw0 = cos(w0);
    float a0_inv = 1.0f / (1 + alpha);

    a0 = ((1 - cosw0) / 2) * a0_inv;
    a1 = (1 - cosw0) * a0_inv;
    a2 = ((1 - cosw0) / 2) * a0_inv;
    b1 = -2 * cosw0 * a0_inv;
    b2 = (1 - alpha) * a0_inv;
}

void MainWindow::Biquad::setHPF(float fs, float f0, float Q) {
    float w0 = 2 * M_PI * f0 / fs;
    float alpha = sin(w0) / (2 * Q);
    float cosw0 = cos(w0);
    float a0_inv = 1.0f / (1 + alpha);

    a0 = ((1 + cosw0) / 2) * a0_inv;
    a1 = -(1 + cosw0) * a0_inv;
    a2 = ((1 + cosw0) / 2) * a0_inv;
    b1 = -2 * cosw0 * a0_inv;
    b2 = (1 - alpha) * a0_inv;
}

float MainWindow::Biquad::process(float in) {

    float w = in - b1 * z1 - b2 * z2;
    float out = a0 * w + a1 * z1 + a2 * z2;


    z2 = z1;
    z1 = w;

    return out;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUI();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    resize(1400, 900);
    setWindowTitle("Harmonic Grid Analyser");

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_mainTabs = new QTabWidget(this);
    mainLayout->addWidget(m_mainTabs);

    // ========================================================
    // TAB 1:
    // ========================================================
    m_tabSurgical = new QWidget();
    QVBoxLayout *surgicalLayout = new QVBoxLayout(m_tabSurgical);

    QHBoxLayout *topLayout = new QHBoxLayout();

    QPushButton *btnLoad = new QPushButton("Load Song");
    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::openFile);

    QPushButton *btnExportSurgical = new QPushButton("Export to LMMS (.xpt)");
    connect(btnExportSurgical, &QPushButton::clicked, this, &MainWindow::exportLMMSSurgical);

    m_spinOffset = new QDoubleSpinBox();
    m_spinOffset->setRange(0.0, 60.0);
    m_spinOffset->setDecimals(3);
    m_spinOffset->setSuffix(" s");
    connect(m_spinOffset, &QDoubleSpinBox::valueChanged, this, &MainWindow::onOffsetChanged);

    m_btnDetectOffset = new QPushButton("Find First Downbeat");
    connect(m_btnDetectOffset, &QPushButton::clicked, this, &MainWindow::autoDetectOffset);

    m_spinBPM = new QSpinBox();
    m_spinBPM->setRange(60, 200);
    m_spinBPM->setValue(120);
    connect(m_spinBPM, &QSpinBox::valueChanged, this, &MainWindow::onBpmChanged);

    m_btnDetectBPM = new QPushButton("Auto-Detect BPM");
    connect(m_btnDetectBPM, &QPushButton::clicked, this, &MainWindow::autoDetectBPM);

    m_comboBpmType = new QComboBox();
    m_comboBpmType->addItems({"Attack Envelope (Synths/Melodies)", "Amplitude Envelope (Drums/Beats)"});

    m_comboSteps = new QComboBox();
    m_comboSteps->addItems({"16 Steps", "32 Steps"});
    connect(m_comboSteps, &QComboBox::currentIndexChanged, this, &MainWindow::onStepsChanged);

    m_spinThreshold = new QDoubleSpinBox();
    m_spinThreshold->setRange(1.0, 100.0);
    m_spinThreshold->setValue(40.0);
    m_spinThreshold->setSuffix(" %");
    connect(m_spinThreshold, &QDoubleSpinBox::valueChanged, this, &MainWindow::onThresholdChanged);

    m_checkSnapToBar = new QCheckBox("Snap to Bar Lines");
    m_checkSnapToBar->setChecked(true);

    m_btnNudgeLeft = new QPushButton("<");
    m_btnNudgeRight = new QPushButton(">");
    connect(m_btnNudgeLeft, &QPushButton::clicked, this, &MainWindow::nudgeSelectionLeft);
    connect(m_btnNudgeRight, &QPushButton::clicked, this, &MainWindow::nudgeSelectionRight);

    m_comboAlgorithm = new QComboBox();
    m_comboAlgorithm->addItems({
        "Note: YIN (High Accuracy, Default)",
        "Note: Harmonic Product Spectrum (Best for Noisy Audio)",
        "Chord: Basic Peak Picker (Fast)",
        "Chord: Template Matching (High Accuracy)"
    });
    m_comboAlgorithm->setCurrentIndex(0);

    topLayout->addWidget(btnLoad);
    topLayout->addWidget(new QLabel("Offset:"));
    topLayout->addWidget(m_spinOffset);
    topLayout->addWidget(m_btnDetectOffset);
    topLayout->addWidget(new QLabel("BPM:"));
    topLayout->addWidget(m_spinBPM);
    topLayout->addWidget(m_btnDetectBPM);
    topLayout->addWidget(m_comboBpmType);
    topLayout->addWidget(new QLabel("Grid:"));
    topLayout->addWidget(m_comboSteps);
    topLayout->addWidget(new QLabel("Gate:"));
    topLayout->addWidget(m_spinThreshold);
    topLayout->addWidget(m_checkSnapToBar);
    topLayout->addWidget(m_btnNudgeLeft);
    topLayout->addWidget(m_btnNudgeRight);
    topLayout->addWidget(m_comboAlgorithm);
    topLayout->addStretch();
    topLayout->addWidget(btnExportSurgical);


    surgicalLayout->addLayout(topLayout);


    QSplitter *verticalSplitter = new QSplitter(Qt::Vertical);
    surgicalLayout->addWidget(verticalSplitter);

    m_spectrogramPlot = new QCustomPlot();
    m_spectrogramPlot->xAxis->setLabel("Time (s)");
    m_spectrogramPlot->yAxis->setLabel("Frequency (Hz)");
    m_spectrogramPlot->setInteraction(QCP::iRangeDrag, true);
    m_spectrogramPlot->setInteraction(QCP::iRangeZoom, true);
    m_spectrogramPlot->axisRect()->setRangeDrag(Qt::Vertical);
    m_spectrogramPlot->axisRect()->setRangeZoom(Qt::Vertical);

    m_spectrogramMap = new QCPColorMap(m_spectrogramPlot->xAxis, m_spectrogramPlot->yAxis);
    m_spectrogramMap->setGradient(QCPColorGradient::gpPolar);
    m_spectrogramMap->setInterpolate(false);

    m_selectionBox = new QCPItemRect(m_spectrogramPlot);
    m_selectionBox->setPen(QPen(Qt::yellow, 2, Qt::SolidLine));
    m_selectionBox->setBrush(QBrush(QColor(255, 255, 0, 50)));
    m_selectionBox->setVisible(false);

    connect(m_spectrogramPlot, &QCustomPlot::mousePress, this, &MainWindow::onSpectrogramMousePress);
    connect(m_spectrogramPlot, &QCustomPlot::mouseMove, this, &MainWindow::onSpectrogramMouseMove);
    connect(m_spectrogramPlot, &QCustomPlot::mouseRelease, this, &MainWindow::onSpectrogramMouseRelease);

    verticalSplitter->addWidget(m_spectrogramPlot);

    QWidget *bottomWidget = new QWidget();
    QVBoxLayout *bottomLayout = new QVBoxLayout(bottomWidget);

    QSplitter *horizontalSplitter = new QSplitter(Qt::Horizontal);
    bottomLayout->addWidget(horizontalSplitter);

    QWidget *waveWidget = new QWidget();
    QVBoxLayout *waveLayout = new QVBoxLayout(waveWidget);
    m_lblADSR = new QLabel("ADSR: Select a region on the spectrogram.");
    m_waveformPlot = new QCustomPlot();
    m_waveformPlot->addGraph();
    m_waveformPlot->graph(0)->setPen(QPen(Qt::cyan));
    m_waveformPlot->addGraph();
    m_waveformPlot->graph(1)->setPen(QPen(Qt::red, 2));
    m_waveformPlot->xAxis->setLabel("Time (s) in Selection");

    waveLayout->addWidget(m_lblADSR);

    m_lblInstrumentGuess = new QLabel("Instrument Range: Select a region to guess.");
    m_lblInstrumentGuess->setStyleSheet("color: gray; font-style: italic;");
    waveLayout->addWidget(m_lblInstrumentGuess);

    waveLayout->addWidget(m_waveformPlot);
    horizontalSplitter->addWidget(waveWidget);

    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(1.0);

    QPushButton *btnPlay = new QPushButton("Play Filtered Selection");
    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::playFilteredSelection);
    waveLayout->addWidget(btnPlay);

    QPushButton *btnSaveWav = new QPushButton("Save Filtered Selection (.wav)");
    connect(btnSaveWav, &QPushButton::clicked, this, &MainWindow::saveFilteredSelection);
    waveLayout->addWidget(btnSaveWav);

    QWidget *gridWidget = new QWidget();
    QVBoxLayout *gridLayout = new QVBoxLayout(gridWidget);
    gridLayout->addWidget(new QLabel("Detected Pattern Grid:"));

    m_stepTable = new QTableWidget(1, 16);
    m_stepTable->setVerticalHeaderLabels({"Instrument"});
    m_stepTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalSplitter->addWidget(gridWidget);
    gridLayout->addWidget(m_stepTable);

    QPushButton *btnPlaySynth = new QPushButton("▶ Play Synthesized Pattern");
    btnPlaySynth->setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold; padding: 5px;");
    connect(btnPlaySynth, &QPushButton::clicked, this, &MainWindow::playSynthesizedPattern);
    gridLayout->addWidget(btnPlaySynth);

    verticalSplitter->addWidget(bottomWidget);
    verticalSplitter->setStretchFactor(0, 3);
    verticalSplitter->setStretchFactor(1, 1);

    // ========================================================
    // TAB 2:
    // ========================================================
    m_tabMultiBand = new QWidget();
    QVBoxLayout *multiBandLayout = new QVBoxLayout(m_tabMultiBand);

    QLabel *lblMultiTitle = new QLabel("<h2>Multi-Band Auto-Transcriber</h2>");
    QLabel *lblMultiDesc = new QLabel("Define frequency bands below. The app will scan the whole song and export each band as a separate LMMS track.");

    m_multiSpectrogramPlot = new QCustomPlot();
    m_multiSpectrogramPlot->xAxis->setLabel("Time (s)");
    m_multiSpectrogramPlot->yAxis->setLabel("Frequency (Hz)");
    m_multiSpectrogramPlot->setInteraction(QCP::iRangeDrag, true);
    m_multiSpectrogramPlot->setInteraction(QCP::iRangeZoom, true);
    m_multiSpectrogramPlot->axisRect()->setRangeDrag(Qt::Vertical);
    m_multiSpectrogramPlot->axisRect()->setRangeZoom(Qt::Vertical);

    m_multiSpectrogramMap = new QCPColorMap(m_multiSpectrogramPlot->xAxis, m_multiSpectrogramPlot->yAxis);
    m_multiSpectrogramMap->setGradient(QCPColorGradient::gpPolar);
    m_multiSpectrogramMap->setInterpolate(false);

    QHBoxLayout *bandControlsLayout = new QHBoxLayout();
    QPushButton *btnAddBand = new QPushButton("+ Add Frequency Band");
    QPushButton *btnDeleteBand = new QPushButton("- Delete Selected Band");
    connect(btnAddBand, &QPushButton::clicked, this, &MainWindow::onAddBandClicked);
    connect(btnDeleteBand, &QPushButton::clicked, this, &MainWindow::onDeleteBandClicked);
    bandControlsLayout->addWidget(btnAddBand);
    bandControlsLayout->addWidget(btnDeleteBand);
    bandControlsLayout->addStretch();

    m_bandTable = new QTableWidget(0, 4);
    m_bandTable->setHorizontalHeaderLabels({"Band Name", "Low Freq (Hz)", "High Freq (Hz)", "Algorithm"});
    m_bandTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bandTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_bandTable, &QTableWidget::cellChanged, this, &MainWindow::updateBandVisuals);

    m_progressBar = new QProgressBar();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);

    QPushButton *btnExportProject = new QPushButton("Analyze Entire Song & Export LMMS Project (.mmp)");
    btnExportProject->setStyleSheet("background-color: #00557f; color: white; font-weight: bold; padding: 10px;");
    connect(btnExportProject, &QPushButton::clicked, this, &MainWindow::exportLMMSProject);

    multiBandLayout->addWidget(lblMultiTitle);
    multiBandLayout->addWidget(lblMultiDesc);
    multiBandLayout->addWidget(m_multiSpectrogramPlot, 1);
    multiBandLayout->addLayout(bandControlsLayout);
    multiBandLayout->addWidget(m_bandTable);
    multiBandLayout->addWidget(m_progressBar);
    multiBandLayout->addWidget(btnExportProject);

    onAddBandClicked(); // Lows
    m_bandTable->item(0, 0)->setText("Bass & Kick");
    m_bandTable->item(0, 1)->setText("20");
    m_bandTable->item(0, 2)->setText("250");

    onAddBandClicked(); // Mids
    m_bandTable->item(1, 0)->setText("Chords & Vocals");
    m_bandTable->item(1, 1)->setText("250");
    m_bandTable->item(1, 2)->setText("2000");
    qobject_cast<QComboBox*>(m_bandTable->cellWidget(1, 3))->setCurrentIndex(3);

    onAddBandClicked(); // Highs
    m_bandTable->item(2, 0)->setText("Hi-Hats & Air");
    m_bandTable->item(2, 1)->setText("2000");
    m_bandTable->item(2, 2)->setText("14000");

    m_mainTabs->addTab(m_tabSurgical, "1. Surgical Pattern Mode");
    m_mainTabs->addTab(m_tabMultiBand, "2. Multi-Band Project Mode (WIP)");

    // ========================================================
    // TAB 3: SOURCE SEPARATION
    // ========================================================
    m_tabSeparation = new QWidget();
    QVBoxLayout *sepLayout = new QVBoxLayout(m_tabSeparation);


    QHBoxLayout *sepControlsLayout = new QHBoxLayout();

    m_comboTarget = new QComboBox();
    m_comboTarget->addItems({"Bassline", "Vocals", "Drums", "Other (Add later)"});

    m_comboAction = new QComboBox();
    m_comboAction->addItems({"Isolate", "Remove"});

    m_sliderIntensity = new QSlider(Qt::Horizontal);
    m_sliderIntensity->setRange(0, 100);
    m_sliderIntensity->setValue(100);
    m_lblIntensity = new QLabel("Intensity: 100%");

    connect(m_sliderIntensity, &QSlider::valueChanged, [this](int value){
        m_lblIntensity->setText(QString("Intensity: %1%").arg(value));
    });

    sepControlsLayout->addWidget(new QLabel("Target:"));
    sepControlsLayout->addWidget(m_comboTarget);
    sepControlsLayout->addWidget(new QLabel("Action:"));
    sepControlsLayout->addWidget(m_comboAction);
    sepControlsLayout->addWidget(m_lblIntensity);
    sepControlsLayout->addWidget(m_sliderIntensity);


    QHBoxLayout *sepActionLayout = new QHBoxLayout();
    QPushButton *btnLoadSepFile = new QPushButton("Load Audio");
    QPushButton *btnSaveSepWav = new QPushButton("Save Processed");
    QPushButton *btnProcessSep = new QPushButton("Process Separation");

    btnPlaySep = new QPushButton("▶ Play");
    btnStopSep = new QPushButton("⏹ Stop");
    btnPlaySep->setEnabled(false); // Disabled until processed
    btnStopSep->setEnabled(false);

    connect(btnLoadSepFile, &QPushButton::clicked, this, &MainWindow::onLoadSepFileClicked);
    connect(btnSaveSepWav, &QPushButton::clicked, this, &MainWindow::onSaveSepWavClicked);
    connect(btnProcessSep, &QPushButton::clicked, this, &MainWindow::onProcessSeparationClicked);
    connect(btnPlaySep, &QPushButton::clicked, this, &MainWindow::onPlaySepClicked);
    connect(btnStopSep, &QPushButton::clicked, this, &MainWindow::onStopSepClicked);

    sepActionLayout->addWidget(btnLoadSepFile);
    sepActionLayout->addWidget(btnProcessSep);
    sepActionLayout->addWidget(btnSaveSepWav);
    sepActionLayout->addWidget(btnPlaySep);
    sepActionLayout->addWidget(btnStopSep);


    m_sepPlayer = new QMediaPlayer(this);
    m_sepAudioOutput = new QAudioOutput(this);
    m_sepPlayer->setAudioOutput(m_sepAudioOutput);

    m_sepPlaybackTimer = new QTimer(this);
    connect(m_sepPlaybackTimer, &QTimer::timeout, this, &MainWindow::updateSepPlaybackLine);


    QHBoxLayout *sepSpectroLayout = new QHBoxLayout();


    QVBoxLayout *beforeLayout = new QVBoxLayout();
    m_spectrogramBefore = new QCustomPlot();
    m_mapBefore = new QCPColorMap(m_spectrogramBefore->xAxis, m_spectrogramBefore->yAxis);
    beforeLayout->addWidget(new QLabel("Original Audio:"));
    beforeLayout->addWidget(m_spectrogramBefore);


    QVBoxLayout *afterLayout = new QVBoxLayout();
    m_spectrogramAfter = new QCustomPlot();
    m_mapAfter = new QCPColorMap(m_spectrogramAfter->xAxis, m_spectrogramAfter->yAxis);
    afterLayout->addWidget(new QLabel("Processed Audio:"));
    afterLayout->addWidget(m_spectrogramAfter);


    m_sepPlaybackLine = new QCPItemLine(m_spectrogramAfter);
    m_sepPlaybackLine->setPen(QPen(Qt::red, 2));
    m_sepPlaybackLine->start->setCoords(0, 0);
    m_sepPlaybackLine->end->setCoords(0, 22050); // Nyquist frequency max Y
    m_sepPlaybackLine->setVisible(false);


    sepSpectroLayout->addLayout(beforeLayout);
    sepSpectroLayout->addLayout(afterLayout);


    sepLayout->addLayout(sepControlsLayout);
    sepLayout->addLayout(sepActionLayout);
    sepLayout->addLayout(sepSpectroLayout);


    m_mainTabs->addTab(m_tabSeparation, "Source Separation (WIP)");


    // ========================================================
    // TAB 4: AUTOMATION EDITOR
    // ========================================================
    m_tabAutomation = new QWidget();
    QVBoxLayout *autoLayout = new QVBoxLayout(m_tabAutomation);

    QHBoxLayout *loadLayout = new QHBoxLayout();
    m_btnLoadMmp = new QPushButton("1. Load .mmp Project");
    m_btnLoadMmp->setStyleSheet("font-weight: bold; padding: 5px;");
    m_lblLoadedMmp = new QLabel("No project loaded.");

    m_lblProjectStats = new QLabel("<b>Project Stats:</b> N/A");
    m_lblProjectStats->setStyleSheet("color: #555;");

    loadLayout->addWidget(m_btnLoadMmp);
    loadLayout->addWidget(m_lblLoadedMmp);
    loadLayout->addStretch();
    loadLayout->addWidget(m_lblProjectStats);
    autoLayout->addLayout(loadLayout);

    QSplitter *mainSplitter = new QSplitter(Qt::Vertical);
    autoLayout->addWidget(mainSplitter);

    QWidget *editorTopWidget = new QWidget();
    QHBoxLayout *editorTopLayout = new QHBoxLayout(editorTopWidget);


    QWidget *editorControls = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(editorControls);
    controlLayout->addWidget(new QLabel("<h2>Editor</h2>"));

    QGridLayout *lfoLayout = new QGridLayout();
    m_comboTracks = new QComboBox();
    m_comboTargetParam = new QComboBox();

    m_spinLfoLengthTicks = new QSpinBox();
    m_spinLfoLengthTicks->setRange(48, 192000);
    m_spinLfoLengthTicks->setValue(768);
    m_lblEditorDurationBars = new QLabel("Duration: 4.00 Bars");

    m_comboInterpolation = new QComboBox();
    m_comboInterpolation->addItems({"0 - Discrete (Step)", "1 - Linear", "2 - Cubic Hermite (Smooth)"});
    m_comboInterpolation->setCurrentIndex(2);

    m_checkSnapGrid = new QCheckBox("Snap to Grid");
    m_checkSnapGrid->setChecked(true);

    m_comboQuantizeX = new QComboBox();
    m_comboQuantizeX->addItems({"1/4 (48 Ticks)", "1/8 (24 Ticks)", "1/16 (12 Ticks)", "1/32 (6 Ticks)"});
    m_comboQuantizeX->setCurrentIndex(2); // Default to 1/16ths

    lfoLayout->addWidget(m_checkSnapGrid, 7, 0);
    lfoLayout->addWidget(m_comboQuantizeX, 7, 1);


    lfoLayout->addWidget(new QLabel("<b>1. Target Linking:</b>"), 0, 0, 1, 2);
    lfoLayout->addWidget(new QLabel("Track:"), 1, 0);
    lfoLayout->addWidget(m_comboTracks, 1, 1);
    lfoLayout->addWidget(new QLabel("Parameter:"), 2, 0);
    lfoLayout->addWidget(m_comboTargetParam, 2, 1);

    lfoLayout->addWidget(new QLabel("<b>2. Master Settings:</b>"), 3, 0, 1, 2);
    lfoLayout->addWidget(new QLabel("Length (Ticks):"), 4, 0);
    lfoLayout->addWidget(m_spinLfoLengthTicks, 4, 1);
    lfoLayout->addWidget(m_lblEditorDurationBars, 5, 1);
    lfoLayout->addWidget(new QLabel("Interpolation:"), 6, 0);
    lfoLayout->addWidget(m_comboInterpolation, 6, 1);

    controlLayout->addLayout(lfoLayout);
    controlLayout->addWidget(new QLabel("<hr><b>3. Generation Tools:</b>"));


    QGridLayout *genLayout = new QGridLayout();

    m_comboMacroType = new QComboBox();
    m_comboMacroType->addItems({
        "LFO Oscillator",
        "ADSR Envelope",
        "Random (Sample & Hold)",
        "Rhythmic Gate (16ths)",
        "Sidechain Pump (Ducking)",
        "Tape Stop / Drop"
    });

    m_comboWaveform = new QComboBox();
    m_comboWaveform->addItems({"Sine", "Square", "Triangle", "Sawtooth Down", "Sawtooth Up"});


    m_spinLfoFreqStart = new QDoubleSpinBox(); m_spinLfoFreqStart->setDecimals(1); m_spinLfoFreqStart->setRange(1.0, 19200.0); m_spinLfoFreqStart->setValue(192.0);
    m_spinLfoFreqEnd = new QDoubleSpinBox(); m_spinLfoFreqEnd->setDecimals(1); m_spinLfoFreqEnd->setRange(1.0, 19200.0); m_spinLfoFreqEnd->setValue(192.0);

    m_spinLfoFreqStart->setSingleStep(24.0);
    m_spinLfoFreqEnd->setSingleStep(24.0);

    m_spinLfoPhase = new QDoubleSpinBox();
    m_spinLfoPhase->setRange(0.0, 360.0);
    m_spinLfoPhase->setValue(0.0);
    m_spinLfoPhase->setSuffix(" °");

    m_spinLfoDepthStart = new QDoubleSpinBox(); m_spinLfoDepthStart->setRange(0.0, 20000.0); m_spinLfoDepthStart->setValue(100.0);
    m_spinLfoDepthEnd = new QDoubleSpinBox(); m_spinLfoDepthEnd->setRange(0.0, 20000.0); m_spinLfoDepthEnd->setValue(100.0);


    m_spinLfoBaseValue = new QDoubleSpinBox();
    m_spinLfoBaseValue->setRange(-20000.0, 20000.0);
    m_spinLfoBaseValue->setValue(100.0);
    m_spinSwing = new QDoubleSpinBox();
    m_spinSwing->setRange(0.0, 100.0);
    m_spinSwing->setValue(0.0);
    m_spinSwing->setSuffix(" %");


    m_spinTension = new QDoubleSpinBox();
    m_spinTension->setRange(0.1, 10.0);
    m_spinTension->setValue(1.0);
    m_spinTension->setSingleStep(0.1);

    m_spinDataPoints = new QSpinBox();
    m_spinDataPoints->setRange(2, 2000);
    m_spinDataPoints->setValue(64);

    genLayout->addWidget(new QLabel("Macro Engine:"), 0, 0); genLayout->addWidget(m_comboMacroType, 0, 1);
    genLayout->addWidget(new QLabel("LFO Waveform:"), 1, 0); genLayout->addWidget(m_comboWaveform, 1, 1);


    QHBoxLayout *rateLayout = new QHBoxLayout();
    rateLayout->addWidget(new QLabel("Start:")); rateLayout->addWidget(m_spinLfoFreqStart);
    rateLayout->addWidget(new QLabel("End:")); rateLayout->addWidget(m_spinLfoFreqEnd);
    genLayout->addWidget(new QLabel("Rate (Ticks/Cycle):"), 2, 0); genLayout->addLayout(rateLayout, 2, 1);

    genLayout->addWidget(new QLabel("Phase (Horiz Offset):"), 3, 0); genLayout->addWidget(m_spinLfoPhase, 3, 1);

    QHBoxLayout *depthLayout = new QHBoxLayout();
    depthLayout->addWidget(new QLabel("Start:")); depthLayout->addWidget(m_spinLfoDepthStart);
    depthLayout->addWidget(new QLabel("End:")); depthLayout->addWidget(m_spinLfoDepthEnd);
    genLayout->addWidget(new QLabel("Depth (Amplitude):"), 4, 0); genLayout->addLayout(depthLayout, 4, 1);

    genLayout->addWidget(new QLabel("Base (Vert Center):"), 5, 0); genLayout->addWidget(m_spinLfoBaseValue, 5, 1);
    genLayout->addWidget(new QLabel("Swing / Groove:"), 6, 0);     genLayout->addWidget(m_spinSwing, 6, 1);
    genLayout->addWidget(new QLabel("Tension (Exp/Log):"), 7, 0);  genLayout->addWidget(m_spinTension, 7, 1); // <--- NEW ROW 7
    genLayout->addWidget(new QLabel("Data Points:"), 8, 0);        genLayout->addWidget(m_spinDataPoints, 8, 1); // <--- MOVED TO ROW 8

    controlLayout->addLayout(genLayout);

    m_btnGenerateLfo = new QPushButton("Generate LFO Shape");

    m_comboBlendMode = new QComboBox();
    m_comboBlendMode->addItems({"Replace", "Add (+)", "Subtract (-)", "Multiply (x)"});

    QHBoxLayout *generateLayout = new QHBoxLayout();

    generateLayout->addWidget(m_btnGenerateLfo);
    generateLayout->addWidget(new QLabel("Blend:"));
    generateLayout->addWidget(m_comboBlendMode);

    controlLayout->addLayout(generateLayout);

    m_btnExtractEnvelope = new QPushButton("Extract Audio Envelope (From Tab 1)");
    m_btnExtractEnvelope->setStyleSheet("background-color: #D2691E; color: white; font-weight: bold;");

    m_btnReverseEditor = new QPushButton("Reverse Array (Flip Time)");
    m_btnClearEditor = new QPushButton("Clear ");

    controlLayout->addWidget(m_btnGenerateLfo);
    controlLayout->addWidget(m_btnExtractEnvelope);
    controlLayout->addWidget(m_btnReverseEditor);
    controlLayout->addWidget(m_btnClearEditor);

    QPushButton *btnSmooth = new QPushButton("Smooth Curve");
    QPushButton *btnQuantizeY = new QPushButton("Quantize Y (Steps)");
    QPushButton *btnHumanize = new QPushButton("Humanize (Jitter)");
    QPushButton *btnInvert = new QPushButton("Invert Vertically");

    controlLayout->addWidget(btnSmooth);
    controlLayout->addWidget(btnQuantizeY);
    controlLayout->addWidget(btnHumanize);
    controlLayout->addWidget(btnInvert);


    QHBoxLayout *shapeFileLayout = new QHBoxLayout();
    QPushButton *btnSaveShape = new QPushButton("Save Shape (.xpa)");
    QPushButton *btnLoadShape = new QPushButton("Load Shape (.xpa)");

    QPushButton *btnLoadXptCv = new QPushButton("Load Pattern as CV (.xpt)");
    btnLoadXptCv->setStyleSheet("background-color: #2E8B57; color: white;");

    QPushButton *btnScaleAmplitude = new QPushButton("Scale Y Amplitude");


    shapeFileLayout->addWidget(btnLoadShape);
    shapeFileLayout->addWidget(btnSaveShape);
    shapeFileLayout->addWidget(btnLoadXptCv);


    controlLayout->addLayout(shapeFileLayout);
    controlLayout->addWidget(btnScaleAmplitude);


    connect(btnSmooth, &QPushButton::clicked, this, &MainWindow::onSmoothClicked);
    connect(btnQuantizeY, &QPushButton::clicked, this, &MainWindow::onQuantizeYClicked);
    connect(btnHumanize, &QPushButton::clicked, this, &MainWindow::onHumanizeClicked);
    connect(btnInvert, &QPushButton::clicked, this, &MainWindow::onInvertClicked);

    connect(btnSaveShape, &QPushButton::clicked, this, &MainWindow::onSaveShapeClicked);
    connect(btnLoadShape, &QPushButton::clicked, this, &MainWindow::onLoadShapeClicked);

    connect(btnLoadXptCv, &QPushButton::clicked, this, &MainWindow::onLoadXptAsCvClicked);
    connect(btnScaleAmplitude, &QPushButton::clicked, this, &MainWindow::onScaleYAmplitudeClicked);

    controlLayout->addStretch();

    m_btnInjectMmp = new QPushButton("INJECT & SAVE AS .MMP");
    m_btnInjectMmp->setStyleSheet("background-color: #8B008B; color: white; font-weight: bold; padding: 15px; font-size: 14px;");
    m_btnInjectMmp->setEnabled(false);
    controlLayout->addWidget(m_btnInjectMmp);


    QScrollArea *controlScrollArea = new QScrollArea();
    controlScrollArea->setWidget(editorControls);
    controlScrollArea->setWidgetResizable(true);
    controlScrollArea->setFrameShape(QFrame::NoFrame);
    controlScrollArea->setMinimumWidth(350);

    editorTopLayout->addWidget(controlScrollArea, 1);

    m_plotEditor = new QCustomPlot();
    m_plotEditor->xAxis->setLabel("Time (Ticks)");
    m_plotEditor->yAxis->setLabel("Parameter Value");
    m_plotEditor->setInteraction(QCP::iRangeDrag, true);
    m_plotEditor->setInteraction(QCP::iRangeZoom, true);
    editorTopLayout->addWidget(m_plotEditor, 3);

    connect(m_plotEditor, &QCustomPlot::mousePress, this, &MainWindow::onEditorMousePress);
    connect(m_plotEditor, &QCustomPlot::mouseMove, this, &MainWindow::onEditorMouseMove);
    connect(m_plotEditor, &QCustomPlot::mouseRelease, this, &MainWindow::onEditorMouseRelease);
    connect(m_plotEditor, &QCustomPlot::mouseDoubleClick, this, &MainWindow::onEditorMouseDoubleClick);


    QWidget *viewerBottomWidget = new QWidget();
    QVBoxLayout *viewerBottomLayout = new QVBoxLayout(viewerBottomWidget);
    viewerBottomLayout->addWidget(new QLabel("<h3>Detected Automations in File</h3>"));

    QHBoxLayout *listInfoLayout = new QHBoxLayout();
    m_listAutomations = new QListWidget();
    listInfoLayout->addWidget(m_listAutomations, 1);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    m_txtAutomationInfo = new QTextEdit();
    m_txtAutomationInfo->setReadOnly(true);
    m_txtAutomationInfo->setStyleSheet("background-color: #f0f0f0; color: #333;");

    m_btnCopyToEditor = new QPushButton("↑ COPY TO EDITOR ↑");
    m_btnCopyToEditor->setStyleSheet("background-color: #00557f; color: white; font-weight: bold; padding: 10px;");
    m_btnCopyToEditor->setEnabled(false);

    infoLayout->addWidget(m_txtAutomationInfo);
    infoLayout->addWidget(m_btnCopyToEditor);
    listInfoLayout->addLayout(infoLayout, 1);

    viewerBottomLayout->addLayout(listInfoLayout, 1);

    m_plotAutomation = new QCustomPlot();
    m_plotAutomation->xAxis->setLabel("Time (Ticks)");
    viewerBottomLayout->addWidget(m_plotAutomation, 2);

    mainSplitter->addWidget(editorTopWidget);
    mainSplitter->addWidget(viewerBottomWidget);


    connect(m_btnLoadMmp, &QPushButton::clicked, this, &MainWindow::onLoadMmpClicked);
    connect(m_listAutomations, &QListWidget::currentRowChanged, this, &MainWindow::onExistingAutomationSelected);
    connect(m_btnCopyToEditor, &QPushButton::clicked, this, &MainWindow::onCopyToEditorClicked);

    connect(m_btnGenerateLfo, &QPushButton::clicked, this, &MainWindow::onGenerateLfoClicked);
    connect(m_btnReverseEditor, &QPushButton::clicked, this, &MainWindow::onReverseEditorClicked);
    connect(m_btnClearEditor, &QPushButton::clicked, this, &MainWindow::onClearEditorClicked);
    connect(m_btnInjectMmp, &QPushButton::clicked, this, &MainWindow::onInjectMmpClicked);

    connect(m_comboTracks, &QComboBox::currentIndexChanged, this, &MainWindow::onTrackSelectionChanged);

    connect(m_comboTargetParam, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.startsWith("vol")) {
            m_spinLfoBaseValue->setValue(100.0);
            m_spinLfoDepthStart->setValue(100.0); m_spinLfoDepthEnd->setValue(100.0);
        } else if (text.startsWith("pan")) {
            m_spinLfoBaseValue->setValue(0.0);
            m_spinLfoDepthStart->setValue(100.0); m_spinLfoDepthEnd->setValue(100.0);
        } else if (text.startsWith("fcut")) {
            m_spinLfoBaseValue->setValue(7000.0);
            m_spinLfoDepthStart->setValue(7000.0); m_spinLfoDepthEnd->setValue(7000.0);
        }
    });

    connect(m_comboMacroType, &QComboBox::currentIndexChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoDepthStart, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoDepthEnd, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoFreqStart, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoFreqEnd, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoBaseValue, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_spinLfoPhase, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });
    connect(m_btnExtractEnvelope, &QPushButton::clicked, this, &MainWindow::onExtractEnvelopeClicked);
    connect(m_spinSwing, &QDoubleSpinBox::valueChanged, this, [this](){ onGenerateLfoClicked(); });

    m_mainTabs->addTab(m_tabAutomation, "4. Automation Macros");

    // ========================================================
    // TAB 5: GROOVE EXTRACTOR (.mod / .mid / .mmp to BB-Editor)
    // ========================================================
    m_tabGroove = new QWidget();
    QVBoxLayout *grooveLayout = new QVBoxLayout(m_tabGroove);

    QHBoxLayout *grooveTopLayout = new QHBoxLayout();

    QPushButton *btnLoadModGroove = new QPushButton("1. Load Amiga .mod");
    btnLoadModGroove->setStyleSheet("font-weight: bold; padding: 5px;");

    QPushButton *btnLoadMidiGroove = new QPushButton("Load .mid TBC");
    QPushButton *btnLoadMmpGroove = new QPushButton("1. Load .mmp (BB Track)");

    m_spinStartPattern = new QSpinBox();
    m_spinStartPattern->setRange(0, 128);
    m_spinStartPattern->setPrefix("Start Pattern: ");

    m_comboModChannel = new QComboBox();
    m_comboModChannel->addItems({"All Channels", "Channel 1", "Channel 2", "Channel 3", "Channel 4", "Channel 5", "Channel 6", "Channel 7", "Channel 8"});

    m_comboGridSize = new QComboBox();
    m_comboGridSize->addItems({"16 Steps", "32 Steps", "64 Steps (Full MOD Pattern)"});
    m_comboGridSize->setCurrentIndex(2);

    grooveTopLayout->addWidget(btnLoadModGroove);
    grooveTopLayout->addWidget(btnLoadMidiGroove);
    grooveTopLayout->addWidget(btnLoadMmpGroove);
    grooveTopLayout->addWidget(m_spinStartPattern);
    grooveTopLayout->addWidget(m_comboModChannel);
    grooveTopLayout->addWidget(m_comboGridSize);
    grooveTopLayout->addStretch();

    grooveLayout->addLayout(grooveTopLayout);

    grooveLayout->addWidget(new QLabel("<h3>Tracker Extractor Grid</h3>"));

    m_bbTable = new QTableWidget(0, 64);
    m_bbTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bbTable->setSelectionMode(QAbstractItemView::MultiSelection);
    grooveLayout->addWidget(m_bbTable, 1);

    QHBoxLayout *grooveBottomLayout = new QHBoxLayout();
    QPushButton *btnExportNewMmp = new QPushButton("Save as NEW .mmp Project");
    btnExportNewMmp->setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold; padding: 10px;");
    btnExportNewMmp->setEnabled(false);

    grooveBottomLayout->addStretch();
    grooveBottomLayout->addWidget(btnExportNewMmp);
    grooveLayout->addLayout(grooveBottomLayout);

    m_mainTabs->addTab(m_tabGroove, "5. Amiga Module Drum Extractor");

    connect(btnLoadModGroove, &QPushButton::clicked, this, &MainWindow::onLoadModGrooveClicked);
    connect(btnLoadMidiGroove, &QPushButton::clicked, this, &MainWindow::onLoadMidiGrooveClicked);
    connect(btnLoadMmpGroove, &QPushButton::clicked, this, &MainWindow::onLoadMmpGrooveClicked);
    connect(btnExportNewMmp, &QPushButton::clicked, this, &MainWindow::onExportNewMmpClicked);
    connect(m_spinStartPattern, &QSpinBox::valueChanged, this, &MainWindow::processModData);
    connect(m_comboModChannel, &QComboBox::currentIndexChanged, this, &MainWindow::processModData);

    // ========================================================
    // Tab 6: 303 Test Tab Setup
    // ========================================================
    m_tab303Test = new QWidget();
    QVBoxLayout *layout303 = new QVBoxLayout(m_tab303Test);

    QHBoxLayout *controlsLayout1 = new QHBoxLayout();

    m_combo303NotePattern = new QComboBox();
        m_combo303NotePattern->addItems({
            "Classic 16-Step", "Syncopated 5-Step", "Rolling 12-Step",
            "A", "B", "The Octave Squelch",
            "C", "D",
            "E", "F", "G"
        });
        controlsLayout1->addWidget(new QLabel("Note Pattern:"));
        controlsLayout1->addWidget(m_combo303NotePattern);

    connect(m_combo303NotePattern, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on303PatternChanged);


    ///controlsLayout1->addWidget(m_combo303Macro);

    m_spin303Bpm = new QSpinBox();
    m_spin303Bpm->setRange(60, 200);
    m_spin303Bpm->setValue(125);
    controlsLayout1->addWidget(new QLabel("BPM:"));
    controlsLayout1->addWidget(m_spin303Bpm);

    m_spin303TotalBars = new QSpinBox();
    m_spin303TotalBars->setRange(1, 128);
    m_spin303TotalBars->setValue(16);
    controlsLayout1->addWidget(new QLabel("Total Bars:"));
    controlsLayout1->addWidget(m_spin303TotalBars);

    m_checkParseSlides = new QCheckBox("Detuning Slides", this);
    m_checkParseSlides->setChecked(true);
    controlsLayout1->addWidget(m_checkParseSlides);

    layout303->addLayout(controlsLayout1);

    QHBoxLayout *controlsLayout2 = new QHBoxLayout();

    m_spin303Cutoff = new QDoubleSpinBox();
    m_spin303Cutoff->setRange(0.0, 1.0);
    m_spin303Cutoff->setValue(0.2);
    m_spin303Cutoff->setSingleStep(0.05);
    controlsLayout2->addWidget(new QLabel("Base Cutoff:"));
    controlsLayout2->addWidget(m_spin303Cutoff);

    m_spin303Resonance = new QDoubleSpinBox();
    m_spin303Resonance->setRange(0.0, 1.0);
    m_spin303Resonance->setValue(0.8);
    m_spin303Resonance->setSingleStep(0.05);
    controlsLayout2->addWidget(new QLabel("Resonance:"));
    controlsLayout2->addWidget(m_spin303Resonance);

    m_spin303EnvMod = new QDoubleSpinBox();
    m_spin303EnvMod->setRange(0.0, 1.0);
    m_spin303EnvMod->setValue(0.6);
    m_spin303EnvMod->setSingleStep(0.05);
    controlsLayout2->addWidget(new QLabel("Env Mod:"));
    controlsLayout2->addWidget(m_spin303EnvMod);

    layout303->addLayout(controlsLayout2);



            layout303->addWidget(new QLabel("<h3>Main Sequencer (16 Steps)</h3>"));


                m_seqTable = new QTableWidget(5, 16, this);
                m_seqTable->setVerticalHeaderLabels({"State", "Note", "Octave", "Slide", "Accent"});
                m_seqTable->setFixedHeight(175);

                for (int col = 0; col < 16; ++col) {
                    QComboBox *stateCombo = new QComboBox();
                    stateCombo->addItems({"Play", "Tie", "Rest"});
                    m_seqTable->setCellWidget(0, col, stateCombo);

                    QComboBox *noteCombo = new QComboBox();
                    noteCombo->addItems({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
                    m_seqTable->setCellWidget(1, col, noteCombo);

                    QSpinBox *octaveSpin = new QSpinBox();
                    octaveSpin->setRange(1, 6); octaveSpin->setValue(3);
                    m_seqTable->setCellWidget(2, col, octaveSpin);

                    QCheckBox *slideCheck = new QCheckBox("Slide");
                    m_seqTable->setCellWidget(3, col, slideCheck);

                    QCheckBox *accentCheck = new QCheckBox("Acc");
                    m_seqTable->setCellWidget(4, col, accentCheck);
                }
                layout303->addWidget(m_seqTable);


                QHBoxLayout *filterHeaderLayout = new QHBoxLayout();
                filterHeaderLayout->addWidget(new QLabel("<h3>Filter Automation (Polymeter)</h3>"));

                m_comboFilterPattern = new QComboBox();
                    m_comboFilterPattern->addItems({
                        "Manual",
                        "3-Step Squelch",
                        "5-Step Tension",
                        "7-Step Demented",
                        "16-Step Classic Sine",
                        "24-Step Bubbler",
                        "12-Step Rolling Reso",
                        "32-Step Slow Filter Sweep",
                        "9-Step Syncopated Chirp",
                        "64-Step Evolving"
                    });
                filterHeaderLayout->addWidget(new QLabel("Preset:"));
                filterHeaderLayout->addWidget(m_comboFilterPattern);

                m_spinFilterLength = new QSpinBox();
                m_spinFilterLength->setRange(1, 128);
                m_spinFilterLength->setValue(32);
                filterHeaderLayout->addWidget(new QLabel("Length (Steps):"));
                filterHeaderLayout->addWidget(m_spinFilterLength);
                filterHeaderLayout->addStretch();
                layout303->addLayout(filterHeaderLayout);

                connect(m_comboFilterPattern, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFilterPatternChanged);
                connect(m_spinFilterLength, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onFilterLengthChanged);


                m_freqTable = new QTableWidget(1, 32, this);
                m_freqTable->setVerticalHeaderLabels({"Cutoff"});
                m_freqTable->setFixedHeight(80);
                layout303->addWidget(m_freqTable);


                m_resTable = new QTableWidget(1, 32, this);
                m_resTable->setVerticalHeaderLabels({"Resonance"});
                m_resTable->setFixedHeight(80);
                layout303->addWidget(m_resTable);


                onFilterLengthChanged(32);

                m_btnGenerate303 = new QPushButton("Generate Self-Contained 303 Project");
                layout303->addWidget(m_btnGenerate303);
                layout303->addStretch();
                connect(m_btnGenerate303, &QPushButton::clicked, this, &MainWindow::generate303Project);




    m_mainTabs->addTab(m_tab303Test, "303 Test");

    // ========================================================
    // TAB 7: PATTERN CLASH CHECKER
    // ========================================================
    m_tabAnalyzer = new QWidget();
    QVBoxLayout *analyzerLayout = new QVBoxLayout(m_tabAnalyzer);

    QHBoxLayout *analyzerTopLayout = new QHBoxLayout();
    m_btnAddPattern = new QPushButton("+ Add .xpt Pattern to Checker");
    m_btnAddPattern->setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold; padding: 10px;");
    m_btnClearPatterns = new QPushButton("Clear All");

    analyzerTopLayout->addWidget(m_btnAddPattern);
    analyzerTopLayout->addWidget(m_btnClearPatterns);
    analyzerTopLayout->addStretch();
    analyzerLayout->addLayout(analyzerTopLayout);

    QSplitter *analyzerSplitter = new QSplitter(Qt::Horizontal);


    QWidget *leftAnalyzerWidget = new QWidget();
    QVBoxLayout *leftAnalyzerLayout = new QVBoxLayout(leftAnalyzerWidget);
    leftAnalyzerLayout->addWidget(new QLabel("<b>Loaded Patterns:</b>"));
    m_listAnalyzerPatterns = new QListWidget();
    leftAnalyzerLayout->addWidget(m_listAnalyzerPatterns);
    analyzerSplitter->addWidget(leftAnalyzerWidget);


    QWidget *rightAnalyzerWidget = new QWidget();
    QVBoxLayout *rightAnalyzerLayout = new QVBoxLayout(rightAnalyzerWidget);
    rightAnalyzerLayout->addWidget(new QLabel("<b>Harmonic & Timing Analysis:</b>"));
    m_txtAnalyzerFeedback = new QTextEdit();
    m_txtAnalyzerFeedback->setReadOnly(true);
    m_txtAnalyzerFeedback->setStyleSheet("background-color: #f8f9fa; color: #333; font-size: 14px;");
    rightAnalyzerLayout->addWidget(m_txtAnalyzerFeedback);
    analyzerSplitter->addWidget(rightAnalyzerWidget);

    analyzerSplitter->setStretchFactor(0, 1);
    analyzerSplitter->setStretchFactor(1, 2);
    analyzerLayout->addWidget(analyzerSplitter);

    m_mainTabs->addTab(m_tabAnalyzer, "7. Pattern Clash Checker");


    connect(m_btnAddPattern, &QPushButton::clicked, this, &MainWindow::onAnalyzerAddPattern);
    connect(m_btnClearPatterns, &QPushButton::clicked, this, &MainWindow::onAnalyzerClearPatterns);

    // ========================================================
    // TAB 8: SMART PATTERN EDITOR
    // ========================================================
    SmartPatternEditor *tabSmartEditor = new SmartPatternEditor(this);
    m_mainTabs->addTab(tabSmartEditor, "8. Smart Pattern Editor");
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Audio", "", "Audio Files (*.wav *.mp3 *.flac)");
    if (fileName.isEmpty()) return;

    if (loadAudio(fileName)) {
        generateSpectrogram();
        drawGridLines();
    }
}

bool MainWindow::loadAudio(const QString &fileName)
{
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, m_sampleRate);

    if (ma_decoder_init_file(fileName.toStdString().c_str(), &config, &decoder) != MA_SUCCESS) {
        return false;
    }

    ma_uint64 frameCount;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    m_audioData.resize(frameCount);
    ma_decoder_read_pcm_frames(&decoder, m_audioData.data(), frameCount, NULL);
    ma_decoder_uninit(&decoder);

    return true;
}

void MainWindow::generateSpectrogram()
{
    if (m_audioData.empty()) return;

    int nSamples = m_audioData.size();
    const int fftSize = 2048;
    const int overlap = 1024;
    int timeSteps = (nSamples - fftSize) / (fftSize - overlap);
    int freqBins = fftSize / 2;

    m_spectrogramMap->data()->setSize(timeSteps, freqBins);
    m_spectrogramMap->data()->setRange(QCPRange(0, (double)nSamples/m_sampleRate), QCPRange(0, m_sampleRate/2));

    m_multiSpectrogramMap->data()->setSize(timeSteps, freqBins);
    m_multiSpectrogramMap->data()->setRange(QCPRange(0, (double)nSamples/m_sampleRate), QCPRange(0, m_sampleRate/2));

    m_mapBefore->data()->setSize(timeSteps, freqBins);
    m_mapBefore->data()->setRange(QCPRange(0, (double)nSamples/m_sampleRate), QCPRange(0, m_sampleRate/2));

    kiss_fft_cfg cfg = kiss_fft_alloc(fftSize, 0, NULL, NULL);
    kiss_fft_cpx in[fftSize], out[fftSize];

    for (int t = 0; t < timeSteps; ++t) {
        int startSample = t * (fftSize - overlap);
        for (int i = 0; i < fftSize; ++i) {
            float win = 0.5 * (1 - cos(2 * M_PI * i / (fftSize - 1)));
            in[i].r = m_audioData[startSample + i] * win;
            in[i].i = 0;
        }
        kiss_fft(cfg, in, out);
        for (int f = 0; f < freqBins; ++f) {
            double mag = sqrt(out[f].r * out[f].r + out[f].i * out[f].i);
            double decibels = 20 * log10(mag + 1e-6);
            m_spectrogramMap->data()->setCell(t, f, decibels);
            m_multiSpectrogramMap->data()->setCell(t, f, decibels);
            m_mapBefore->data()->setCell(t, f, decibels);
        }

        if (t % 50 == 0) {
            QCoreApplication::processEvents();
        }
    }
    free(cfg);

    m_spectrogramMap->rescaleDataRange(true);
    m_spectrogramPlot->xAxis->setRange(0, (double)nSamples/m_sampleRate);
    m_spectrogramPlot->yAxis->setRange(0, 5000);
    m_spectrogramPlot->replot();

    m_multiSpectrogramMap->rescaleDataRange(true);
    m_multiSpectrogramPlot->xAxis->setRange(0, (double)nSamples/m_sampleRate);
    m_multiSpectrogramPlot->yAxis->setRange(0, 5000);
    updateBandVisuals();

    m_mapBefore->rescaleDataRange(true);
    m_spectrogramBefore->xAxis->setRange(0, (double)nSamples/m_sampleRate);
    m_spectrogramBefore->yAxis->setRange(0, 5000);
    m_spectrogramBefore->replot();

    m_mainTabs->addTab(m_tab303Test, "303 Test");

}

void MainWindow::drawGridLines()
{
}

void MainWindow::onSpectrogramMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        m_isSelecting = true;

        double rawTime = m_spectrogramPlot->xAxis->pixelToCoord(event->pos().x());
        double freq = m_spectrogramPlot->yAxis->pixelToCoord(event->pos().y());

        m_selStartTime = snapTimeToGrid(rawTime);
        m_selLowFreq = freq;
        m_selHighFreq = freq;

        if (m_checkSnapToBar->isChecked()) {
            double barDuration = (60.0 / m_bpm) * 4.0;
            m_selEndTime = m_selStartTime + barDuration;
        } else {
            m_selEndTime = m_selStartTime;
        }

        updateSelectionVisuals();
    }
}

void MainWindow::onSpectrogramMouseMove(QMouseEvent *event)
{
    if (m_isSelecting) {
        double rawTime = m_spectrogramPlot->xAxis->pixelToCoord(event->pos().x());
        double freq = m_spectrogramPlot->yAxis->pixelToCoord(event->pos().y());

        m_selHighFreq = freq;

        if (m_checkSnapToBar->isChecked()) {
            double barDuration = (60.0 / m_bpm) * 4.0;
            double draggedDuration = rawTime - m_selStartTime;


            int numBars = std::max(1, (int)std::round(draggedDuration / barDuration));
            m_selEndTime = m_selStartTime + (numBars * barDuration);
        } else {
            m_selEndTime = rawTime;
        }

        updateSelectionVisuals();
    }
}

void MainWindow::onSpectrogramMouseRelease(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_isSelecting) {
        m_isSelecting = false;


        if (m_selLowFreq > m_selHighFreq) std::swap(m_selLowFreq, m_selHighFreq);

        processSelection();
    }
}

void MainWindow::processSelection()
{
    if (m_audioData.empty()) return;

    int startSample = std::max(0, (int)(m_selStartTime * m_sampleRate));
    int endSample = std::min((int)m_audioData.size(), (int)(m_selEndTime * m_sampleRate));
    if (startSample >= endSample) return;

    std::vector<float> timeIsolatedRegion(m_audioData.begin() + startSample, m_audioData.begin() + endSample);

    std::vector<float> filteredAudio = applyBandpassFilter(timeIsolatedRegion, m_selLowFreq, m_selHighFreq);
    m_lastFilteredAudio = filteredAudio;

    QVector<double> x(filteredAudio.size()), y(filteredAudio.size());
    for (size_t i = 0; i < filteredAudio.size(); ++i) {
        x[i] = (double)i / m_sampleRate;
        y[i] = filteredAudio[i];
    }
    m_waveformPlot->graph(0)->setData(x, y);

    int windowSize = m_sampleRate / 100;
    QVector<double> energyX, energyY;

    for (size_t i = 0; i < filteredAudio.size() - windowSize; i += windowSize) {
        double sumSq = 0;
        for (int j = 0; j < windowSize; ++j) {
            sumSq += filteredAudio[i+j] * filteredAudio[i+j];
        }
        energyX.append((double)i / m_sampleRate);
        energyY.append(sqrt(sumSq / windowSize));
    }
    m_waveformPlot->graph(1)->setData(energyX, energyY);
    m_waveformPlot->clearItems();

    double totalDuration = (double)filteredAudio.size() / m_sampleRate;
    double stepDuration = (60.0 / m_bpm) / 4.0;


    m_numSteps = std::max(1, (int)std::round(totalDuration / stepDuration));


    m_stepTable->setColumnCount(m_numSteps);
    QStringList headers;
    for(int i = 1; i <= m_numSteps; ++i) headers << QString::number(i);
    m_stepTable->setHorizontalHeaderLabels(headers);

    double divisionStep = totalDuration / m_numSteps;

    for (int i = 1; i < m_numSteps; ++i) {
        QCPItemLine *gridLine = new QCPItemLine(m_waveformPlot);
        double lineX = i * divisionStep;

        gridLine->start->setCoords(lineX, -1000.0);
        gridLine->end->setCoords(lineX, 1000.0);
        gridLine->setPen(QPen(QColor(0, 0, 0, 150), 1, Qt::DashLine));
    }

    m_waveformPlot->rescaleAxes();
    m_waveformPlot->replot();

    int attackSample = extractADSR(filteredAudio);
    int yinSize = 4096;

    if (attackSample + yinSize > filteredAudio.size()) {
        yinSize = filteredAudio.size() - attackSample;
    }

    if (yinSize > 512) {
        std::vector<float> yinBuffer(filteredAudio.begin() + attackSample, filteredAudio.begin() + attackSample + yinSize);
        float detectedFreq = detectPitchYin(yinBuffer);
        m_detectedMidiNote = freqToMidi(detectedFreq);
        qDebug() << "YIN Detected Freq:" << detectedFreq << "Hz | MIDI:" << m_detectedMidiNote;
    }

    updateStepGrid(filteredAudio);
    m_stepTable->setVerticalHeaderLabels({QString("Note: %1").arg(m_detectedMidiNote)});
    m_lblInstrumentGuess->setText("Instrument Range: " + guessInstrument(m_selLowFreq, m_selHighFreq));

}

int MainWindow::extractADSR(const std::vector<float>& isolatedAudio)
{
    int window = m_sampleRate / 100;
    if (isolatedAudio.size() <= window) return 0;

    float maxVol = 0;
    int attackSample = 0;

    for (size_t i = 0; i < isolatedAudio.size() - window; i += window) {
        float sumSq = 0;
        for (int j = 0; j < window; ++j) sumSq += isolatedAudio[i+j] * isolatedAudio[i+j];
        float rms = sqrt(sumSq / window);

        if (rms > maxVol) {
            maxVol = rms;
            attackSample = i;
        }
    }

    double attackTimeMs = ((double)attackSample / m_sampleRate) * 1000.0;
    m_lblADSR->setText(QString("ADSR -> Attack Peak at: %1 ms | Max Amp: %2").arg(attackTimeMs, 0, 'f', 1).arg(maxVol, 0, 'f', 2));

    return attackSample;
}

void MainWindow::updateStepGrid(const std::vector<float>& isolatedAudio)
{
    m_surgicalNotes.clear();


    for (int i = 0; i < m_numSteps; ++i) {
        QTableWidgetItem *item = m_stepTable->item(0, i);
        if (!item) {
            item = new QTableWidgetItem("");
            m_stepTable->setItem(0, i, item);
        } else {
            item->setText("");
        }
        item->setBackground(QBrush(Qt::white));
    }

    if (isolatedAudio.empty()) return;


    double stepDuration = (60.0 / m_bpm) / 4.0;
    double ticksPerSecond = (m_bpm / 60.0) * 48.0;

    int window = m_sampleRate / 100;
    std::vector<float> envelope;

    for (size_t i = 0; i < isolatedAudio.size(); i += window) {
        float sumSq = 0;
        int count = 0;
        for (int j = 0; j < window && (i + j) < isolatedAudio.size(); ++j) {
            sumSq += isolatedAudio[i+j] * isolatedAudio[i+j];
            count++;
        }
        envelope.push_back(sqrt(sumSq / count));
    }

    float maxVol = 0;
    for (float v : envelope) { if (v > maxVol) maxVol = v; }
    if (maxVol == 0) return;

    float threshold = maxVol * (m_noteThreshold / 100.0f);

    for (size_t i = 1; i < envelope.size() - 1; ++i) {

        if (envelope[i] > envelope[i-1] && envelope[i] > envelope[i+1] && envelope[i] > threshold) {


            double timeInSelection = (i * window) / (double)m_sampleRate;


            double lmmsTickPos = timeInSelection * ticksPerSecond;
            int visualStep = (int)(timeInSelection / stepDuration);


            if (visualStep >= m_numSteps) visualStep = m_numSteps - 1;
            if (visualStep < 0) visualStep = 0;


            int noteVol = qBound(10, (int)((envelope[i] / maxVol) * 100.0), 100);



            int exactSample = i * window;
            int transientOffset = m_sampleRate * 0.04;
            int analysisStart = exactSample + transientOffset;


            if (analysisStart + 512 > isolatedAudio.size()) {
                analysisStart = exactSample;
            }

            int analysisSize = 8192;
            if (analysisStart + analysisSize > isolatedAudio.size()) {
                analysisSize = isolatedAudio.size() - analysisStart;
            }

            ExtractedNote newNote;
            newNote.startTick = lmmsTickPos;
            newNote.lengthTicks = 12.0;
            newNote.volume = noteVol;
            newNote.visualStep = visualStep;

            if (analysisSize > 512) {
                std::vector<float> analysisBuffer(isolatedAudio.begin() + analysisStart, isolatedAudio.begin() + analysisStart + analysisSize);

                int algoIndex = m_comboAlgorithm->currentIndex();
                switch(algoIndex) {
                case 0:
                    newNote.midiNote = freqToMidi(detectPitchYin(analysisBuffer));
                    break;
                case 1:
                    newNote.midiNote = freqToMidi(detectPitchHPS(analysisBuffer));
                    break;
                case 2:
                    newNote.chord = detectChord(analysisBuffer);
                    break;
                case 3: // Template Chord
                    newNote.chord = detectChordTemplate(analysisBuffer);
                    break;
                }
            }

            if (newNote.midiNote > 0 || !newNote.chord.isEmpty()) {
                m_surgicalNotes.push_back(newNote);
            }
        }
    }


    for (const auto& note : m_surgicalNotes) {
        if (note.visualStep >= 0 && note.visualStep < m_numSteps) {
            QString cellText;
            if (m_comboAlgorithm->currentIndex() >= 2) {
                cellText = note.chord;
            } else {
                cellText = QString::number(note.midiNote) + "\n" + midiToNoteName(note.midiNote);
            }


            cellText += QString("\nVol: %1").arg(note.volume);

            m_stepTable->item(0, note.visualStep)->setText(cellText);
            m_stepTable->item(0, note.visualStep)->setTextAlignment(Qt::AlignCenter);


            int alpha = qBound(50, (note.volume * 255) / 100, 255);
            m_stepTable->item(0, note.visualStep)->setBackground(QBrush(QColor(255, 0, 0, alpha)));
        }
    }
}


void MainWindow::exportLMMSSurgical()
{
    if (m_surgicalNotes.empty()) {
        QMessageBox::warning(this, "Empty", "No notes detected to export!");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Save Pattern", "extracted_pattern.xpt", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project creator=\"LMMS\" type=\"pattern\" version=\"20\" creatorversion=\"1.3.0\">\n";
    out << "  <head/>\n";

    out << "  <pattern muted=\"0\" steps=\"" << m_numSteps << "\" type=\"1\" pos=\"0\" name=\"Extracted\">\n";

    for (const auto& note : m_surgicalNotes) {
        if (note.midiNote > 0) {
            // Write exact sub-grid position and dynamic volume
            out << "    <note pan=\"0\" key=\"" << note.midiNote
                << "\" len=\"" << (int)note.lengthTicks
                << "\" pos=\"" << (int)note.startTick
                << "\" vol=\"" << note.volume << "\"/>\n";
        } else if (!note.chord.isEmpty()) {

            std::vector<double> freqs = chordToFrequencies(note.chord);
            for (double f : freqs) {
                int midi = freqToMidi(f);
                if (midi > 0) {
                    out << "    <note pan=\"0\" key=\"" << midi
                        << "\" len=\"" << (int)note.lengthTicks
                        << "\" pos=\"" << (int)note.startTick
                        << "\" vol=\"" << note.volume << "\"/>\n";
                }
            }
        }
    }

    out << "  </pattern>\n";
    out << "</lmms-project>\n";
    file.close();
    QMessageBox::information(this, "Success", "Exported!");
}

void MainWindow::onStepsChanged() { m_numSteps = m_comboSteps->currentIndex() == 0 ? 16 : 32; m_stepTable->setColumnCount(m_numSteps); }
void MainWindow::onBpmChanged() { m_bpm = m_spinBPM->value(); }

std::vector<float> MainWindow::applyBandpassFilter(const std::vector<float>& input, double lowFreq, double highFreq)
{
    if (input.empty()) return {};

    std::vector<float> filtered(input.size());


    Biquad hpf;
    hpf.setHPF(m_sampleRate, lowFreq, 0.707f);


    Biquad lpf;
    lpf.setLPF(m_sampleRate, highFreq, 0.707f);


    for (size_t i = 0; i < input.size(); ++i) {
        float sample = hpf.process(input[i]);
        filtered[i] = lpf.process(sample);
    }

    return filtered;
}

float MainWindow::detectPitchYin(const std::vector<float>& buffer)
{
    int halfSize = buffer.size() / 2;
    if (halfSize == 0) return 0.0f;

    std::vector<float> yinBuffer(halfSize, 0.0f);

    for (int tau = 1; tau < halfSize; tau++) {
        for (int i = 0; i < halfSize; i++) {
            float delta = buffer[i] - buffer[i + tau];
            yinBuffer[tau] += delta * delta;
        }
    }

    yinBuffer[0] = 1;
    float runningSum = 0;
    for (int tau = 1; tau < halfSize; tau++) {
        runningSum += yinBuffer[tau];
        yinBuffer[tau] *= tau / runningSum;
    }

    int tauEstimate = -1;
    float threshold = 0.15f;
    for (int tau = 2; tau < halfSize; tau++) {
        if (yinBuffer[tau] < threshold) {
            while (tau + 1 < halfSize && yinBuffer[tau + 1] < yinBuffer[tau]) {
                tau++;
            }
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate == -1) {
        float minVal = 1.0f;
        for (int tau = 2; tau < halfSize; tau++) {
            if (yinBuffer[tau] < minVal) {
                minVal = yinBuffer[tau];
                tauEstimate = tau;
            }
        }
    }

    if (tauEstimate > 0) {
        return (float)m_sampleRate / tauEstimate;
    }
    return 0.0f;
}

int MainWindow::freqToMidi(float freq) {
    if (freq <= 0) return 0;
    return qRound(12 * log2(freq / 440.0) + 69);
}

void MainWindow::onOffsetChanged() {
    m_gridOffset = m_spinOffset->value();
}

void MainWindow::autoDetectBPM()
{
    if (m_audioData.empty()) {
        QMessageBox::warning(this, "Error", "Load an audio file first!");
        return;
    }

    int downsampleFactor = m_sampleRate / 100;
    std::vector<float> envelope;
    float previousSample = 0;

    bool useAttackMode = (m_comboBpmType->currentIndex() == 0);

    for (size_t i = 1; i < m_audioData.size() - downsampleFactor; i += downsampleFactor) {
        float sum = 0;
        for (int j = 0; j < downsampleFactor; ++j) {
            float currentAbs = std::abs(m_audioData[i+j]);
            if (useAttackMode) {
                float diff = currentAbs - previousSample;
                if (diff > 0) sum += diff;
            } else {
                sum += currentAbs;
            }
            previousSample = currentAbs;
        }
        envelope.push_back(sum);
    }

    int minBpm = 60;
    int maxBpm = 200;
    int minLag = (60.0 / maxBpm) * 100;
    int maxLag = (60.0 / minBpm) * 100;

    float maxCorrelation = 0;
    float averageCorrelation = 0;
    int bestLag = 0;

    size_t scanLength = std::min(envelope.size(), (size_t)(15 * 100));

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float correlation = 0;
        for (size_t i = 0; i < scanLength - lag; ++i) {
            correlation += envelope[i] * envelope[i + lag];
        }

        averageCorrelation += correlation;

        if (correlation > maxCorrelation) {
            maxCorrelation = correlation;
            bestLag = lag;
        }
    }

    averageCorrelation /= (maxLag - minLag + 1);

    if (bestLag > 0 && maxCorrelation > (averageCorrelation * 1.2f)) {
        double detectedBpm = 60.0 / (bestLag / 100.0);
        int roundedBpm = qRound(detectedBpm);
        m_spinBPM->setValue(roundedBpm);
    } else {
        QMessageBox::warning(this, "BPM Detection Failed",
                             "Could not detect a clear tempo. Try switching the BPM algorithm dropdown or setting it manually.");
    }
}

void MainWindow::playFilteredSelection()
{
    if (m_lastFilteredAudio.empty()) return;

    QString tempPath = QDir::tempPath() + "/harmonic_temp.wav";
    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly)) return;

    struct {
        char riff[4] = {'R','I','F','F'};
        uint32_t fileSize;
        char wave[4] = {'W','A','V','E'};
        char fmt[4] = {'f','m','t',' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 3;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 44100;
        uint32_t byteRate = 44100 * 4;
        uint16_t blockAlign = 4;
        uint16_t bitsPerSample = 32;
        char data[4] = {'d','a','t','a'};
        uint32_t dataSize;
    } header;

    header.sampleRate = m_sampleRate;
    header.byteRate = m_sampleRate * 4;
    header.dataSize = m_lastFilteredAudio.size() * sizeof(float);
    header.fileSize = 36 + header.dataSize;

    file.write((const char*)&header, sizeof(header));
    file.write((const char*)m_lastFilteredAudio.data(), header.dataSize);
    file.close();

    m_player->setSource(QUrl::fromLocalFile(tempPath));
    m_player->play();
}

QString MainWindow::midiToNoteName(int midiNote) {
    if (midiNote <= 0) return "";

    const QString notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int noteIndex = midiNote % 12;
    int octave = (midiNote / 12) - 1;

    return notes[noteIndex] + QString::number(octave);
}

QString MainWindow::detectChord(const std::vector<float>& buffer) {
    if (buffer.empty()) return "";

    int N = 1;
    while (N < buffer.size()) N *= 2;
    N = std::min(N, 8192);

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, NULL, NULL);
    std::vector<kiss_fft_cpx> cx_in(N, {0.0f, 0.0f});
    std::vector<kiss_fft_cpx> cx_out(N, {0.0f, 0.0f});

    for (size_t i = 0; i < std::min(buffer.size(), (size_t)N); ++i) {
        float win = 0.5 * (1 - cos(2 * M_PI * i / (N - 1)));
        cx_in[i].r = buffer[i] * win;
    }

    kiss_fft(cfg, cx_in.data(), cx_out.data());
    free(cfg);

    double chroma[12] = {0};
    double binFreqResolution = (double)m_sampleRate / N;

    for (int i = 1; i < N / 2; ++i) {
        double freq = i * binFreqResolution;
        if (freq < 50 || freq > 2000) continue;

        double midiFloat = 12 * log2(freq / 440.0) + 69;
        int pitchClass = qRound(midiFloat) % 12;
        if (pitchClass < 0) pitchClass += 12;

        double mag = sqrt(cx_out[i].r * cx_out[i].r + cx_out[i].i * cx_out[i].i);
        chroma[pitchClass] += mag;
    }

    int root = 0;
    double maxChroma = 0;
    for(int i = 0; i < 12; i++) {
        if(chroma[i] > maxChroma) {
            maxChroma = chroma[i];
            root = i;
        }
    }

    int majorThird = (root + 4) % 12;
    int minorThird = (root + 3) % 12;

    const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    QString rootString = noteNames[root];

    if (chroma[minorThird] > chroma[majorThird]) {
        return rootString + " Min";
    } else {
        return rootString + " Maj";
    }
}

void MainWindow::onThresholdChanged()
{
    m_noteThreshold = m_spinThreshold->value();
    if (!m_lastFilteredAudio.empty()) {
        processSelection();
    }
}

void MainWindow::exportLMMSProject()
{
    if (m_audioData.empty()) {
        QMessageBox::warning(this, "Error", "Please load an audio file first.");
        return;
    }

    int numBands = m_bandTable->rowCount();
    if (numBands == 0) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Save Multi-Track Project", "AutoTranscribed.mmp", "LMMS Project (*.mmp)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);


    double stepDuration = (60.0 / m_bpm) / 4.0;
    int samplesPerStep = m_sampleRate * stepDuration;
    int totalSteps = m_audioData.size() / samplesPerStep;
    int ticksPerStep = 48;

    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(numBands);
    m_progressBar->setValue(0);


    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project version=\"20\" creatorversion=\"1.3.0\" type=\"song\" creator=\"LMMS\">\n";
    out << "  <head timesig_denominator=\"4\" masterpitch=\"0\" bpm=\"" << m_bpm << "\" mastervol=\"100\" timesig_numerator=\"4\"/>\n";
    out << "  <song>\n";


    out << "    <trackcontainer minimized=\"0\" type=\"song\" height=\"300\" width=\"720\" maximized=\"0\" y=\"5\" visible=\"1\" x=\"5\">\n";


    for (int t = 0; t < numBands; ++t) {
        QString bandName = m_bandTable->item(t, 0)->text().toHtmlEscaped();
        double lowFreq = m_bandTable->item(t, 1)->text().toDouble();
        double highFreq = m_bandTable->item(t, 2)->text().toDouble();

        QComboBox *algoCombo = qobject_cast<QComboBox*>(m_bandTable->cellWidget(t, 3));
        int algoIndex = algoCombo ? algoCombo->currentIndex() : 0;


        out << "      <track solo=\"0\" type=\"0\" muted=\"0\" mutedBeforeSolo=\"0\" name=\"" << bandName << "\">\n";


        out << R"(        <instrumenttrack fxch="0" basenote="69" vol="100" usemasterpitch="1" enablecc="0" pitch="0" firstkey="0" pan="0" lastkey="127" pitchrange="1">
          <midicontrollers cc106="0" cc65="0" cc102="0" cc107="0" cc120="0" cc27="0" cc51="0" cc3="0" cc108="0" cc117="0" cc35="0" cc18="0" cc87="0" cc20="0" cc92="0" cc125="0" cc109="0" cc61="0" cc123="0" cc24="0" cc62="0" cc12="0" cc52="0" cc79="0" cc44="0" cc16="0" cc77="0" cc59="0" cc74="0" cc82="0" cc33="0" cc40="0" cc84="0" cc70="0" cc127="0" cc46="0" cc85="0" cc63="0" cc89="0" cc96="0" cc113="0" cc69="0" cc86="0" cc97="0" cc55="0" cc100="0" cc116="0" cc73="0" cc81="0" cc111="0" cc83="0" cc8="0" cc11="0" cc50="0" cc14="0" cc48="0" cc103="0" cc5="0" cc105="0" cc34="0" cc115="0" cc31="0" cc56="0" cc72="0" cc122="0" cc58="0" cc91="0" cc9="0" cc126="0" cc1="0" cc57="0" cc75="0" cc99="0" cc28="0" cc124="0" cc43="0" cc76="0" cc25="0" cc22="0" cc64="0" cc36="0" cc4="0" cc78="0" cc66="0" cc112="0" cc121="0" cc26="0" cc23="0" cc95="0" cc114="0" cc7="0" cc41="0" cc42="0" cc119="0" cc13="0" cc29="0" cc38="0" cc45="0" cc101="0" cc2="0" cc39="0" cc49="0" cc32="0" cc94="0" cc90="0" cc37="0" cc60="0" cc21="0" cc53="0" cc88="0" cc71="0" cc68="0" cc0="0" cc10="0" cc19="0" cc93="0" cc98="0" cc30="0" cc54="0" cc118="0" cc17="0" cc6="0" cc47="0" cc110="0" cc15="0" cc104="0" cc67="0" cc80="0"/>
          <instrument name="tripleoscillator">
            <tripleoscillator vol0="33" pan1="0" stphdetun0="0" vol2="33" pan2="0" modalgo3="2" stphdetun1="0" stphdetun2="0" coarse0="0" userwavefile2="" modalgo1="2" userwavefile1="" modalgo2="2" phoffset0="0" finer2="0" userwavefile0="" phoffset2="0" finer1="0" finel1="0" wavetype2="0" phoffset1="0" coarse1="-12" wavetype1="0" finel2="0" pan0="0" vol1="33" finer0="0" wavetype0="0" coarse2="-24" finel0="0">
              <key/>
            </tripleoscillator>
          </instrument>
          <eldata fres="0.5" fwet="0" ftype="0" fcut="14000">
            <elvol lspd_syncmode="0" latt="0" rel="0.1" lshp="0" sustain="0.5" att="0" dec="0.5" lspd_numerator="4" hold="0.5" amt="0" lspd="0.1" lspd_denominator="4" x100="0" ctlenvamt="0" userwavefile="" lamt="0" lpdel="0" pdel="0"/>
            <elcut lspd_syncmode="0" latt="0" rel="0.1" lshp="0" sustain="0.5" att="0" dec="0.5" lspd_numerator="4" hold="0.5" amt="0" lspd="0.1" lspd_denominator="4" x100="0" ctlenvamt="0" userwavefile="" lamt="0" lpdel="0" pdel="0"/>
            <elres lspd_syncmode="0" latt="0" rel="0.1" lshp="0" sustain="0.5" att="0" dec="0.5" lspd_numerator="4" hold="0.5" amt="0" lspd="0.1" lspd_denominator="4" x100="0" ctlenvamt="0" userwavefile="" lamt="0" lpdel="0" pdel="0"/>
          </eldata>
          <chordcreator chord="0" chord-enabled="0" chordrange="1"/>
          <arpeggiator arpmiss="0" arp="0" arpgate="100" arp-enabled="0" arptime="200" arpmode="0" arptime_syncmode="0" arpskip="0" arpcycle="0" arprepeats="1" arptime_numerator="4" arptime_denominator="4" arprange="1" arpdir="0"/>
          <midiport inputchannel="0" fixedoutputnote="-1" outputchannel="1" inputcontroller="0" outputprogram="1" readable="0" outputcontroller="0" fixedinputvelocity="-1" fixedoutputvelocity="-1" basevelocity="63" writable="0"/>
          <fxchain enabled="0" numofeffects="0"/>
        </instrumenttrack>
)";


        out << "        <pattern muted=\"0\" type=\"1\" pos=\"0\" name=\"Extracted\" steps=\"" << totalSteps << "\">\n";


        for (int step = 0; step < totalSteps; ++step) {
            int startSample = step * samplesPerStep;

            int windowSize = 8192;
            if (startSample + windowSize > m_audioData.size()) continue;

            std::vector<float> chunk(m_audioData.begin() + startSample, m_audioData.begin() + startSample + windowSize);

            float maxVol = 0;
            for(float v : chunk) { if(std::abs(v) > maxVol) maxVol = std::abs(v); }

            if (maxVol > (m_noteThreshold / 100.0f)) {
                std::vector<float> filteredChunk = applyBandpassFilter(chunk, lowFreq, highFreq);

                int detectedMidi = 0;
                QString detectedChord = "";

                switch(algoIndex) {
                case 0: detectedMidi = freqToMidi(detectPitchYin(filteredChunk)); break;
                case 1: detectedMidi = freqToMidi(detectPitchHPS(filteredChunk)); break;
                case 2: detectedChord = detectChord(filteredChunk); break;
                case 3: detectedChord = detectChordTemplate(filteredChunk); break;
                }

                int notePos = step * ticksPerStep;


                if (algoIndex < 2 && detectedMidi > 0) {
                    out << "          <note vol=\"100\" len=\"" << ticksPerStep << "\" pan=\"0\" pos=\"" << notePos << "\" key=\"" << detectedMidi << "\"/>\n";
                }
                else if (algoIndex >= 2 && !detectedChord.isEmpty()) {
                    std::vector<double> freqs = chordToFrequencies(detectedChord);
                    for (double f : freqs) {
                        int midi = freqToMidi(f);
                        if (midi > 0) {
                            out << "          <note vol=\"100\" len=\"" << ticksPerStep << "\" pan=\"0\" pos=\"" << notePos << "\" key=\"" << midi << "\"/>\n";
                        }
                    }
                }
            }
        }

        out << "        </pattern>\n";
        out << "      </track>\n";

        m_progressBar->setValue(t + 1);
        QCoreApplication::processEvents();
    }

    out << "    </trackcontainer>\n";


    out << R"(    <track mutedBeforeSolo="0" muted="0" solo="0" type="6" name="Automation track">
      <automationtrack/>
      <automationpattern len="192" tens="1" pos="0" name="Numerator" mute="0" prog="0"/>
      <automationpattern len="192" tens="1" pos="0" name="Denominator" mute="0" prog="0"/>
      <automationpattern len="192" tens="1" pos="0" name="Tempo" mute="0" prog="0"/>
      <automationpattern len="192" tens="1" pos="0" name="Master volume" mute="0" prog="0"/>
      <automationpattern len="192" tens="1" pos="0" name="Master pitch" mute="0" prog="0"/>
    </track>
    <fxmixer height="333" maximized="0" y="310" minimized="0" x="5" visible="1" width="543">
      <fxchannel muted="0" soloed="0" name="Master" volume="1" num="0">
        <fxchain enabled="0" numofeffects="0"/>
      </fxchannel>
    </fxmixer>
    <timeline lp0pos="0" lpstate="0" lp1pos="192" stopbehaviour="1"/>
  </song>
</lmms-project>
)";

    file.close();
    m_progressBar->setVisible(false);
    QMessageBox::information(this, "Success", "Analysed the whole song and generated multi-track LMMS project!");
}




QString MainWindow::guessInstrument(double lowFreq, double highFreq)
{

    double centerFreq = (lowFreq + highFreq) / 2.0;

    if (centerFreq < 60) return "Sub Bass (808s, Deep Synths, Rumble)";
    if (centerFreq < 250) return "Bass / Kick Drum / Lower Fundamentals";
    if (centerFreq < 500) return "Low Mids (Snare body, Cellos, Toms, Warmth)";
    if (centerFreq < 2000) return "Midrange (Vocals, Guitars, Synths, Attack)";
    if (centerFreq < 5000) return "Upper Mids (Snare crack, Vocal presence, Cymbals)";

    return "Highs / Air (Hi-hats, Breaths, Sizzle)";
}

void MainWindow::saveFilteredSelection()
{
    if (m_lastFilteredAudio.empty()) {
        QMessageBox::warning(this, "Empty", "No selection to save! Draw a box on the spectrogram first.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Save Filtered Audio", "spectral_sample.wav", "WAV Files (*.wav)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) return;


    struct {
        char riff[4] = {'R','I','F','F'};
        uint32_t fileSize;
        char wave[4] = {'W','A','V','E'};
        char fmt[4] = {'f','m','t',' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 3;
        uint16_t numChannels = 1; // Mono export from selection
        uint32_t sampleRate = 44100;
        uint32_t byteRate = 44100 * 4;
        uint16_t blockAlign = 4;
        uint16_t bitsPerSample = 32;
        char data[4] = {'d','a','t','a'};
        uint32_t dataSize;
    } header;

    header.sampleRate = m_sampleRate;
    header.byteRate = m_sampleRate * 4;
    header.dataSize = m_lastFilteredAudio.size() * sizeof(float);
    header.fileSize = 36 + header.dataSize;

    file.write((const char*)&header, sizeof(header));
    file.write((const char*)m_lastFilteredAudio.data(), header.dataSize);
    file.close();

    QMessageBox::information(this, "Success", "Sample saved! You can now drag this directly into LMMS.");
}
float MainWindow::detectPitchHPS(const std::vector<float>& buffer)
{
    if (buffer.empty()) return 0.0f;

    int N = 1;
    while (N < buffer.size()) N *= 2;
    N = std::min(N, 8192); // Cap FFT size for performance

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, NULL, NULL);
    std::vector<kiss_fft_cpx> cx_in(N, {0.0f, 0.0f});
    std::vector<kiss_fft_cpx> cx_out(N, {0.0f, 0.0f});


    for (size_t i = 0; i < std::min(buffer.size(), (size_t)N); ++i) {
        float win = 0.5 * (1 - cos(2 * M_PI * i / (N - 1)));
        cx_in[i].r = buffer[i] * win;
    }

    kiss_fft(cfg, cx_in.data(), cx_out.data());
    free(cfg);

    int numBins = N / 2;
    std::vector<float> magnitude(numBins, 0.0f);
    for (int i = 0; i < numBins; ++i) {
        magnitude[i] = sqrt(cx_out[i].r * cx_out[i].r + cx_out[i].i * cx_out[i].i);
    }


    std::vector<float> hps = magnitude;
    int downsampleRatios = 3;

    for (int ratio = 2; ratio <= downsampleRatios; ++ratio) {
        for (int i = 0; i < numBins / ratio; ++i) {
            hps[i] *= magnitude[i * ratio];
        }
    }


    float maxVal = 0;
    int peakBin = 0;


    int startBin = (50.0 * N) / m_sampleRate;

    for (int i = startBin; i < numBins / downsampleRatios; ++i) {
        if (hps[i] > maxVal) {
            maxVal = hps[i];
            peakBin = i;
        }
    }

    return (float)peakBin * m_sampleRate / N;
}
QString MainWindow::detectChordTemplate(const std::vector<float>& buffer)
{
    if (buffer.empty()) return "";


    int N = 1;
    while (N < buffer.size()) N *= 2;
    N = std::min(N, 8192);

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, NULL, NULL);
    std::vector<kiss_fft_cpx> cx_in(N, {0.0f, 0.0f}), cx_out(N, {0.0f, 0.0f});

    for (size_t i = 0; i < std::min(buffer.size(), (size_t)N); ++i) {
        float win = 0.5 * (1 - cos(2 * M_PI * i / (N - 1)));
        cx_in[i].r = buffer[i] * win;
    }
    kiss_fft(cfg, cx_in.data(), cx_out.data());
    free(cfg);

    double chroma[12] = {0};
    double binFreqResolution = (double)m_sampleRate / N;

    for (int i = 1; i < N / 2; ++i) {
        double freq = i * binFreqResolution;
        if (freq < 50 || freq > 2000) continue;

        double midiFloat = 12 * log2(freq / 440.0) + 69;
        int pitchClass = qRound(midiFloat) % 12;
        if (pitchClass < 0) pitchClass += 12;

        chroma[pitchClass] += sqrt(cx_out[i].r * cx_out[i].r + cx_out[i].i * cx_out[i].i);
    }


    double chromaSum = 0;
    for (int i = 0; i < 12; ++i) chromaSum += chroma[i];
    if (chromaSum > 0) {
        for (int i = 0; i < 12; ++i) chroma[i] /= chromaSum;
    }


    std::vector<std::vector<double>> majorTemplates(12, std::vector<double>(12, 0.0));
    std::vector<std::vector<double>> minorTemplates(12, std::vector<double>(12, 0.0));

    for (int i = 0; i < 12; ++i) {
        majorTemplates[i][i] = 1.0;                           // Root
        majorTemplates[i][(i + 4) % 12] = 1.0;                // Major 3rd
        majorTemplates[i][(i + 7) % 12] = 1.0;                // Perfect 5th

        minorTemplates[i][i] = 1.0;                           // Root
        minorTemplates[i][(i + 3) % 12] = 1.0;                // Minor 3rd
        minorTemplates[i][(i + 7) % 12] = 1.0;                // Perfect 5th
    }


    double bestScore = -1.0;
    QString bestChord = "";
    const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    for (int i = 0; i < 12; ++i) {
        double majorScore = 0, minorScore = 0;
        for (int j = 0; j < 12; ++j) {
            majorScore += chroma[j] * majorTemplates[i][j];
            minorScore += chroma[j] * minorTemplates[i][j];
        }

        if (majorScore > bestScore) {
            bestScore = majorScore;
            bestChord = noteNames[i] + " Maj";
        }
        if (minorScore > bestScore) {
            bestScore = minorScore;
            bestChord = noteNames[i] + " Min";
        }
    }

    return bestChord;
}
std::vector<double> MainWindow::chordToFrequencies(const QString& chordName)
{
    if (chordName.isEmpty()) return {};
    QStringList parts = chordName.split(" ");
    if (parts.size() != 2) return {};
    QString rootStr = parts[0];
    QString type = parts[1];

    const QStringList notes = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int rootIndex = notes.indexOf(rootStr);
    if (rootIndex == -1) return {};

    int rootMidi = 48 + rootIndex; // C3
    std::vector<int> midiNotes = {rootMidi, rootMidi + 7}; // Root and 5th

    if (type == "Maj") midiNotes.push_back(rootMidi + 4); // Major 3rd
    else midiNotes.push_back(rootMidi + 3);               // Minor 3rd

    std::vector<double> freqs;
    for (int midi : midiNotes) {
        freqs.push_back(440.0 * std::pow(2.0, (midi - 69) / 12.0));
    }
    return freqs;
}

void MainWindow::playSynthesizedPattern()
{
    m_player->stop();

    double stepDuration = (60.0 / m_bpm) / 4.0;
    int samplesPerStep = m_sampleRate * stepDuration;
    int totalSamples = m_numSteps * samplesPerStep;
    std::vector<float> synthBuffer(totalSamples, 0.0f);

    int fadeSamples = m_sampleRate * 0.01;

    for (int i = 0; i < m_numSteps; ++i) {
        std::vector<double> activeFreqs;
        double noteVolumeScale = 1.0;


        for (const auto& note : m_surgicalNotes) {
            if (note.visualStep == i) {
                if (m_comboAlgorithm->currentIndex() >= 2) {
                    if (!note.chord.isEmpty()) {
                        activeFreqs = chordToFrequencies(note.chord);
                    }
                } else if (note.midiNote > 0) {
                    activeFreqs.push_back(440.0 * std::pow(2.0, (note.midiNote - 69) / 12.0));
                }


                noteVolumeScale = note.volume / 100.0;

                break;
            }
        }

        if (activeFreqs.empty()) continue;

        int startSample = i * samplesPerStep;

        for (int s = 0; s < samplesPerStep; ++s) {
            double t = (double)s / m_sampleRate;
            double sample = 0.0;

            for (double f : activeFreqs) {
                sample += std::sin(2.0 * M_PI * f * t);
            }
            sample /= activeFreqs.size();

            double envelope = 1.0;
            if (s < fadeSamples) {
                envelope = (double)s / fadeSamples;
            } else if (s > samplesPerStep - fadeSamples) {
                envelope = (double)(samplesPerStep - s) / fadeSamples;
            }


            synthBuffer[startSample + s] = sample * envelope * noteVolumeScale * 0.5f;
        }
    }



    QString tempPath = QDir::tempPath() + "/synth_temp.wav";
    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly)) return;

    struct {
        char riff[4] = {'R','I','F','F'};
        uint32_t fileSize;
        char wave[4] = {'W','A','V','E'};
        char fmt[4] = {'f','m','t',' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 3;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 44100;
        uint32_t byteRate = 44100 * 4;
        uint16_t blockAlign = 4;
        uint16_t bitsPerSample = 32;
        char data[4] = {'d','a','t','a'};
        uint32_t dataSize;
    } header;

    header.sampleRate = m_sampleRate;
    header.byteRate = m_sampleRate * 4;
    header.dataSize = synthBuffer.size() * sizeof(float);
    header.fileSize = 36 + header.dataSize;

    file.write((const char*)&header, sizeof(header));
    file.write((const char*)synthBuffer.data(), header.dataSize);
    file.close();

    m_player->setSource(QUrl::fromLocalFile(tempPath));
    m_player->play();
}

void MainWindow::onAddBandClicked()
{
    m_bandTable->blockSignals(true);

    int row = m_bandTable->rowCount();
    m_bandTable->insertRow(row);

    m_bandTable->setItem(row, 0, new QTableWidgetItem("New Band"));
    m_bandTable->setItem(row, 1, new QTableWidgetItem("500"));
    m_bandTable->setItem(row, 2, new QTableWidgetItem("1000"));

    QComboBox *algoCombo = new QComboBox();
    algoCombo->addItems({
        "Note: YIN (Default)",
        "Note: Harmonic Product Spectrum",
        "Chord: Basic Peak Picker",
        "Chord: Template Matching"
    });
    m_bandTable->setCellWidget(row, 3, algoCombo);

    m_bandTable->blockSignals(false);
    updateBandVisuals();
}

void MainWindow::onDeleteBandClicked()
{
    int row = m_bandTable->currentRow();
    if (row >= 0) {
        m_bandTable->blockSignals(true);
        m_bandTable->removeRow(row);
        m_bandTable->blockSignals(false);
        updateBandVisuals();
    }
}
void MainWindow::updateBandVisuals()
{

    for (QCPItemRect *rect : std::as_const(m_bandVisualRects)) {
        m_multiSpectrogramPlot->removeItem(rect);
    }
    m_bandVisualRects.clear();


    double maxTime = 10.0;
    if (!m_audioData.empty()) {
        maxTime = (double)m_audioData.size() / m_sampleRate;
    }


    QList<QColor> bandColors = {
        QColor(255, 0, 0, 60),    // Red
        QColor(0, 255, 0, 60),    // Green
        QColor(0, 150, 255, 60),  // Blue
        QColor(255, 165, 0, 60),  // Orange
        QColor(255, 0, 255, 60)   // Magenta
    };


    for (int i = 0; i < m_bandTable->rowCount(); ++i) {

        if (!m_bandTable->item(i, 1) || !m_bandTable->item(i, 2)) continue;

        double lowFreq = m_bandTable->item(i, 1)->text().toDouble();
        double highFreq = m_bandTable->item(i, 2)->text().toDouble();

        QCPItemRect *rect = new QCPItemRect(m_multiSpectrogramPlot);


        QColor color = bandColors[i % bandColors.size()];
        rect->setPen(QPen(color.darker(), 2));
        rect->setBrush(QBrush(color));


        rect->topLeft->setCoords(0, highFreq);
        rect->bottomRight->setCoords(maxTime, lowFreq);

        m_bandVisualRects.append(rect);
    }

    m_multiSpectrogramPlot->replot();
}

double MainWindow::snapTimeToGrid(double rawTime)
{
    if (!m_checkSnapToBar->isChecked() || m_bpm <= 0) return rawTime;

    double secondsPerBeat = 60.0 / m_bpm;

    double relativeTime = rawTime - m_gridOffset;
    double snappedRelative = std::round(relativeTime / secondsPerBeat) * secondsPerBeat;

    return m_gridOffset + snappedRelative;
}

void MainWindow::updateSelectionVisuals()
{
    m_selectionBox->topLeft->setCoords(m_selStartTime, m_selHighFreq);
    m_selectionBox->bottomRight->setCoords(m_selEndTime, m_selLowFreq);
    m_selectionBox->setVisible(true);
    m_spectrogramPlot->replot();
}
void MainWindow::autoDetectOffset()
{
    if (m_audioData.empty()) {
        QMessageBox::warning(this, "Error", "Load an audio file first!");
        return;
    }


    int scanSamples = std::min((int)m_audioData.size(), m_sampleRate * 10);
    int window = m_sampleRate / 100; // 10ms window

    float maxLowEnergy = 0;
    int bestSample = 0;


    std::vector<float> intro(m_audioData.begin(), m_audioData.begin() + scanSamples);
    std::vector<float> lowFreqs = applyBandpassFilter(intro, 20.0, 150.0);

    for (size_t i = 0; i < lowFreqs.size() - window; i += window) {
        float sumSq = 0;
        for (int j = 0; j < window; ++j) {
            sumSq += lowFreqs[i+j] * lowFreqs[i+j];
        }
        float rms = sqrt(sumSq / window);


        if (rms > maxLowEnergy) {
            maxLowEnergy = rms;
            bestSample = i;
        }
    }


    double offsetSeconds = std::max(0.0, ((double)bestSample / m_sampleRate) - 0.01);

    m_spinOffset->setValue(offsetSeconds);
    m_gridOffset = offsetSeconds;

    QMessageBox::information(this, "Offset Detected", QString("First major downbeat detected at %1 seconds.").arg(offsetSeconds, 0, 'f', 3));
}

void MainWindow::nudgeSelectionLeft()
{
    if (m_selStartTime == m_selEndTime) return;

    double barDuration = (60.0 / m_bpm) * 4.0;
    m_selStartTime -= barDuration;
    m_selEndTime -= barDuration;

    if (m_selStartTime < 0) {
        m_selStartTime = 0;
        m_selEndTime = barDuration;
    }

    updateSelectionVisuals();
    processSelection();
}

void MainWindow::nudgeSelectionRight()
{
    if (m_selStartTime == m_selEndTime) return;

    double barDuration = (60.0 / m_bpm) * 4.0;
    m_selStartTime += barDuration;
    m_selEndTime += barDuration;

    double maxTime = (double)m_audioData.size() / m_sampleRate;
    if (m_selEndTime > maxTime) return;
    updateSelectionVisuals();
    processSelection();
}
void MainWindow::onLoadSepFileClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "", "Audio Files (*.wav *.mp3 *.flac)");
    if (fileName.isEmpty()) return;

    m_currentSepFilePath = fileName;


    if (loadAudio(fileName)) {
        generateSpectrogram(); // This calculates the FFT math for the visuals
        QMessageBox::information(this, "Loaded", "Audio loaded. Ready to process.");
    } else {
        QMessageBox::warning(this, "Error", "Failed to load audio file.");
    }
}

void MainWindow::onProcessSeparationClicked()
{
    if (m_currentSepFilePath.isEmpty() || m_audioData.empty()) {
        QMessageBox::warning(this, "Error", "Please load an audio file first.");
        return;
    }

    QString target = m_comboTarget->currentText();
    QString action = m_comboAction->currentText();


    QPushButton* btnProcessSep = qobject_cast<QPushButton*>(sender());
    if (btnProcessSep) btnProcessSep->setEnabled(false);


    QFuture<void> future = QtConcurrent::run([=]() {



        m_separatedAudioData.clear();
        m_separatedAudioData.resize(m_audioData.size(), 0.0f);

        if (target == "Bassline") {

            Biquad filter1, filter2, filter3, filter4;
            if (action == "Isolate") {
                filter1.setLPF(m_sampleRate, 200.0f, 0.5098f);
                filter2.setLPF(m_sampleRate, 200.0f, 0.6013f);
                filter3.setLPF(m_sampleRate, 200.0f, 0.8999f);
                filter4.setLPF(m_sampleRate, 200.0f, 2.5629f);
            } else {
                filter1.setHPF(m_sampleRate, 200.0f, 0.5098f);
                filter2.setHPF(m_sampleRate, 200.0f, 0.6013f);
                filter3.setHPF(m_sampleRate, 200.0f, 0.8999f);
                filter4.setHPF(m_sampleRate, 200.0f, 2.5629f);
            }

            for (size_t i = 0; i < m_audioData.size(); ++i) {
                float out = filter1.process(m_audioData[i]);
                out = filter2.process(out);
                out = filter3.process(out);
                out = filter4.process(out);


                m_separatedAudioData[i] = out;
            }
        }
        else if (target == "Vocals") {
            Biquad hp1, hp2, lp1, lp2;
            hp1.setHPF(m_sampleRate, 300.0f, 0.707f);
            hp2.setHPF(m_sampleRate, 300.0f, 0.707f);
            lp1.setLPF(m_sampleRate, 3400.0f, 0.707f);
            lp2.setLPF(m_sampleRate, 3400.0f, 0.707f);

            for (size_t i = 0; i < m_audioData.size(); ++i) {
                float out = hp1.process(m_audioData[i]);
                out = hp2.process(out);
                out = lp1.process(out);
                out = lp2.process(out);

                if (action == "Isolate") {
                    m_separatedAudioData[i] = out;
                } else {
                    m_separatedAudioData[i] = m_audioData[i] - out;
                }
            }
        } else {
            m_separatedAudioData = m_audioData;
        }


        int nSamples = m_separatedAudioData.size();
        const int fftSize = 2048;
        const int overlap = 1024;
        int timeSteps = (nSamples - fftSize) / (fftSize - overlap);
        int freqBins = fftSize / 2;

        m_mapAfter->data()->setSize(timeSteps, freqBins);
        m_mapAfter->data()->setRange(QCPRange(0, (double)nSamples/m_sampleRate), QCPRange(0, m_sampleRate/2));

        kiss_fft_cfg cfg = kiss_fft_alloc(fftSize, 0, NULL, NULL);
        kiss_fft_cpx in[fftSize], out[fftSize];

        for (int t = 0; t < timeSteps; ++t) {
            int startSample = t * (fftSize - overlap);
            for (int i = 0; i < fftSize; ++i) {
                float win = 0.5 * (1 - cos(2 * M_PI * i / (fftSize - 1)));
                in[i].r = m_separatedAudioData[startSample + i] * win;
                in[i].i = 0;
            }
            kiss_fft(cfg, in, out);
            for (int f = 0; f < freqBins; ++f) {
                double mag = sqrt(out[f].r * out[f].r + out[f].i * out[f].i);
                double decibels = 20 * log10(mag + 1e-6);
                m_mapAfter->data()->setCell(t, f, decibels);
            }
        }
        free(cfg);


        QMetaObject::invokeMethod(this, [=]() {



            m_mapAfter->rescaleDataRange(true);
            m_spectrogramAfter->xAxis->setRange(0, (double)nSamples/m_sampleRate);
            m_spectrogramAfter->yAxis->setRange(0, 5000);
            m_spectrogramAfter->replot();

            btnPlaySep->setEnabled(true);
            if (btnProcessSep) btnProcessSep->setEnabled(true);

            QMessageBox::information(this, "Done", QString("DSP Applied: %1 %2").arg(action, target));

        }, Qt::QueuedConnection);
    });
}

void MainWindow::onPlaySepClicked()
{
    if (m_separatedAudioData.empty()) return;


    m_sepPlayer->stop();
    m_sepPlayer->setSource(QUrl());

    QString tempWav = QDir::tempPath() + "/hga_temp_playback.wav";


    QFile::remove(tempWav);

    ma_encoder encoder;
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 1, m_sampleRate);
    if (ma_encoder_init_file(tempWav.toStdString().c_str(), &config, &encoder) == MA_SUCCESS) {
        ma_encoder_write_pcm_frames(&encoder, m_separatedAudioData.data(), m_separatedAudioData.size(), NULL);
        ma_encoder_uninit(&encoder);
    }

    m_sepPlayer->setSource(QUrl::fromLocalFile(tempWav));
    m_sepPlayer->play();

    m_sepPlaybackLine->setVisible(true);
    m_sepPlaybackTimer->start(50);
    btnStopSep->setEnabled(true);
}

void MainWindow::onStopSepClicked()
{
    m_sepPlayer->stop();
    m_sepPlaybackTimer->stop();
    m_sepPlaybackLine->setVisible(false);
    m_spectrogramAfter->replot();
    btnStopSep->setEnabled(false);
}

void MainWindow::updateSepPlaybackLine()
{
    if (m_sepPlayer->playbackState() == QMediaPlayer::PlayingState) {
        qint64 pos = m_sepPlayer->position();
        double seconds = pos / 1000.0;

        m_sepPlaybackLine->start->setCoords(seconds, 0);
        m_sepPlaybackLine->end->setCoords(seconds, m_sampleRate / 2.0); // Up to Nyquist
        m_spectrogramAfter->replot();
    } else {
        onStopSepClicked();
    }
}

void MainWindow::onSaveSepWavClicked()
{
    if (m_separatedAudioData.empty()) {
        QMessageBox::warning(this, "Error", "No processed audio to save. Please Process first.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Save Processed WAV", "", "WAV Files (*.wav)");
    if (savePath.isEmpty()) return;


    ma_encoder encoder;
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 1, m_sampleRate);
    if (ma_encoder_init_file(savePath.toStdString().c_str(), &config, &encoder) == MA_SUCCESS) {
        ma_encoder_write_pcm_frames(&encoder, m_separatedAudioData.data(), m_separatedAudioData.size(), NULL);
        ma_encoder_uninit(&encoder);
        QMessageBox::information(this, "Saved", "Successfully saved the separated WAV file.");
    } else {
        QMessageBox::warning(this, "Error", "Failed to save the WAV file.");
    }
}

void MainWindow::onLoadMmpClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open LMMS Project", "", "LMMS Project (*.mmp)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file.");
        return;
    }

    QDomDocument::ParseResult result = m_mmpDocument.setContent(&file);

    if (!result) {
        QMessageBox::warning(this, "Parse Error",
                             QString("XML Parse Error at line %1, col %2: %3")
                                 .arg(result.errorLine).arg(result.errorColumn).arg(result.errorMessage));
        file.close();
        return;
    }
    file.close();

    m_currentMmpPath = fileName;
    m_lblLoadedMmp->setText("Loaded: " + QFileInfo(fileName).fileName());

    parseMmpFile(fileName);
}

void MainWindow::parseMmpFile(const QString &/*filePath*/)
{
    QDomElement head = m_mmpDocument.documentElement().firstChildElement("head");
    if (!head.isNull() && head.hasAttribute("bpm")) {
        QString bpm = head.attribute("bpm");
        m_lblProjectStats->setText(QString("<b>Project Stats:</b> %1 BPM | 48 Ticks/Beat | 192 Ticks/Bar").arg(bpm));
    }
    m_parsedTracks.clear();
    m_comboTracks->clear();
    m_existingAutomations.clear();
    m_listAutomations->clear();
    m_txtAutomationInfo->clear();
    m_plotAutomation->clearGraphs();
    m_plotAutomation->replot();


    QHash<QString, QString> idMap;
    QDomNodeList allNodes = m_mmpDocument.elementsByTagName("*");
    for (int i = 0; i < allNodes.count(); ++i) {
        QDomElement elem = allNodes.at(i).toElement();
        if (elem.hasAttribute("id")) {
            idMap[elem.attribute("id")] = elem.tagName();
        }
    }


    QDomNodeList trackNodes = m_mmpDocument.documentElement().elementsByTagName("track");

    for (int i = 0; i < trackNodes.count(); ++i) {
        QDomElement trackElem = trackNodes.at(i).toElement();
        QString type = trackElem.attribute("type");

        if (type == "0" || type == "1") {
            QString trackName = trackElem.attribute("name");
            QDomNodeList instTracks = trackElem.elementsByTagName("instrumenttrack");
            if (instTracks.count() > 0) {
                QDomElement instTrackElem = instTracks.at(0).toElement();
                QDomElement targetElem = instTrackElem;
                QDomElement instContainer = instTrackElem.firstChildElement("instrument");
                if (!instContainer.isNull()) {
                    QDomElement pluginElem = instContainer.firstChildElement();
                    if (!pluginElem.isNull()) {
                        targetElem = pluginElem;
                        trackName += " (" + pluginElem.tagName() + ")";
                    }
                }
                ParsedTrack pt;
                pt.trackName = trackName;
                pt.trackElement = trackElem;
                pt.targetElement = targetElem;
                m_parsedTracks.push_back(pt);
                m_comboTracks->addItem(trackName);
            }
        }

        else if (type == "5") {
            QString trackName = trackElem.attribute("name");
            QDomNodeList patterns = trackElem.elementsByTagName("automationpattern");

            for (int j = 0; j < patterns.count(); ++j) {
                QDomElement patElem = patterns.at(j).toElement();
                ExistingAutomation ea;
                ea.trackName = trackName;
                ea.patternName = patElem.attribute("name");
                ea.lengthTicks = patElem.attribute("len").toInt();
                ea.prog = patElem.attribute("prog", "1").toInt();


                QDomNodeList times = patElem.elementsByTagName("time");
                for (int k = 0; k < times.count(); ++k) {
                    QDomElement tElem = times.at(k).toElement();
                    ea.tickX.append(tElem.attribute("pos").toDouble());
                    ea.valueY.append(tElem.attribute("value").toDouble());
                }


                QDomNodeList objects = patElem.elementsByTagName("object");
                for (int k = 0; k < objects.count(); ++k) {
                    QString objId = objects.at(k).toElement().attribute("id");
                    ea.targetObjectIds.append(objId);


                    QString targetName = idMap.value(objId, "Unlinked / Dead ID");
                    ea.resolvedTargets.append(targetName);
                }

                m_existingAutomations.push_back(ea);
                m_listAutomations->addItem(trackName + " | " + ea.patternName);
            }
        }
    }

    m_btnInjectMmp->setEnabled(m_parsedTracks.size() > 0);
}
void MainWindow::onInjectMmpClicked()
{

    if (m_editorX.isEmpty()) {
        QMessageBox::warning(this, "Empty ", "Please generate or copy a pattern in the Mega Editor first!");
        return;
    }

    int selectedIdx = m_comboTracks->currentIndex();
    if (selectedIdx < 0 || selectedIdx >= m_parsedTracks.size()) return;

    ParsedTrack &pt = m_parsedTracks[selectedIdx];


    QString paramRaw = m_comboTargetParam->currentText();
    QString paramKey = paramRaw.split(" ").first();


    int newId = QRandomGenerator::global()->bounded(1000000, 9999999);

    QDomElement nodeToMutate = pt.targetElement;
    if (paramKey == "vol" || paramKey == "pan") {
        nodeToMutate = pt.trackElement.firstChildElement("instrumenttrack");
    }


    QDomElement targetChild = nodeToMutate.firstChildElement(paramKey);

    if (!targetChild.isNull()) {

        targetChild.setAttribute("id", QString::number(newId));
    } else {
            QString existingVal = nodeToMutate.attribute(paramKey, QString::number(m_spinLfoBaseValue->value()));
        nodeToMutate.removeAttribute(paramKey);

        QDomElement newParamChild = m_mmpDocument.createElement(paramKey);
        newParamChild.setAttribute("id", QString::number(newId));
        newParamChild.setAttribute("value", existingVal);
        if (pt.targetElement.tagName() == "xpressive") newParamChild.setAttribute("scale_type", "linear");

        nodeToMutate.appendChild(newParamChild);
    }


    QDomElement trackContainer = m_mmpDocument.documentElement().firstChildElement("song").firstChildElement("trackcontainer");

    QDomElement autoTrack = m_mmpDocument.createElement("track");
    autoTrack.setAttribute("type", "5");
    autoTrack.setAttribute("name", "Injected Macro -> " + paramKey);
    autoTrack.setAttribute("muted", "0");
    autoTrack.setAttribute("solo", "0");

    QDomElement autoTrackInner = m_mmpDocument.createElement("automationtrack");
    autoTrack.appendChild(autoTrackInner);

    int lenTicks = m_spinLfoLengthTicks->value();

    QDomElement autoPattern = m_mmpDocument.createElement("automationpattern");
    autoPattern.setAttribute("name", pt.trackName + " > " + paramKey);
    autoPattern.setAttribute("pos", "0");
    autoPattern.setAttribute("len", QString::number(lenTicks));
    autoPattern.setAttribute("prog", QString::number(m_comboInterpolation->currentIndex()));
    autoPattern.setAttribute("tens", "1");
    autoPattern.setAttribute("mute", "0");


    for (int i = 0; i < m_editorX.size(); ++i) {
        QDomElement timeElem = m_mmpDocument.createElement("time");
        timeElem.setAttribute("pos", QString::number(m_editorX[i], 'f', 0));


        QString valString = QString::number(m_editorY[i], 'f', 5);


        timeElem.setAttribute("value", valString);
        timeElem.setAttribute("outValue", valString);

        autoPattern.appendChild(timeElem);
    }


    QDomElement objLink = m_mmpDocument.createElement("object");
    objLink.setAttribute("id", QString::number(newId));
    autoPattern.appendChild(objLink);

    autoTrack.appendChild(autoPattern);
    trackContainer.appendChild(autoTrack);

    QString savePath = QFileDialog::getSaveFileName(this, "Save Injected Project", m_currentMmpPath.replace(".mmp", "_Macro.mmp"), "LMMS Project (*.mmp)");
    if (savePath.isEmpty()) return;

    QFile outFile(savePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save the new file.");
        return;
    }

    QTextStream outStream(&outFile);
    m_mmpDocument.save(outStream, 2);
    outFile.close();

    QMessageBox::information(this, "Success", "Automation injected!\nLoad the new .mmp in LMMS to see your algorithmic macro.");
}

void MainWindow::onExistingAutomationSelected(int currentRow)
{
    if (currentRow < 0 || currentRow >= m_existingAutomations.size()) return;

    ExistingAutomation& ea = m_existingAutomations[currentRow];


    double bars = ea.lengthTicks / 192.0; // 192 ticks per standard 4/4 bar

    QString progString;
    switch(ea.prog) {
    case 0: progString = "Discrete / Step (Instant changes)"; break;
    case 1: progString = "Linear (Straight lines)"; break;
    case 2: progString = "Cubic Hermite (Smooth curves)"; break;
    default: progString = "Unknown"; break;
    }

    QString infoHtml = "<h3>Automation Details</h3>";
    infoHtml += "<b>Parent Track:</b> " + ea.trackName + "<br/>";
    infoHtml += "<b>Pattern Name:</b> " + ea.patternName + "<br/>";
    infoHtml += "<b>Duration:</b> " + QString::number(ea.lengthTicks) + " Ticks (<b>" + QString::number(bars, 'f', 2) + " Bars</b>)<br/>";
    infoHtml += "<b>Interpolation:</b> " + progString + "<br/>";
    infoHtml += "<b>Data Points:</b> " + QString::number(ea.tickX.size()) + "<br/><br/>";

    infoHtml += "<b>Linked Parameters:</b><ul>";
    if (ea.resolvedTargets.isEmpty()) {
        infoHtml += "<li><i>None</i></li>";
    } else {
        for (int i = 0; i < ea.resolvedTargets.size(); ++i) {
            infoHtml += "<li><b>" + ea.resolvedTargets[i] + "</b> (ID: " + ea.targetObjectIds[i] + ")</li>";
        }
    }
    infoHtml += "</ul>";

    m_txtAutomationInfo->setHtml(infoHtml);


    m_plotAutomation->clearGraphs();
    m_plotAutomation->addGraph();
    m_plotAutomation->graph(0)->setData(ea.tickX, ea.valueY);
    m_plotAutomation->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));

    if (ea.prog == 0) {
        m_plotAutomation->graph(0)->setLineStyle(QCPGraph::lsStepLeft);
        m_plotAutomation->graph(0)->setPen(QPen(Qt::red, 2));
    } else {
        m_plotAutomation->graph(0)->setLineStyle(QCPGraph::lsLine);
        m_plotAutomation->graph(0)->setPen(QPen(Qt::cyan, 2));
    }

    double maxX = ea.lengthTicks > 0 ? ea.lengthTicks : 192;
    if (!ea.tickX.isEmpty() && ea.tickX.last() > maxX) maxX = ea.tickX.last();

    m_plotAutomation->xAxis->setRange(0, maxX + 48);
    m_plotAutomation->graph(0)->rescaleValueAxis(false, true);

    QCPRange yRange = m_plotAutomation->yAxis->range();
    double padding = yRange.size() * 0.1;
    if (padding == 0) padding = 0.5; // Catch flatlines
    m_plotAutomation->yAxis->setRange(yRange.lower - padding, yRange.upper + padding);

    m_plotAutomation->replot();


    m_btnCopyToEditor->setEnabled(true);
}


void MainWindow::onTrackSelectionChanged(int index)
{
    m_comboTargetParam->clear();
    if (index < 0 || index >= m_parsedTracks.size()) return;

    ParsedTrack &pt = m_parsedTracks[index];


    m_comboTargetParam->addItem("vol (Base Volume)");
    m_comboTargetParam->addItem("pan (Base Panning)");


    QDomNamedNodeMap attributes = pt.targetElement.attributes();
    for (int i = 0; i < attributes.count(); ++i) {
        QString attrName = attributes.item(i).nodeName();

        if (attrName != "name" && attrName != "id" && !attrName.contains("sample") && attrName != "interpolateW3") {
            m_comboTargetParam->addItem(attrName);
        }
    }
}


void MainWindow::onEditorLengthChanged(int ticks)
{
    double bars = ticks / 768.0;
    m_lblEditorDurationBars->setText(QString("Duration: %1 Bars").arg(bars, 0, 'f', 2));
    updateEditorPlot();
}


void MainWindow::onCopyToEditorClicked()
{
    int currentRow = m_listAutomations->currentRow();
    if (currentRow < 0 || currentRow >= m_existingAutomations.size()) return;

    ExistingAutomation& ea = m_existingAutomations[currentRow];

    m_editorX = ea.tickX;
    m_editorY = ea.valueY;

    m_spinLfoLengthTicks->setValue(ea.lengthTicks);
    m_comboInterpolation->setCurrentIndex(ea.prog);

    updateEditorPlot();
    QMessageBox::information(this, "Copied", "Pattern copied to the Editor!");
}

void MainWindow::onReverseEditorClicked()
{
    if (m_editorX.isEmpty()) return;

    QVector<double> newY;

    for (int i = m_editorY.size() - 1; i >= 0; --i) {
        newY.append(m_editorY[i]);
    }
    m_editorY = newY;
    updateEditorPlot();
}

void MainWindow::onClearEditorClicked()
{
    m_editorX.clear();
    m_editorY.clear();
    updateEditorPlot();
}


void MainWindow::onGenerateLfoClicked()
{
    int blendMode = m_comboBlendMode->currentIndex();
    int points = m_spinDataPoints->value();
    if (points < 2) points = 2;

    bool isReplacing = (blendMode == 0 || m_editorX.isEmpty() || m_editorX.size() != points);

    if (isReplacing) {
        m_editorX.clear();
        m_editorY.clear();
    }

    int lenTicks = m_spinLfoLengthTicks->value();

    double rateStart = m_spinLfoFreqStart->value();
    double rateEnd = m_spinLfoFreqEnd->value();
    double depthStart = m_spinLfoDepthStart->value();
    double depthEnd = m_spinLfoDepthEnd->value();

    double freqStart = 1.0 / rateStart;
    double freqEnd = 1.0 / rateEnd;

    double currentPhase = m_spinLfoPhase->value() / 360.0;
    double baseVal = m_spinLfoBaseValue->value();

    double swingAmt = m_spinSwing->value() / 100.0;
    double swingMid = 0.5 + (swingAmt * 0.25);
    double last_virtual_t = 0.0;

    int macroType = m_comboMacroType->currentIndex();
    int waveType = m_comboWaveform->currentIndex();

    double stepSize = (double)lenTicks / (points - 1);

    for (int i = 0; i < points; ++i) {
        double t = i * stepSize;

        double eighthNoteTicks = 24.0;
        double cyclePos = std::fmod(t, eighthNoteTicks) / eighthNoteTicks;
        double virtualCyclePos = 0.0;

        if (cyclePos < swingMid) {
            virtualCyclePos = 0.5 * (cyclePos / swingMid);
        } else {
            virtualCyclePos = 0.5 + 0.5 * ((cyclePos - swingMid) / (1.0 - swingMid));
        }

        double virtual_t = std::floor(t / eighthNoteTicks) * eighthNoteTicks + (virtualCyclePos * eighthNoteTicks);

        double t_norm = (lenTicks > 0) ? virtual_t / lenTicks : 0.0;
        double val = 0.0;

        double currentDepth = depthStart + (depthEnd - depthStart) * t_norm;
        double currentFreq = freqStart + (freqEnd - freqStart) * t_norm;

        if (i > 0) {
            double delta_v_t = virtual_t - last_virtual_t;
            currentPhase += currentFreq * delta_v_t;
        }
        last_virtual_t = virtual_t;

        if (macroType == 0) { // LFO
            double p_mod = currentPhase - std::floor(currentPhase);
            if (waveType == 0) val = std::sin(p_mod * 2.0 * M_PI);
            else if (waveType == 1) val = (p_mod < 0.5) ? 1.0 : -1.0;
            else if (waveType == 2) val = 4.0 * std::abs(p_mod - 0.5) - 1.0;
            else if (waveType == 3) val = 1.0 - 2.0 * p_mod;
            else if (waveType == 4) val = 2.0 * p_mod - 1.0;
            val = baseVal + (val * currentDepth);
        }
        else if (macroType == 1) { // ADSR
            double attT = lenTicks * 0.20;
            double decT = lenTicks * 0.20;
            double relT = lenTicks * 0.20;
            double susT = lenTicks - attT - decT - relT;
            double susLvl = 0.5;

            if (virtual_t < attT) val = virtual_t / attT;
            else if (virtual_t < attT + decT) val = 1.0 - ((virtual_t - attT) / decT) * (1.0 - susLvl);
            else if (virtual_t < attT + decT + susT) val = susLvl;
            else val = susLvl * (1.0 - ((virtual_t - (attT + decT + susT)) / relT));

            val = baseVal + (val * currentDepth * 2.0 - currentDepth);
        }
        else if (macroType == 2) {
            double ticksPerHold = currentFreq > 0 ? (1.0 / currentFreq) : 1.0;
            int holdIndex = (int)(virtual_t / ticksPerHold);
            QRandomGenerator rand(holdIndex + 9999);
            val = baseVal + ((rand.generateDouble() * 2.0 - 1.0) * currentDepth);
        }
        else if (macroType == 3) {
            int sixteenth = (int)(virtual_t / 12.0);
            val = baseVal + (((sixteenth % 2 == 0) ? 1.0 : -1.0) * currentDepth);
        }
        else if (macroType == 4) {
            double cyclePhase = std::fmod(virtual_t, rateStart) / rateStart;
            double duckCurve = std::pow(cyclePhase, 0.4);
            val = baseVal - currentDepth + (currentDepth * duckCurve);
        }
        else if (macroType == 5) {
            double drop = std::pow(t_norm, 3.0);
            val = baseVal - (drop * currentDepth);
        }


        if (isReplacing) {
            m_editorX.append(t);
            m_editorY.append(val);
        } else {
            if (blendMode == 1) m_editorY[i] += val;
            else if (blendMode == 2) m_editorY[i] -= val;
            else if (blendMode == 3) m_editorY[i] *= (val / 100.0);
        }
    }

    if ((macroType == 0 && waveType == 1) || macroType == 3) {
        m_comboInterpolation->setCurrentIndex(0);
    } else if (macroType == 4 || macroType == 5) {
        m_comboInterpolation->setCurrentIndex(2);
    }

    updateEditorPlot();
}

void MainWindow::updateEditorPlot()
{
    m_plotEditor->clearGraphs();
    if (m_editorX.isEmpty()) {
        m_plotEditor->replot();
        m_btnInjectMmp->setEnabled(false);
        return;
    }

    m_plotEditor->addGraph();
    m_plotEditor->graph(0)->setData(m_editorX, m_editorY);
    m_plotEditor->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));

    int prog = m_comboInterpolation->currentIndex();
    if (prog == 0) {
        m_plotEditor->graph(0)->setLineStyle(QCPGraph::lsStepLeft);
        m_plotEditor->graph(0)->setPen(QPen(Qt::red, 3));
    } else {
        m_plotEditor->graph(0)->setLineStyle(QCPGraph::lsLine);
        m_plotEditor->graph(0)->setPen(QPen(Qt::magenta, 3));
    }

    m_plotEditor->xAxis->setRange(0, m_spinLfoLengthTicks->value() + 48);
    m_plotEditor->graph(0)->rescaleValueAxis(false, true);

    QCPRange yRange = m_plotEditor->yAxis->range();
    double padding = yRange.size() * 0.2;
    if (padding == 0) padding = 0.5;
    m_plotEditor->yAxis->setRange(yRange.lower - padding, yRange.upper + padding);

    m_plotEditor->replot();
    m_btnInjectMmp->setEnabled(m_parsedTracks.size() > 0);
}void MainWindow::onEditorMousePress(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;


    m_draggedPointIndex = -1;
    double minDistance = 10.0;

    for (int i = 0; i < m_editorX.size(); ++i) {
        double px = m_plotEditor->xAxis->coordToPixel(m_editorX[i]);
        double py = m_plotEditor->yAxis->coordToPixel(m_editorY[i]);
        double dist = std::hypot(event->pos().x() - px, event->pos().y() - py);

        if (dist < minDistance) {
            minDistance = dist;
            m_draggedPointIndex = i;
        }
    }


    if (m_draggedPointIndex != -1) {
        m_plotEditor->setInteraction(QCP::iRangeDrag, false);
    }
}
void MainWindow::onEditorMouseMove(QMouseEvent *event) {
    if (m_draggedPointIndex == -1) return;

    double newX = m_plotEditor->xAxis->pixelToCoord(event->pos().x());
    double newY = m_plotEditor->yAxis->pixelToCoord(event->pos().y());


    if (m_checkSnapGrid->isChecked()) {
        double snapInterval = 12.0; // Default 1/16th
        int qIndex = m_comboQuantizeX->currentIndex();
        if (qIndex == 0) snapInterval = 48.0; // 1/4
        else if (qIndex == 1) snapInterval = 24.0; // 1/8
        else if (qIndex == 2) snapInterval = 12.0; // 1/16
        else if (qIndex == 3) snapInterval = 6.0;  // 1/32

        newX = std::round(newX / snapInterval) * snapInterval;
    }

    if (m_draggedPointIndex > 0) {
        newX = std::max(newX, m_editorX[m_draggedPointIndex - 1] + 1.0);
    } else {
        newX = std::max(newX, 0.0);
    }

    if (m_draggedPointIndex < m_editorX.size() - 1) {
        newX = std::min(newX, m_editorX[m_draggedPointIndex + 1] - 1.0);
    }

    m_editorX[m_draggedPointIndex] = newX;
    m_editorY[m_draggedPointIndex] = newY;

    updateEditorPlot();
}



void MainWindow::onEditorMouseRelease(QMouseEvent *event) {
    Q_UNUSED(event);
    m_draggedPointIndex = -1;
    m_plotEditor->setInteraction(QCP::iRangeDrag, true);
}

void MainWindow::onEditorMouseDoubleClick(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    double x = m_plotEditor->xAxis->pixelToCoord(event->pos().x());
    double y = m_plotEditor->yAxis->pixelToCoord(event->pos().y());


    for (int i = 0; i < m_editorX.size(); ++i) {
        double px = m_plotEditor->xAxis->coordToPixel(m_editorX[i]);
        double py = m_plotEditor->yAxis->coordToPixel(m_editorY[i]);
        if (std::hypot(event->pos().x() - px, event->pos().y() - py) < 10.0) {
            m_editorX.remove(i);
            m_editorY.remove(i);
            updateEditorPlot();
            return;
        }
    }


    int insertIndex = 0;
    while (insertIndex < m_editorX.size() && m_editorX[insertIndex] < x) {
        insertIndex++;
    }

    m_editorX.insert(insertIndex, x);
    m_editorY.insert(insertIndex, y);
    updateEditorPlot();
}

void MainWindow::onSaveShapeClicked() {
    if (m_editorX.isEmpty()) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Save Automation Shape", "", "XML Pattern Automation (*.xpa)");
    if (fileName.isEmpty()) return;

    QDomDocument doc;
    QDomElement root = doc.createElement("automation_shape");
    root.setAttribute("prog", m_comboInterpolation->currentIndex());
    root.setAttribute("len", m_spinLfoLengthTicks->value());
    doc.appendChild(root);

    for (int i = 0; i < m_editorX.size(); ++i) {
        QDomElement point = doc.createElement("time");
        point.setAttribute("pos", QString::number(m_editorX[i], 'f', 1));
        point.setAttribute("value", QString::number(m_editorY[i], 'f', 5));
        root.appendChild(point);
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        doc.save(out, 4);
        file.close();
        QMessageBox::information(this, "Saved", "Automation shape saved successfully.");
    }
}

void MainWindow::onLoadShapeClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load Automation Shape", "", "XML Pattern Automation (*.xpa)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QDomDocument doc;
    if (!doc.setContent(&file)) { file.close(); return; }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != "automation_shape") return;

    m_editorX.clear();
    m_editorY.clear();

    m_comboInterpolation->setCurrentIndex(root.attribute("prog", "2").toInt());
    m_spinLfoLengthTicks->setValue(root.attribute("len", "768").toInt());

    QDomNodeList points = root.elementsByTagName("time");
    for (int i = 0; i < points.count(); ++i) {
        QDomElement point = points.at(i).toElement();
        m_editorX.append(point.attribute("pos").toDouble());
        m_editorY.append(point.attribute("value").toDouble());
    }

    updateEditorPlot();
}

void MainWindow::onInvertClicked() {
    if (m_editorY.isEmpty()) return;
    double baseVal = m_spinLfoBaseValue->value();
    for (int i = 0; i < m_editorY.size(); ++i) {

        m_editorY[i] = baseVal - (m_editorY[i] - baseVal);
    }
    updateEditorPlot();
}

void MainWindow::onSmoothClicked() {

    if (m_editorY.size() < 3) return;
    QVector<double> newY = m_editorY;
    for (int i = 1; i < m_editorY.size() - 1; ++i) {
        newY[i] = (m_editorY[i-1] + m_editorY[i] + m_editorY[i+1]) / 3.0;
    }
    m_editorY = newY;
    updateEditorPlot();
}

void MainWindow::onHumanizeClicked() {
    if (m_editorY.isEmpty()) return;
        double maxJitter = m_spinLfoDepthStart->value() * 0.1;

    for (int i = 1; i < m_editorY.size() - 1; ++i) {
        double jitterY = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * maxJitter;
        m_editorY[i] += jitterY;


        double spaceLeft = m_editorX[i] - m_editorX[i-1];
        double spaceRight = m_editorX[i+1] - m_editorX[i];
        double maxJitterX = std::min(spaceLeft, spaceRight) * 0.2;
        double jitterX = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * maxJitterX;
        m_editorX[i] += jitterX;
    }
    updateEditorPlot();
}

void MainWindow::onQuantizeYClicked() {

    if (m_editorY.isEmpty()) return;
    double steps = 12.0;
    double range = m_spinLfoDepthStart->value() * 2.0;
    double stepSize = range / steps;
    if (stepSize <= 0) return;

    for (int i = 0; i < m_editorY.size(); ++i) {
        m_editorY[i] = std::round(m_editorY[i] / stepSize) * stepSize;
    }
    m_comboInterpolation->setCurrentIndex(0);
    updateEditorPlot();
}

void MainWindow::onScaleYAmplitudeClicked() {
    if (m_editorY.isEmpty()) return;


    bool ok;
    double scale = QInputDialog::getDouble(this, "Scale Y Amplitude",
                                           "Enter multiplier (e.g., 0.5 to halve, 2.0 to double):\nNote: It scales around your 'Base' value.",
                                           1.0, -100.0, 100.0, 3, &ok);
    if (!ok) return;

    double baseVal = m_spinLfoBaseValue->value();

    for (int i = 0; i < m_editorY.size(); ++i) {

        m_editorY[i] = baseVal + ((m_editorY[i] - baseVal) * scale);
    }

    updateEditorPlot();
}

void MainWindow::onLoadXptAsCvClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load LMMS Pattern as CV", "", "LMMS Pattern (*.xpt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QDomDocument doc;
    if (!doc.setContent(&file)) { file.close(); return; }
    file.close();

    struct XptNote { int key; double pos; double len; };
    std::vector<XptNote> parsedNotes;

    QDomNodeList notes = doc.elementsByTagName("note");
    for (int i = 0; i < notes.count(); ++i) {
        QDomElement nElem = notes.at(i).toElement();
        XptNote n;
        n.key = nElem.attribute("key").toInt();
        n.pos = nElem.attribute("pos").toDouble();
        n.len = nElem.attribute("len").toDouble();
        parsedNotes.push_back(n);
    }

    std::sort(parsedNotes.begin(), parsedNotes.end(), [](const XptNote& a, const XptNote& b) {
        if (a.pos == b.pos) return a.key > b.key;
        return a.pos < b.pos;
    });

    m_editorX.clear();
    m_editorY.clear();

    double baseVal = m_spinLfoBaseValue->value();
    double scalePerSemi = m_spinLfoDepthStart->value() * 0.01;
    double currentX = 0;
    double maxPos = 0;

    for (const auto& n : parsedNotes) {
        if (n.pos < currentX) continue;

        double yVal = baseVal + ((n.key - 60) * scalePerSemi);

        m_editorX.append(n.pos);
        m_editorY.append(yVal);

        m_editorX.append(n.pos + n.len - 1.0);
        m_editorY.append(yVal);

        currentX = n.pos + n.len;
        if (currentX > maxPos) maxPos = currentX;
    }

    m_comboInterpolation->setCurrentIndex(0);

    if (maxPos > m_spinLfoLengthTicks->value()) {
        m_spinLfoLengthTicks->setValue(maxPos);
    }

    updateEditorPlot();
    QMessageBox::information(this, "Success", "Notes converted to CV!\n\nTip: Use the 'Scale Y Amplitude' button to adjust the voltage range.");
}
void MainWindow::onExtractEnvelopeClicked()
{

    if (m_audioData.empty()) {
        QMessageBox::warning(this, "No Audio", "Please load an audio file in Tab 1 first!");
        return;
    }


    double audioLengthSecs = (double)m_audioData.size() / m_sampleRate;
    double ticksPerSecond = (m_bpm / 60.0) * 48.0;
    double totalTicks = audioLengthSecs * ticksPerSecond;


    m_spinLfoLengthTicks->setValue(qRound(totalTicks));

    m_editorX.clear();
    m_editorY.clear();


    int points = m_spinDataPoints->value();
    if (points < 2) points = 2;

    double baseVal = m_spinLfoBaseValue->value();
    double depthStart = m_spinLfoDepthStart->value();
    double depthEnd = m_spinLfoDepthEnd->value();

    int samplesPerPoint = m_audioData.size() / points;


    std::vector<double> rawEnvelopes;
    double maxRms = 0.00001;

    for (int i = 0; i < points; ++i) {
        int startSample = i * samplesPerPoint;
        int endSample = std::min(startSample + samplesPerPoint, (int)m_audioData.size());

        double sumSq = 0;
        int count = endSample - startSample;
        for (int j = startSample; j < endSample; ++j) {
            sumSq += m_audioData[j] * m_audioData[j];
        }

        double rms = std::sqrt(sumSq / count);
        rawEnvelopes.push_back(rms);

        if (rms > maxRms) maxRms = rms;
    }


    double stepTicks = totalTicks / (points - 1);
    double tension = m_spinTension->value();

    for (int i = 0; i < points; ++i) {
        double t = i * stepTicks;
        double t_norm = (points > 1) ? (double)i / (points - 1) : 0.0;

        double currentDepth = depthStart + (depthEnd - depthStart) * t_norm;

        double normalizedVol = rawEnvelopes[i] / maxRms;


        normalizedVol = std::pow(normalizedVol, tension);


        double val = baseVal + (normalizedVol * currentDepth);

        m_editorX.append(t);
        m_editorY.append(val);
    }


    m_comboInterpolation->setCurrentIndex(2);

    updateEditorPlot();
}

void MainWindow::onLoadModGrooveClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Amiga .mod", "", "Tracker Modules (*.mod)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Could not open .mod file.");
        return;
    }


    m_currentModData = file.readAll();
    file.close();

    processModData();
}

void MainWindow::processModData()
{
    if (m_currentModData.isEmpty() || m_currentModData.size() < 1084) return;

    QString sig = QString::fromLatin1(m_currentModData.mid(1080, 4));
    int channels = 4;
    if (sig == "6CHN") channels = 6;
    else if (sig == "8CHN" || sig == "OCTA" || sig == "CD81") channels = 8;

    QStringList instrumentNames;
    instrumentNames.append("None");
    for (int i = 0; i < 31; ++i) {
        int offset = 20 + (i * 30);
        QByteArray nameBytes = m_currentModData.mid(offset, 22);

        int nullIdx = nameBytes.indexOf('\0');
        if (nullIdx != -1) nameBytes.truncate(nullIdx);

        QString name = QString::fromLatin1(nameBytes).trimmed();
        if (name.isEmpty()) name = QString("Inst %1").arg(i + 1);
        instrumentNames.append(name);
    }

    int targetPattern = m_spinStartPattern->value();
    int targetChannel = m_comboModChannel->currentIndex() - 1;

    m_bbTable->setRowCount(0);
    m_bbTable->setColumnCount(64);


    int patternStartOffset = 1084 + (targetPattern * 64 * channels * 4);
    if (patternStartOffset + (64 * channels * 4) > m_currentModData.size()) return;

    QMap<int, int> instToTableRow;

    for (int row = 0; row < 64; ++row) {
        for (int ch = 0; ch < channels; ++ch) {
            if (targetChannel != -1 && ch != targetChannel) continue;

            int byteOffset = patternStartOffset + (row * channels * 4) + (ch * 4);
            unsigned char b0 = m_currentModData[byteOffset];
            unsigned char b1 = m_currentModData[byteOffset + 1];
            unsigned char b2 = m_currentModData[byteOffset + 2];
            unsigned char b3 = m_currentModData[byteOffset + 3];
            (void)b3;
            int inst = (b0 & 0xF0) | ((b2 & 0xF0) >> 4);
            int period = ((b0 & 0x0F) << 8) | b1;
            int effect = b2 & 0x0F;

            if (inst > 0 || period > 0) {
                int activeInst = (inst > 0) ? inst : 999;

                if (!instToTableRow.contains(activeInst)) {
                    int newRow = m_bbTable->rowCount();
                    m_bbTable->insertRow(newRow);
                    QString rowLabel = (activeInst == 999) ? "Ghost Note (No Inst)" : QString("%1: %2").arg(activeInst).arg(instrumentNames[activeInst]);
                    m_bbTable->setVerticalHeaderItem(newRow, new QTableWidgetItem(rowLabel));
                    instToTableRow[activeInst] = newRow;
                }

                int tableRow = instToTableRow[activeInst];
                QTableWidgetItem *item = m_bbTable->item(tableRow, row);
                if (!item) {
                    item = new QTableWidgetItem();
                    item->setTextAlignment(Qt::AlignCenter);
                    m_bbTable->setItem(tableRow, row, item);
                }

                QString cellText = "X";
                if (period > 0) cellText = "Hit";
                if (effect > 0) cellText += QString("\nFX:%1").arg(effect, 1, 16);

                item->setText(cellText);
                item->setBackground(QBrush(QColor(46, 139, 87, 150)));
            }
        }
    }

    QList<QPushButton*> buttons = m_tabGroove->findChildren<QPushButton*>();
    for (QPushButton* btn : std::as_const(buttons)) {
        if (btn->text().contains("NEW .mmp")) btn->setEnabled(true);
    }
}

void MainWindow::onLoadMmpGrooveClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open LMMS Project", "", "LMMS Project (*.mmp)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        QMessageBox::warning(this, "Parse Error", "Invalid XML in .mmp file.");
        return;
    }
    file.close();

    QDomNodeList trackContainers = doc.elementsByTagName("trackcontainer");
    QDomElement bbContainer;

    for (int i = 0; i < trackContainers.count(); ++i) {
        if (trackContainers.at(i).toElement().attribute("type") == "bbtrackcontainer") {
            bbContainer = trackContainers.at(i).toElement();
            break;
        }
    }

    if (bbContainer.isNull()) {
        QMessageBox::warning(this, "Not Found", "No Beat/Bassline data found in this project.");
        return;
    }

    m_bbTable->setRowCount(0);
    int maxCols = 16;
    m_bbTable->setColumnCount(maxCols);

    QDomNodeList tracks = bbContainer.elementsByTagName("track");

    if (tracks.isEmpty()) {
        QMessageBox::information(this, "Empty Container", "The Beat/Bassline container exists, but it has no instruments inside it!");
        return;
    }

    for (int i = 0; i < tracks.count(); ++i) {
        QDomElement track = tracks.at(i).toElement();
        QString trackName = track.attribute("name", "Unknown Track");

        int newRow = m_bbTable->rowCount();
        m_bbTable->insertRow(newRow);
        m_bbTable->setVerticalHeaderItem(newRow, new QTableWidgetItem(trackName));

        QDomNodeList patterns = track.elementsByTagName("pattern");
        for (int p = 0; p < patterns.count(); ++p) {
            QDomElement pattern = patterns.at(p).toElement();
            int steps = pattern.attribute("steps", "16").toInt();

            if (steps > maxCols) {
                maxCols = steps;
                m_bbTable->setColumnCount(maxCols);
            }

            QDomNodeList notes = pattern.elementsByTagName("note");
            for (int n = 0; n < notes.count(); ++n) {
                QDomElement note = notes.at(n).toElement();
                int pos = note.attribute("pos").toInt();
                int vol = note.attribute("vol", "100").toInt();

                 int col = pos / 12;

                if (col >= 0 && col < maxCols) {
                    QTableWidgetItem *item = m_bbTable->item(newRow, col);
                    if (!item) {
                        item = new QTableWidgetItem();
                        item->setTextAlignment(Qt::AlignCenter);
                        m_bbTable->setItem(newRow, col, item);
                    }
                    item->setText(QString("Vol:%1").arg(vol));
                    item->setBackground(QBrush(QColor(0, 150, 255, 150)));
                }
            }
        }
    }

    QList<QPushButton*> buttons = m_tabGroove->findChildren<QPushButton*>();
    for (QPushButton* btn : buttons) {
        if (btn->text().contains("NEW .mmp")) btn->setEnabled(true);
    }

    QMessageBox::information(this, "Success", "Beat/Bassline tracks extracted from .mmp project!");
}



void MainWindow::onExportNewMmpClicked()
{
    if (m_bbTable->rowCount() == 0) {
        QMessageBox::warning(this, "Empty", "No pattern data to export! Load a .mod file first.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Save LMMS Project", "Amiga_Groove.mmp", "LMMS Project (*.mmp)");
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not write to file.");
        return;
    }

    QTextStream out(&file);


    out << "<?xml version=\"1.0\"?>\n";
    out << "<!DOCTYPE lmms-project>\n";
    out << "<lmms-project version=\"20\" type=\"song\">\n";
    out << "  <head bpm=\"125\" mastervol=\"100\" timesig_numerator=\"4\" timesig_denominator=\"4\"/>\n";
    out << "  <song>\n";


    out << "    <trackcontainer type=\"song\">\n";
    out << "      <track type=\"1\" name=\"Amiga Imported Pattern\">\n";
    out << "        <bbtrack>\n";
    out << "          <trackcontainer type=\"bbtrackcontainer\">\n";


    int numCols = m_bbTable->columnCount();
    for (int row = 0; row < m_bbTable->rowCount(); ++row) {


        QString trackName = m_bbTable->verticalHeaderItem(row)->text().toHtmlEscaped();


        out << "            <track type=\"0\" name=\"" << trackName << "\">\n";
        out << "              <instrumenttrack vol=\"100\" pan=\"0\" basenote=\"69\">\n";
        out << "                <instrument name=\"kicker\">\n";
        out << "                  <kicker startfreq=\"150\" endfreq=\"40\" decay=\"440\" dist=\"0.8\" click=\"0.4\">\n";
        out << "                    <key/>\n";
        out << "                  </kicker>\n";
        out << "                </instrument>\n";
        out << "              </instrumenttrack>\n";


        out << "              <pattern type=\"0\" pos=\"0\" steps=\"" << numCols << "\">\n";


        for (int col = 0; col < numCols; ++col) {
            QTableWidgetItem *item = m_bbTable->item(row, col);
            if (item && !item->text().isEmpty()) {


                int lmmsPos = col * 12;

                out << "                <note pos=\"" << lmmsPos << "\" key=\"69\" vol=\"100\" len=\"-192\" pan=\"0\"/>\n";
            }
        }

        out << "              </pattern>\n";
        out << "            </track>\n";
    }

    out << "          </trackcontainer>\n";
    out << "        </bbtrack>\n";


    out << "        <pattern type=\"1\" pos=\"0\" steps=\"" << numCols << "\" muted=\"0\" name=\"Extracted Groove\"/>\n";

    out << "      </track>\n";
    out << "    </trackcontainer>\n";
    out << "  </song>\n";
    out << "</lmms-project>\n";

    file.close();
    QMessageBox::information(this, "Success", "Groove exported! Open this .mmp in LMMS to see your tracks (with the blue box!).");
}

void MainWindow::onLoadMidiGrooveClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open MIDI File", "", "MIDI Files (*.mid *.midi)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.readAll();
    file.close();

    int pos = 0;

    auto readBe16 = [&](int& p) -> uint16_t {
        if (p + 2 > data.size()) return 0;
        uint16_t v = (uint8_t)data[p]<<8 | (uint8_t)data[p+1]; p += 2; return v;
    };
    auto readBe32 = [&](int& p) -> uint32_t {
        if (p + 4 > data.size()) return 0;
        uint32_t v = (uint8_t)data[p]<<24 | (uint8_t)data[p+1]<<16 | (uint8_t)data[p+2]<<8 | (uint8_t)data[p+3]; p += 4; return v;
    };
    auto readVLQ = [&](int& p) -> uint32_t {
        uint32_t v = 0;
        while (p < data.size()) {
            uint8_t b = data[p++];
            v = (v << 7) | (b & 0x7F);
            if (!(b & 0x80)) break;
        }
        return v;
    };


    if (data.mid(0, 4) != "MThd") {
        QMessageBox::warning(this, "Error", "Not a valid MIDI file.");
        return;
    }
    pos += 4;
    uint32_t headerLen = readBe32(pos);
    int format = readBe16(pos);
    int numTracks = readBe16(pos);
    int ppq = readBe16(pos); // Ticks per quarter note

    struct MidiHit { int tick; int note; int vel; };
    QList<MidiHit> allHits;


    for (int i = 0; i < numTracks; ++i) {
        if (pos + 8 > data.size()) break;
        if (data.mid(pos, 4) != "MTrk") {
            pos += 4; uint32_t len = readBe32(pos); pos += len; continue;
        }
        pos += 4;
        uint32_t trackLen = readBe32(pos);
        int trackEnd = pos + trackLen;

        int absTick = 0;
        uint8_t lastStatus = 0;

        while (pos < trackEnd && pos < data.size()) {
            absTick += readVLQ(pos);
            uint8_t status = data[pos];

            if (status < 0x80) { status = lastStatus; }
            else { pos++; lastStatus = status; }

            if (status == 0xFF) {
                pos++; uint32_t len = readVLQ(pos); pos += len;
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t len = readVLQ(pos); pos += len;
            } else {
                uint8_t type = status & 0xF0;
                uint8_t channel = status & 0x0F;

                if (type == 0xC0 || type == 0xD0) { pos += 1; }
                else {
                    uint8_t d1 = data[pos++];
                    uint8_t d2 = data[pos++];


                    if (type == 0x90 && channel == 9 && d2 > 0) {
                        allHits.append({absTick, d1, d2});
                    }
                }
            }
        }
    }


    int targetBar = m_spinStartPattern->value();
    if (targetBar < 1) targetBar = 1;

    double ticksPerStep = ppq / 4.0;
    int startTick = (targetBar - 1) * 4 * ppq;

    int numSteps = 64;
    if (m_comboGridSize->currentIndex() == 0) numSteps = 16;
    else if (m_comboGridSize->currentIndex() == 1) numSteps = 32;

    int endTick = startTick + (numSteps * ticksPerStep);

    m_bbTable->setRowCount(0);
    m_bbTable->setColumnCount(numSteps);
    QMap<int, int> noteToRow;


    QMap<int, QString> gmDrums = {
        {35, "Acoustic Bass Drum"}, {36, "Bass Drum 1"}, {37, "Side Stick"}, {38, "Acoustic Snare"},
        {39, "Hand Clap"}, {40, "Electric Snare"}, {41, "Low Floor Tom"}, {42, "Closed Hi Hat"},
        {43, "High Floor Tom"}, {44, "Pedal Hi-Hat"}, {45, "Low Tom"}, {46, "Open Hi-Hat"},
        {49, "Crash Cymbal 1"}, {51, "Ride Cymbal 1"}, {56, "Cowbell"}
    };

    for (const MidiHit& hit : allHits) {
        if (hit.tick >= startTick && hit.tick < endTick) {

            int step = qRound((hit.tick - startTick) / ticksPerStep);

            if (step >= 0 && step < numSteps) {
                if (!noteToRow.contains(hit.note)) {
                    int newRow = m_bbTable->rowCount();
                    m_bbTable->insertRow(newRow);
                    QString name = gmDrums.value(hit.note, QString("MIDI Note %1").arg(hit.note));
                    m_bbTable->setVerticalHeaderItem(newRow, new QTableWidgetItem(name));
                    noteToRow[hit.note] = newRow;
                }

                int row = noteToRow[hit.note];
                QTableWidgetItem *item = m_bbTable->item(row, step);
                if (!item) {
                    item = new QTableWidgetItem();
                    item->setTextAlignment(Qt::AlignCenter);
                    m_bbTable->setItem(row, step, item);
                }
                item->setText(QString("Hit\nVol:%1").arg(hit.vel));
                item->setBackground(QBrush(QColor(138, 43, 226, 150)));
            }
        }
    }

    QList<QPushButton*> buttons = m_tabGroove->findChildren<QPushButton*>();
    for (QPushButton* btn : std::as_const(buttons)) {
        if (btn->text().contains("NEW .mmp")) btn->setEnabled(true);
    }

    QMessageBox::information(this, "MIDI Loaded", QString("Extracted %1 unique drum tracks from Bar %2!").arg(m_bbTable->rowCount()).arg(targetBar));
}

void MainWindow::generate303Project() {
    const QString xmlTemplate = R"XML(<?xml version="1.0"?>
<!DOCTYPE lmms-project>
<lmms-project version="20" creator="LMMS" type="song">
  <head mastervol="100" bpm="%1"/>
  <song>
    <trackcontainer type="song">
      <track name="303-Test" type="0">
        <instrumenttrack basenote="69" vol="100" pitch="0" pan="0" fxch="0">
<instrument name="xpressive">
    <xpressive W1="" smoothW1="0" smoothW3="0" W3sample="AA==" interpolateW2="0" A1="1" W2="" PAN1="0" W3="" O2="" PAN2="-1" W1sample="AA==" interpolateW1="0" W2sample="AA==" interpolateW3="0" A2="1" RELTRANS="50" smoothW2="0" version="0.1" O1="saww(integrate(f)) * (1 + (v > 0.7) * 0.7)" A3="1">              <key/>
            </xpressive>
          </instrument>
          <eldata ftype="7" fwet="1">
            <fcut value="6081" id="14001" scale_type="linear"/>
            <fres value="1.77" id="30821"/>
            <elvol lpdel="0" pdel="0" dec="0.132" x100="0" amt="1" lshp="0" sustain="0.716" lspd_syncmode="0" userwavefile="" lspd_numerator="4" rel="0.184" ctlenvamt="0" att="0" lamt="0" hold="0" lspd_denominator="4" lspd="0.1" latt="0"/>
          </eldata>
        </instrumenttrack>
      </track>
<track type="5" name="Cutoff Automation">
        <automationtrack/>
        <automationpattern name="Sweep" prog="0" tens="1" len="%2" pos="0">
          <object id="14001"/>
        </automationpattern>
      </track>
      <track type="5" name="Resonance Automation" solo="0" muted="0" mutedBeforeSolo="0">
        <automationtrack/>
        <automationpattern prog="2" name="303-Test>Envelopes/LFOs>Q/Resonance" pos="0" mute="0" len="%2" tens="1">
          <object id="30821"/>
        </automationpattern>
      </track>
    </trackcontainer>
  </song>
</lmms-project>)XML";


    int bpm = m_spin303Bpm->value();
        double C_base = m_spin303Cutoff->value();
        double E_mod = m_spin303EnvMod->value();
        double R_base = m_spin303Resonance->value();

        int ticksPerStep = 12;
        double step_time = 60.0 / bpm / 4.0;

        int totalBars = m_spin303TotalBars->value();
        int totalSteps = totalBars * 16;
        int totalTicks = totalSteps * ticksPerStep;

        QString filledXml = xmlTemplate.arg(bpm).arg(totalTicks);
        QDomDocument doc;
        doc.setContent(filledXml);

        QDomElement instrumentTrackNode = doc.elementsByTagName("track").at(0).toElement();
        QDomElement cutoffTrackNode = doc.elementsByTagName("track").at(1).toElement();
        QDomElement autoPattern = cutoffTrackNode.firstChildElement("automationpattern");
        QDomElement resTrackNode = doc.elementsByTagName("track").at(2).toElement();
        QDomElement resPattern = resTrackNode.firstChildElement("automationpattern");

        QDomElement sequencePattern = doc.createElement("pattern");
        sequencePattern.setAttribute("pos", 0);
        sequencePattern.setAttribute("steps", totalSteps);
        sequencePattern.setAttribute("type", 0);

        double v_cap = 0.0;
        int currentTick = 0;
        QDomElement lastNote;

        for (int stepCounter = 0; stepCounter < totalSteps; ++stepCounter) {

            int seqCol = stepCounter % 16;
            QComboBox *stateCombo = qobject_cast<QComboBox*>(m_seqTable->cellWidget(0, seqCol));
            QComboBox *noteCombo = qobject_cast<QComboBox*>(m_seqTable->cellWidget(1, seqCol));
            QSpinBox *octaveSpin = qobject_cast<QSpinBox*>(m_seqTable->cellWidget(2, seqCol));
            QCheckBox *slideCheck = qobject_cast<QCheckBox*>(m_seqTable->cellWidget(3, seqCol));
            QCheckBox *accentCheck = qobject_cast<QCheckBox*>(m_seqTable->cellWidget(4, seqCol));

            int stepState = stateCombo->currentIndex();
            int stepNote = (octaveSpin->value() + 1) * 12 + noteCombo->currentIndex();
            bool stepAccent = accentCheck->isChecked();
            bool stepSlide = slideCheck->isChecked();


            int nextSeqCol = (stepCounter + 1) % 16;
            QComboBox *nextNoteCombo = qobject_cast<QComboBox*>(m_seqTable->cellWidget(1, nextSeqCol));
            QSpinBox *nextOctaveSpin = qobject_cast<QSpinBox*>(m_seqTable->cellWidget(2, nextSeqCol));
            int nextNote = (nextOctaveSpin->value() + 1) * 12 + nextNoteCombo->currentIndex();


            int filterLen = m_spinFilterLength->value();
            int filterCol = stepCounter % filterLen;
            QSlider *freqSlider = qobject_cast<QSlider*>(m_freqTable->cellWidget(0, filterCol));
            QSlider *resSlider = qobject_cast<QSlider*>(m_resTable->cellWidget(0, filterCol));

            double currentC_base = freqSlider ? (freqSlider->value() / 100.0) : 0.5;
            double currentR = resSlider ? (resSlider->value() / 100.0) : 0.5;
            double currentE_mod = m_spin303EnvMod->value();

            double tau_d = 0.1 + (currentR * 0.4);
            double tau_c = 0.05 + (currentR * 0.2);


            auto addAutoNode = [&](int tickOffset, double val, double resVal) {
                double mappedVal = 3000.0 + (val * 8000.0);
                QDomElement timeNode = doc.createElement("time");
                timeNode.setAttribute("pos", static_cast<int>(currentTick + tickOffset));
                timeNode.setAttribute("value", QString::number(mappedVal, 'f', 2));
                timeNode.setAttribute("outValue", QString::number(mappedVal, 'f', 2));
                autoPattern.insertBefore(timeNode, autoPattern.firstChildElement("object"));

                double mappedRes = 0.1 + (resVal * 9.9);
                QDomElement resNode = doc.createElement("time");
                resNode.setAttribute("pos", static_cast<int>(currentTick + tickOffset));
                resNode.setAttribute("value", QString::number(mappedRes, 'f', 2));
                resNode.setAttribute("outValue", QString::number(mappedRes, 'f', 2));
                resPattern.insertBefore(resNode, resPattern.firstChildElement("object"));
            };


            if (stepState == 2) {
                currentTick += ticksPerStep;
                v_cap = v_cap * std::exp(-step_time / tau_d);
                addAutoNode(0, currentC_base + (currentE_mod * 0.2), currentR);
                addAutoNode(6, currentC_base, currentR);
                continue;
            }

            if (stepState == 1) {
                if (!lastNote.isNull()) {
                    int oldLen = lastNote.attribute("len").toInt();
                    lastNote.setAttribute("len", QString::number(oldLen + ticksPerStep));
                }
                currentTick += ticksPerStep;
                v_cap = v_cap * std::exp(-step_time / tau_d);
                addAutoNode(0, currentC_base + (currentE_mod * 0.2), currentR);
                continue;
            }

            QDomElement n = doc.createElement("note");
            n.setAttribute("key", stepNote);
            n.setAttribute("vol", stepAccent ? 127 : 80);
            n.setAttribute("pos", currentTick);
            n.setAttribute("len", stepSlide ? 14 : 6);

            if (m_checkParseSlides->isChecked() && stepSlide) {
                double slideTargetValue = (nextNote - stepNote);

                QDomElement autoPat = doc.createElement("automationpattern");
                QDomElement detuning = doc.createElement("detuning");
                detuning.setAttribute("name", "Note detuning");
                detuning.setAttribute("mute", "0");
                detuning.setAttribute("tens", "1");
                detuning.setAttribute("prog", "1");
                detuning.setAttribute("len", "12");
                detuning.setAttribute("pos", "0");

                QDomElement time1 = doc.createElement("time");
                time1.setAttribute("value", "0");
                time1.setAttribute("outValue", "0");
                time1.setAttribute("pos", "0");

                QDomElement time2 = doc.createElement("time");
                time2.setAttribute("value", QString::number(slideTargetValue));
                time2.setAttribute("outValue", QString::number(slideTargetValue));
                time2.setAttribute("pos", "12");

                detuning.appendChild(time1);
                detuning.appendChild(time2);
                autoPat.appendChild(detuning);
                n.appendChild(autoPat);
            }

            sequencePattern.appendChild(n);
            lastNote = n;

            if (stepAccent) {
                v_cap = v_cap * std::exp(-step_time / tau_d) + 1.0 * (1.0 - std::exp(-0.05 / tau_c));
                double peak = currentC_base + (currentE_mod * 0.4) + (currentR * v_cap * 0.5);
                if (peak > 1.0) peak = 1.0;

                addAutoNode(0, peak, currentR);
                addAutoNode(1, peak * 0.8, currentR);
                addAutoNode(6, peak * 0.3, currentR);
                addAutoNode(19, currentC_base, currentR);
            } else {
                v_cap = v_cap * std::exp(-step_time / tau_d);
                addAutoNode(0, currentC_base + (currentE_mod * 0.2), currentR);
                addAutoNode(6, currentC_base, currentR);
            }

            currentTick += ticksPerStep;
        }

        instrumentTrackNode.appendChild(sequencePattern);

        QString savePath = QFileDialog::getSaveFileName(this, "Save 303 Project", QDir::currentPath(), "LMMS Project (*.mmp)");
        if (!savePath.isEmpty()) {
            QFile outFile(savePath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&outFile);
                stream << doc.toString(2);
                outFile.close();
                QMessageBox::information(this, "Success", "Project generated successfully!");
            }
        }
    }

    void MainWindow::on303PatternChanged(int index) {
        if (!m_seqTable) return;

        struct Step { int note; bool accent; bool slide; };
        std::vector<Step> pattern;

        if (index == 0) {
            pattern = {
                {36, false, false}, {36, true, false}, {48, true, true}, {48, false, false},
                {39, false, false}, {39, true, false}, {36, true, false}, {36, true, false},
                {48, false, false}, {48, false, true}, {51, true, false}, {36, false, false},
                {36, true, false}, {36, true, true}, {43, true, false}, {36, false, false}
            };
        } else if (index == 1) {
            pattern = {{36, true, false}, {48, false, true}, {48, true, false}, {39, false, false}, {41, true, false}};
        } else if (index == 2) {
            pattern = {
                {36, false, false}, {36, false, true}, {48, true, false}, {36, false, false},
                {39, false, false}, {39, true, true},  {51, true, false}, {36, false, false},
                {43, true, false},  {36, false, false}, {36, true, false}, {48, false, false}
            };
        } else if (index == 3) {
            pattern = {
                {36, true, false},  {36, false, true},  {48, false, false}, {36, false, false},
                {39, true, true},   {41, false, false}, {36, false, false}, {36, true, false},
                {48, false, true},  {51, true, false},  {36, false, false}, {39, false, true},
                {41, true, false},  {36, false, false}, {36, false, false}, {48, true, false}
            };
        } else if (index == 4) {
            pattern = {
                {36, true, false}, {48, false, true},  {48, true, false},
                {39, false, false}, {36, false, true}, {41, true, false}, {36, false, false}
            };
        } else if (index == 5) {
            pattern = {
                {36, false, true}, {60, true, false}, {36, false, false}, {36, true, true},
                {48, false, false}, {36, false, true}, {55, true, false}, {36, false, false},
                {36, true, false}, {60, false, true}, {48, true, false}, {36, false, false},
                {39, true, true},  {48, false, false}, {36, true, false}, {60, true, false}
            };
        } else if (index == 6) {
            pattern = {
                {36, true, false}, {36, false, true}, {48, false, false}, {39, true, false},
                {36, false, false}, {41, true, true}, {53, false, false}, {36, true, false},
                {36, false, true}, {48, true, false}, {39, false, false}, {36, false, false},
                {41, true, false}, {43, false, true}, {55, true, false}, {36, false, false}
            };
        } else if (index == 7) {
            pattern = {
                {38, true, false}, {38, false, true}, {50, true, false}, {41, false, false},
                {43, true, true},  {55, false, false}, {38, false, false}, {38, true, false},
                {50, true, false}, {41, false, true}, {43, true, false}, {38, false, false},
                {50, true, true},  {62, true, false}, {41, false, false}, {43, true, false}
            };
        } else if (index == 8) {
            pattern = {
                {40, false, false}, {40, true, false}, {52, false, true}, {52, false, false},
                {50, true, false},  {47, false, false}, {43, true, false}, {40, false, false},
                {40, true, false},  {52, false, false}, {50, true, true},  {52, false, false},
                {47, false, false}, {43, true, false}, {45, false, true},  {47, true, false}
            };
        } else if (index == 9) {
            pattern = {
                {45, true, false}, {45, false, true}, {57, false, false}, {45, false, false},
                {49, true, false}, {45, false, false}, {47, false, true}, {52, true, false},
                {45, false, false}, {45, true, true}, {57, false, false}, {45, false, false},
                {49, false, true}, {47, true, false}, {45, false, false}, {40, true, false}
            };
        } else if (index == 10) {
            pattern = {
                {36, true, false}, {36, false, false}, {36, true, true},  {39, false, false},
                {41, true, false}, {36, false, false}, {43, false, true}, {43, true, false},
                {36, false, false}, {36, true, false}, {36, false, true}, {39, true, false},
                {41, false, false}, {43, true, true},  {46, false, false}, {43, true, false}
            };
        }

        for (int col = 0; col < 16; ++col) {
            if (col < pattern.size()) {
                int midi = pattern[col].note;
                int noteIdx = midi % 12;
                int octave = (midi / 12) - 1;

                QComboBox *stateCombo = qobject_cast<QComboBox*>(m_seqTable->cellWidget(0, col)); // NEW
                QComboBox *noteCombo = qobject_cast<QComboBox*>(m_seqTable->cellWidget(1, col));
                QSpinBox *octaveSpin = qobject_cast<QSpinBox*>(m_seqTable->cellWidget(2, col));
                QCheckBox *slideCheck = qobject_cast<QCheckBox*>(m_seqTable->cellWidget(3, col));
                QCheckBox *accentCheck = qobject_cast<QCheckBox*>(m_seqTable->cellWidget(4, col));

                if(stateCombo) stateCombo->setCurrentIndex(0);
                if(noteCombo) noteCombo->setCurrentIndex(noteIdx);
                if(octaveSpin) octaveSpin->setValue(octave);
                if(slideCheck) slideCheck->setChecked(pattern[col].slide);
                if(accentCheck) accentCheck->setChecked(pattern[col].accent);
            }
        }
    }
    void MainWindow::onFilterLengthChanged(int steps) {
        m_freqTable->setColumnCount(steps);
        m_resTable->setColumnCount(steps);

        for (int col = 0; col < steps; ++col) {

            if (!m_freqTable->cellWidget(0, col)) {
                QSlider *fSlider = new QSlider(Qt::Horizontal);
                fSlider->setRange(0, 100); fSlider->setValue(50);
                m_freqTable->setCellWidget(0, col, fSlider);
            }

            if (!m_resTable->cellWidget(0, col)) {
                QSlider *rSlider = new QSlider(Qt::Horizontal);
                rSlider->setRange(0, 100); rSlider->setValue(80);
                m_resTable->setCellWidget(0, col, rSlider);
            }
        }
    }

    void MainWindow::onFilterPatternChanged(int index) {
        if (index == 0) return;
        struct FreqRes { int f; int r; };
        std::vector<FreqRes> vals;

        if (index == 1) { // 3-Step Polymeter
                    m_spinFilterLength->setValue(3);
                    vals = {{80, 90}, {30, 40}, {50, 80}};
                } else if (index == 2) { // 5-Step Tension
                    m_spinFilterLength->setValue(5);
                    vals = {{20, 100}, {30, 90}, {40, 80}, {60, 95}, {100, 100}};
                } else if (index == 3) { // 7-Step Demented
                    m_spinFilterLength->setValue(7);
                    vals = {{90, 80}, {20, 30}, {30, 40}, {85, 95}, {10, 20}, {60, 60}, {95, 100}};
                } else if (index == 4) { // 16-Step Sine
                    m_spinFilterLength->setValue(16);
                    for(int i=0; i<16; ++i) {
                        int f = 50 + 40 * std::sin(i * M_PI / 8.0);
                        vals.push_back({f, 85});
                    }
                } else if (index == 5) { // 24-Step Bubbler
                    m_spinFilterLength->setValue(24);
                    for(int i=0; i<24; ++i) {
                        int r = (i % 3 == 0) ? 95 : 60;
                        int f = 30 + (i * 2) % 60;
                        vals.push_back({f, r});
                    }
                } else if (index == 6) { // 12-Step Rolling Reso
                    m_spinFilterLength->setValue(12);
                    vals = {{40, 90}, {60, 95}, {80, 80}, {30, 70}, {50, 85}, {70, 90},
                            {40, 95}, {60, 80}, {80, 70}, {30, 85}, {50, 90}, {70, 95}};
                } else if (index == 7) { // 32-Step Slow Sweep
                    m_spinFilterLength->setValue(32);
                    for(int i=0; i<32; ++i) {
                        vals.push_back({10 + (int)(80.0 * i / 31.0), 85});
                    }
                } else if (index == 8) {
                    m_spinFilterLength->setValue(9);
                    vals = {{95, 95}, {20, 40}, {20, 40}, {80, 90}, {20, 40}, {90, 95}, {20, 40}, {20, 40}, {70, 85}};
                } else if (index == 9) {
                    m_spinFilterLength->setValue(64);
                    for(int i=0; i<64; ++i) {
                        int f = 40 + 30 * std::sin(i * M_PI / 16.0); // Slow sine cutoff
                        int r = 70 + 25 * std::cos(i * M_PI / 8.0);  // Faster cosine resonance
                        vals.push_back({f, r});
                    }
                }

        for (int i=0; i < vals.size(); ++i) {
            QSlider *fSlider = qobject_cast<QSlider*>(m_freqTable->cellWidget(0, i));
            QSlider *rSlider = qobject_cast<QSlider*>(m_resTable->cellWidget(0, i));
            if (fSlider) fSlider->setValue(vals[i].f);
            if (rSlider) rSlider->setValue(vals[i].r);
        }
    }

    void MainWindow::onAnalyzerAddPattern()
    {
        QStringList fileNames = QFileDialog::getOpenFileNames(this, "Select LMMS Patterns", "", "LMMS Pattern (*.xpt)");
        if (fileNames.isEmpty()) return;

        for (const QString& fileName : fileNames) {
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            QDomDocument doc;
            if (!doc.setContent(&file)) { file.close(); continue; }
            file.close();

            LoadedPattern lp;
            lp.fileName = QFileInfo(fileName).fileName();


            QDomElement patElem = doc.documentElement().firstChildElement("pattern");
            lp.patternStartPos = patElem.attribute("pos", "0").toInt();


            QDomNodeList notes = doc.elementsByTagName("note");
            double chroma[12] = {0};

            for (int i = 0; i < notes.count(); ++i) {
                int key = notes.at(i).toElement().attribute("key").toInt();
                lp.uniqueKeys.push_back(key);
                if (key >= 0) {

                    chroma[key % 12] += 1.0;
                }
            }


            int majorTemplate[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
            int minorTemplate[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};

            double bestScore = -1.0;
            lp.rootNote = 0;
            lp.scaleType = 0;

            const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};


            for (int root = 0; root < 12; ++root) {
                double majScore = 0;
                double minScore = 0;

                for (int i = 0; i < 12; ++i) {
                    int templateIndex = (i - root + 12) % 12;
                    majScore += chroma[i] * majorTemplate[templateIndex];
                    minScore += chroma[i] * minorTemplate[templateIndex];
                }


                if (majScore > bestScore) {
                    bestScore = majScore;
                    lp.rootNote = root;
                    lp.scaleType = 0; // Major
                    lp.fullScaleName = noteNames[root] + " Major";
                }
                if (minScore > bestScore) {
                    bestScore = minScore;
                    lp.rootNote = root;
                    lp.scaleType = 1; // Minor
                    lp.fullScaleName = noteNames[root] + " Minor";
                }
            }

            m_analyzerPatterns.push_back(lp);
            m_listAnalyzerPatterns->addItem(lp.fileName + " (" + lp.fullScaleName + ")");
        }

        runPatternAnalysis();
    }

    void MainWindow::onAnalyzerClearPatterns()
    {
        m_analyzerPatterns.clear();
        m_listAnalyzerPatterns->clear();
        m_txtAnalyzerFeedback->clear();
    }

    void MainWindow::runPatternAnalysis()
    {
        if (m_analyzerPatterns.size() < 2) {
            m_txtAnalyzerFeedback->setHtml("<i>Load at least 2 patterns to compare them...</i>");
            return;
        }

        QString report = "<h2>Pattern Analysis Report</h2>";


        report += "<h3>⏱️ Timeline Alignment</h3>";
        bool allAligned = true;
        int firstPos = m_analyzerPatterns[0].patternStartPos;

        for (const auto& pat : m_analyzerPatterns) {
            if (pat.patternStartPos != firstPos) {
                allAligned = false;
                break;
            }
        }

        if (allAligned) {
            report += "<p style='color: green;'><b>Perfect!</b> All patterns start at position " + QString::number(firstPos) + " and will play simultaneously.</p>";
        } else {
            report += "<p style='color: orange;'><b>Warning: Staggered Patterns</b><br/>These patterns are set to start at different times. If you want them to play together, you must align them in the LMMS Song Editor.</p><ul>";
            for (const auto& pat : m_analyzerPatterns) {
                report += "<li>" + pat.fileName + " starts at <b>pos " + QString::number(pat.patternStartPos) + "</b></li>";
            }
            report += "</ul>";
        }


            report += "<h3>🎵 Harmonic Clash Check</h3>";
            bool perfectMatch = true;
            QString baseScale = m_analyzerPatterns[0].fullScaleName;

            for (const auto& pat : m_analyzerPatterns) {
                if (pat.fullScaleName != baseScale) {
                    perfectMatch = false;
                    break;
                }
            }

            if (perfectMatch) {
                report += "<p style='color: green;'><b>Harmonic Harmony!</b> All patterns are detected as <b>" + baseScale + "</b>.</p>";
            } else {
                report += "<p style='color: red;'><b>Harmonic Clash Detected!</b></p><ul>";
                for (const auto& pat : m_analyzerPatterns) {
                    report += "<li>" + pat.fileName + " is in <b>" + pat.fullScaleName + "</b></li>";
                }
                report += "</ul>";


                report += "<h4>💡 How to Fix It (Transposition Guide):</h4><ul>";
                LoadedPattern masterPat = m_analyzerPatterns[0];

                for (size_t i = 1; i < m_analyzerPatterns.size(); ++i) {
                    LoadedPattern currentPat = m_analyzerPatterns[i];

                    if (currentPat.rootNote != masterPat.rootNote || currentPat.scaleType != masterPat.scaleType) {


                        int shift = masterPat.rootNote - currentPat.rootNote;
                        if (shift > 6) shift -= 12;  // Shift down instead of way up
                        if (shift < -5) shift += 12; // Shift up instead of way down

                        QString dir = (shift >= 0) ? "UP" : "DOWN";

                        report += "<li>To make <b>" + currentPat.fileName + "</b> match the root of <b>" + masterPat.fileName + "</b>:<br/>";

                        if (shift == 0) {
                            report += "Roots already match, no shift needed!";
                        } else {
                            report += "Highlight all notes in LMMS and shift <b>" + dir + " by " + QString::number(std::abs(shift)) + " semitones</b>.</li>";
                        }


                        if (masterPat.scaleType != currentPat.scaleType) {
                            report += "<ul><li style='color: gray;'><i>Warning: One is Major and the other is Minor. Shifting the root will align them, but you may still have clashing 3rds. Consider rewriting the minor 3rds to major 3rds!</i></li></ul>";
                        }
                    }
                }
                report += "</ul>";
            }

            m_txtAnalyzerFeedback->setHtml(report);
    }
