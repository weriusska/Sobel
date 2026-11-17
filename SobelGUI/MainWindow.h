#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QScrollArea>
#include <QtGui/QImage>
#include <cstdint>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLoadImage();
    void onRunFilter();
    void onSaveImage();

private:
    void setupUi();
    void freeBuffers();
    bool allocateBuffers(int width, int height);

    // Sta³e zgodne z kart¹ projektu (4K UHD i limit 64 MB)
    static constexpr int MAX_WIDTH = 3840;
    static constexpr int MAX_HEIGHT = 2160;
    static constexpr size_t MAX_TOTAL_BYTES = 64ULL * 1024 * 1024;

    // Kontrolki GUI
    QPushButton* btnLoad = nullptr;
    QPushButton* btnRun = nullptr;
    QPushButton* btnSave = nullptr;
    QComboBox* comboMode = nullptr;
    QLabel* lblStatus = nullptr;
    QLabel* lblImageOrig = nullptr;
    QLabel* lblImageEdge = nullptr;

    // Dane obrazu i wyrównane bufory pamiêci (32 bajty pod AVX2)
    QImage loadedImage;
    QImage resultImage;
    int imgWidth = 0;
    int imgHeight = 0;

    uint8_t* bufRGBA = nullptr; // Bufor wejœciowy (4 B na piksel)
    uint8_t* bufGray = nullptr; // Bufor poœredni (1 B na piksel)
    uint8_t* bufEdge = nullptr; // Bufor wyjœciowy (1 B na piksel)
};