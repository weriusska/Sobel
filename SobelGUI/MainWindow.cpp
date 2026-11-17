#include "MainWindow.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QPixmap>
#include <malloc.h>
#include <cstring>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {
    freeBuffers();
}

void MainWindow::setupUi() {
    setWindowTitle("JA - Wykrywanie krawędzi filtrem Sobela (SIMD vs C++)");
    resize(1280, 760);

    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Pasek górny z przyciskami
    QHBoxLayout* topBar = new QHBoxLayout();
    btnLoad = new QPushButton("1. Wczytaj obraz", this);
    comboMode = new QComboBox(this);
    comboMode->addItem("C++ (Skalarny)");
    comboMode->addItem("MASM x64 (Skalarny)");
    comboMode->addItem("MASM x64 (Wektorowy SIMD)");

    btnRun = new QPushButton("2. Uruchom filtr Sobela", this);
    btnRun->setEnabled(false);

    btnSave = new QPushButton("3. Zapisz wynik", this);
    btnSave->setEnabled(false);

    topBar->addWidget(btnLoad);
    topBar->addWidget(comboMode);
    topBar->addWidget(btnRun);
    topBar->addWidget(btnSave);
    topBar->addStretch();

    // Środkowy obszar z dwoma podglądami obrazu
    QHBoxLayout* imageLayout = new QHBoxLayout();

    lblImageOrig = new QLabel("Brak wczytanego obrazu", this);
    lblImageOrig->setAlignment(Qt::AlignCenter);
    QScrollArea* scrollOrig = new QScrollArea(this);
    scrollOrig->setWidget(lblImageOrig);
    scrollOrig->setWidgetResizable(true);

    lblImageEdge = new QLabel("Tutaj pojawi się mapa krawędzi", this);
    lblImageEdge->setAlignment(Qt::AlignCenter);
    QScrollArea* scrollEdge = new QScrollArea(this);
    scrollEdge->setWidget(lblImageEdge);
    scrollEdge->setWidgetResizable(true);

    imageLayout->addWidget(scrollOrig);
    imageLayout->addWidget(scrollEdge);

    // Dolny pasek statusu
    lblStatus = new QLabel("Gotowy. Maksymalna rozdzielczość: 3840x2160 (limit buforów: 64 MB).", this);

    mainLayout->addLayout(topBar);
    mainLayout->addLayout(imageLayout, 1);
    mainLayout->addWidget(lblStatus);
    setCentralWidget(central);

    // Podpięcie sygnałów do slotów
    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(btnRun, &QPushButton::clicked, this, &MainWindow::onRunFilter);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveImage);
}

void MainWindow::freeBuffers() {
    if (bufRGBA) { _aligned_free(bufRGBA); bufRGBA = nullptr; }
    if (bufGray) { _aligned_free(bufGray); bufGray = nullptr; }
    if (bufEdge) { _aligned_free(bufEdge); bufEdge = nullptr; }
}

bool MainWindow::allocateBuffers(int width, int height) {
    freeBuffers();

    size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    size_t sizeRGBA = pixels * 4; // 4 B na piksel (dla 4K: 33 177 600 B)
    size_t sizeGray = pixels * 1; // 1 B na piksel (dla 4K: 8 294 400 B)
    size_t sizeEdge = pixels * 1; // 1 B na piksel (dla 4K: 8 294 400 B)
    size_t totalBytes = sizeRGBA + sizeGray + sizeEdge; // Dla 4K: 49 766 400 B

    if (totalBytes > MAX_TOTAL_BYTES) {
        QMessageBox::critical(this, "Błąd alokacji", "Przekroczono sztywny limit 64 MB pamięci buforów!");
        return false;
    }

    // Alokacja wyrównana do 32 bajtów pod instrukcje wektorowe AVX2
    bufRGBA = static_cast<uint8_t*>(_aligned_malloc(sizeRGBA, 32));
    bufGray = static_cast<uint8_t*>(_aligned_malloc(sizeGray, 32));
    bufEdge = static_cast<uint8_t*>(_aligned_malloc(sizeEdge, 32));

    if (!bufRGBA || !bufGray || !bufEdge) {
        freeBuffers();
        QMessageBox::critical(this, "Błąd pamięci", "Nie udało się zaalokować buforów obrazu!");
        return false;
    }

    std::memset(bufGray, 0, sizeGray);
    std::memset(bufEdge, 0, sizeEdge);
    return true;
}

void MainWindow::onLoadImage() {
    QString path = QFileDialog::getOpenFileName(
        this, "Wybierz obraz", "", "Obrazy (*.png *.jpg *.jpeg *.bmp)"
    );
    if (path.isEmpty()) return;

    QImage tempImg;
    if (!tempImg.load(path)) {
        QMessageBox::warning(this, "Błąd", "Nie udało się otworzyć wybranego pliku.");
        return;
    }

    // Weryfikacja rozdzielczości 4K (3840x2160) zgodna z kartą projektu
    if (tempImg.width() > MAX_WIDTH || tempImg.height() > MAX_HEIGHT) {
        QMessageBox::critical(
            this,
            "Zbyt duża rozdzielczość",
            QString("Obraz ma wymiary %1x%2, co przekracza dopuszczalny limit 4K (3840x2160)!")
            .arg(tempImg.width()).arg(tempImg.height())
        );
        return;
    }

    // Konwersja do ścisłego formatu RGBA (4 bajty na piksel: R, G, B, A)
    loadedImage = tempImg.convertToFormat(QImage::Format_RGBA8888);
    imgWidth = loadedImage.width();
    imgHeight = loadedImage.height();

    if (!allocateBuffers(imgWidth, imgHeight)) {
        return;
    }

    // Skopiowanie surowych bajtów RGBA z QImage do naszego wyrównanego bufora bufRGBA
    size_t byteCount = static_cast<size_t>(imgWidth) * imgHeight * 4;
    std::memcpy(bufRGBA, loadedImage.constBits(), byteCount);

    // Wyświetlenie podglądu oryginału (przeskalowane do rozmiaru okna)
    lblImageOrig->setPixmap(
        QPixmap::fromImage(loadedImage).scaled(
            lblImageOrig->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
        )
    );
    lblImageEdge->setText("Gotowy do uruchomienia filtru.");

    double totalMB = (static_cast<double>(imgWidth) * imgHeight * 6.0) / (1000.0 * 1000.0);
    lblStatus->setText(
        QString("Wczytano: %1x%2 px | Zaalokowane bufory (RGBA + 2x Gray): %3 MB")
        .arg(imgWidth).arg(imgHeight).arg(totalMB, 0, 'f', 2)
    );

    btnRun->setEnabled(true);
    btnSave->setEnabled(false);
}

void MainWindow::onRunFilter() {
    if (!bufRGBA || !bufEdge) return;

    // TYMCZASOWY TEST (Dzień 3): sprawdzamy, czy potok QImage -> bufRGBA -> bufEdge -> ekran działa!
    // W Dniu 4 i 6 podmienimy tę pętlę na wywołanie funkcji z SobelCPP.dll oraz SobelASM.dll.
    size_t pixels = static_cast<size_t>(imgWidth) * imgHeight;
    for (size_t i = 0; i < pixels; ++i) {
        uint16_t r = bufRGBA[i * 4 + 0];
        uint16_t g = bufRGBA[i * 4 + 1];
        uint16_t b = bufRGBA[i * 4 + 2];
        bufEdge[i] = static_cast<uint8_t>((77 * r + 150 * g + 29 * b) >> 8);
    }

    // Zbudowanie obrazu wyjściowego 8-bit Grayscale z bufora bufEdge
    resultImage = QImage(imgWidth, imgHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < imgHeight; ++y) {
        std::memcpy(resultImage.scanLine(y), bufEdge + y * imgWidth, imgWidth);
    }

    lblImageEdge->setPixmap(
        QPixmap::fromImage(resultImage).scaled(
            lblImageEdge->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
        )
    );
    lblStatus->setText("Przetwarzanie zakończone pomyślnie (test skali szarości 77R+150G+29B >> 8).");
    btnSave->setEnabled(true);
}

void MainWindow::onSaveImage() {
    if (resultImage.isNull()) return;
    QString outPath = QFileDialog::getSaveFileName(
        this, "Zapisz wynik", "sobel_wynik.png", "Obraz PNG (*.png);;Obraz BMP (*.bmp)"
    );
    if (!outPath.isEmpty()) {
        resultImage.save(outPath);
    }
}