#ifndef HOUSEBEATGENERATOR_H
#define HOUSEBEATGENERATOR_H

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPainter>
#include <QtXml>
#include <vector>
#include <map>


class VolumeAutomationLane : public QWidget {
    Q_OBJECT
public:
    explicit VolumeAutomationLane(int bars, const QString& name, QWidget *parent = nullptr);
    void setBars(int bars);
    float getVolumeAtRatio(float ratio) const;
    bool hasAutomation() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateValueFromMouse(QMouseEvent *event);

    QString m_name;
    int m_bars;
    std::vector<float> m_points;
    bool m_isAutomated;
};

struct Drum {
    QString name;
    QString xpressiveO1;
    QString params;
    float defaultVol;
    int defaultPan;
    bool swingEnabledByDefault;
    bool canFlam;
};

class HouseBeatGenerator : public QWidget
{
    Q_OBJECT

public:
    explicit HouseBeatGenerator(QWidget *parent = nullptr);
    ~HouseBeatGenerator();

private slots:
    void onPresetChanged(int index);
    void onExportMMPClicked();
    void onSwingGlobalChanged();
    void onGridCellClicked(int row, int col);
    void onSongLengthChanged(int bars);
    void onHatFxModeChanged(int index);
    void onDevDumpClicked();
    void onRandomSnareBuild();
    void onRandomSnarePattern();
    void onRandomRimshotPattern();
    void onDuplicate16Clicked();
    void onDevLoadClicked();

private:
    void setupUI();
    void createPresets();
    void applyPreset(int presetIndex);
    void updateCell(int row, int col);
    void buildMMP(const QString &filePath);
    QPushButton *m_btnDevLoad;


    QTableWidget *m_grid;
    QComboBox *m_presetCombo;
    QSpinBox *m_bpmSpin;
    QSpinBox *m_songLengthSpin;
    QSpinBox *m_shuffleDial;
    QDoubleSpinBox *m_ghostIntensity;
    QPushButton *m_btnExport;
    QPushButton *m_btnDevDump;
    QPushButton *m_btnDuplicate16;


    QComboBox *m_closedHatVelCombo;
    QComboBox *m_openHatVelCombo;
    QSpinBox *m_velDepthSpin;
    QComboBox *m_hatFxModeCombo;
    QComboBox *m_filterTypeCombo;
    QComboBox *m_filterModCombo;
    QDoubleSpinBox *m_lfoBarsSpin;


    std::map<QString, QDoubleSpinBox*> m_tuneKnobs;
    std::map<QString, QDoubleSpinBox*> m_attackKnobs;
    std::map<QString, QDoubleSpinBox*> m_decayKnobs;
    std::map<QString, QDoubleSpinBox*> m_toneKnobs;
    std::map<QString, QDoubleSpinBox*> m_snappyKnobs;

    QScrollArea *m_automationScrollArea;
    std::vector<VolumeAutomationLane*> m_automationLanes;

    std::vector<Drum> m_drums;
    std::vector<std::vector<float>> m_velocities;
    std::vector<std::vector<std::vector<float>>> m_presets;
};

#endif // HOUSEBEATGENERATOR_H
