#ifndef HOUSEVOCALSTABS_H
#define HOUSEVOCALSTABS_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

class HouseVocalStabsTab : public QWidget {
    Q_OBJECT

public:
    explicit HouseVocalStabsTab(QWidget *parent = nullptr);

private slots:
    void generateStab();
    void updateLabels();

private:
    QComboBox *stabPresetCombo;

    QSlider *pitchSlider;       QLabel *pitchLabel;
    QSlider *durationSlider;    QLabel *durationLabel;
    QSlider *glottalScoopSlider; QLabel *glottalLabel;
    QSlider *vibratoRateSlider; QLabel *vibratoRateLabel;
    QSlider *vibratoDepthSlider; QLabel *vibratoDepthLabel;
    QSlider *breathSlider;      QLabel *breathLabel;
    QSlider *brightnessSlider;  QLabel *brightnessLabel;
    QSlider *shimmerSlider;     QLabel *shimmerLabel;

    QCheckBox *nightlyCheckBox;
    QPushButton *generateButton;

    QTextEdit *w1Output;
    QTextEdit *o1Output;

    void setupUI();
    QString buildAdvancedVocalExpression(bool nightly);
};

#endif // HOUSEVOCALSTABS_H
