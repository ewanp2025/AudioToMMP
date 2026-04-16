#ifndef VOCALXPRESSTAB_H
#define VOCALXPRESSTAB_H

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QListWidget>
#include <QMap>
#include <vector>

struct SAMPhoneme {
    QString name;
    int f1, f2, f3;
    bool voiced;
    int a1, a2, a3, length;
};

struct SequenceNode {
    QString syllable;
    double startSec;
    double durationSec;
};

class VocalXpressTab : public QWidget {
    Q_OBJECT
public:
    explicit VocalXpressTab(QWidget *parent = nullptr);

private slots:
    void phonemizeLyrics();
    void assignSyllable();
    void renderExpression();
    void onPatternLengthChanged();
    void loadPattern();

private:
    void setupUI();
    void initNRLRules();
    void initSamLibrary();
    QString compileNRLRegex(const QString& context, bool isLeft);
    QString textToPhonemes(const QString& english);
    QString buildTimedExpression(bool nightly);


    QLineEdit *m_lyricsInput;
    QPushButton *m_phonemizeBtn;
    QListWidget *m_syllablePool;

    QSpinBox *m_bpmSpin;
    QComboBox *m_stepsCombo;
    QLabel *m_durationLabel;

    QTableWidget *m_patternTable;

    QSlider *m_mouthSlider, *m_throatSlider, *m_pitchSlider, *m_speedSlider;
    QCheckBox *m_nightlyCheckBox;

    QPushButton *m_renderBtn;
    QTextEdit *m_w1Output;
    QTextEdit *m_o1Output;
    QPushButton *m_copyBtn;


    QMap<QString, SAMPhoneme> m_samLibrary;
    std::vector<SequenceNode> m_timedSequence;

    struct SamNRLRule { QString leftRx, match, rightRx, phonemes; };
    QList<SamNRLRule> m_nrlRules;
    QPushButton *m_loadPatternBtn;
};

#endif // VOCALXPRESSTAB_H
