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
#include <QtXml>
#include <vector>

struct Drum {
    QString name;
    QString xpressiveO1;
    QString params;
    float defaultVol;
    int defaultPan;
    bool swingEnabledByDefault;
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
    void onRandomSnareBuild();
    void onRandomSnarePattern();
    void onRandomRimshotPattern();

private:
    void setupUI();
    void createPresets();
    void applyPreset(int presetIndex);
    void updateCell(int row, int col);
    void buildMMP(const QString &filePath);

    QTableWidget *m_grid;
    QScrollArea *m_scrollArea;
    QComboBox *m_presetCombo;
    QSpinBox *m_bpmSpin;
    QSpinBox *m_shuffleDial;
    QDoubleSpinBox *m_ghostIntensity;
    QPushButton *m_btnExport;

    std::vector<Drum> m_drums;
    std::vector<std::vector<float>> m_velocities;
    std::vector<std::vector<std::vector<float>>> m_presets;
};

#endif // HOUSEBEATGENERATOR_H
