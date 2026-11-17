# Wykrywanie krawędzi filtrem Sobela z użyciem instrukcji wektorowych
### *Edge Detection Using a Vectorized Implementation of the Sobel Filter*

![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078D6?logo=windows)
![Language](https://img.shields.io/badge/Language-C%2B%2B17%20%7C%20x86--64%20MASM-00599C?logo=cplusplus)
![SIMD](https://img.shields.io/badge/SIMD-SSE2%20%2F%20AVX2-FF6F00)
![GUI](https://img.shields.io/badge/GUI-Qt%206-41CD52?logo=qt)


## 🎯 Cel projektu

Celem projektu jest implementacja filtru Sobela służącego do wykrywania krawędzi w obrazach cyfrowych z wykorzystaniem instrukcji wektorowych (**SIMD — AVX2/SSE**) w języku asemblera (**x86-64 MASM**) oraz porównanie jej wydajności z implementacją skalarną w języku **C++** oraz skalarną w asemblerze.

---

## 📐 Podstawa matematyczna

Obraz w skali szarości jest reprezentowany jako dyskretna funkcja jasności $I(x, y)$, natomiast krawędź odpowiada miejscu jej gwałtownej zmiany — czyli dużej wartości gradientu:

$$\nabla I = \left(\frac{\partial I}{\partial x}, \frac{\partial I}{\partial y}\right)$$

Operator Sobela przybliża pochodne kierunkowe za pomocą różnicy centralnej $[-1, 0, +1]$ połączonej z wygładzaniem wagami $[1, 2, 1]$ w kierunku prostopadłym, co skutecznie ogranicza wpływ szumu wysokoczęstotliwościowego.

### 1. Konwersja RGBA do skali szarości
Obliczenia wykonywane są w szybkiej arytmetyce stałoprzecinkowej (wagi sumują się do $256$, co pozwala zastąpić dzielenie przesunięciem bitowym):

$$Y = (77 \cdot R + 150 \cdot G + 29 \cdot B) \gg 8$$

### 2. Splot z jądrami Sobela ($3 \times 3$)
Składowe gradientu poziomego $G_x$ oraz pionowego $G_y$ wyznaczane są poprzez dyskretny splot obrazu $I$ z maskami $K_x$ oraz $K_y$:

$$K_x = \begin{bmatrix} -1 & 0 & +1 \\ -2 & 0 & +2 \\ -1 & 0 & +1 \end{bmatrix}, \quad K_y = \begin{bmatrix} -1 & -2 & -1 \\ 0 & 0 & 0 \\ +1 & +2 & +1 \end{bmatrix}$$

$$G_x = I * K_x, \quad G_y = I * K_y$$

### 3. Aproksymacja modułu gradientu i nasycenie
Ponieważ wartości $G_x$ i $G_y$ dla pikseli 8-bitowych mieszczą się w przedziale $[-1020, 1020]$, obliczenia pośrednie realizowane są na **16-bitowych słowach ze znakiem (`int16_t`)**. Końcowa jasność piksela wyjściowego przybliża normę euklidesową $\sqrt{G_x^2 + G_y^2}$ za pomocą normy $L_1$ z nasyceniem do przedziału $[0, 255]$:

$$G = \min(255, \vert{}G_x\vert{} + \vert{}G_y\vert{})$$

---

## 💾 Architektura pamięci i obsługa rozdzielczości 4K

Dla optymalnego działania instrukcji wektorowych obraz wejściowy ładowany jest w formacie **RGBA** (4 bajty na piksel, adresy buforów wyrównane do granicy 32 bajtów pod rejestry `YMM`). Biblioteka dynamiczna sama przeprowadza konwersję do skali szarości, a następnie wykonuje filtrację Sobela.

Maksymalna obsługiwana rozdzielczość to **4K UHD ($3840 \times 2160$, tj. $8\,294\,400$ pikseli)**:

| Bufor | Format | Rozmiar na piksel | Rozmiar dla 4K ($3840 \times 2160$) |
| :--- | :--- | :---: | ---: |
| **Wejściowy (`bufRGBA`)** | RGBA | 4 B | $33\,177\,600\text{ B}$ ($\approx 33,18\text{ MB}$) |
| **Pośredni (`bufGray`)** | Grayscale | 1 B | $8\,294\,400\text{ B}$ ($\approx 8,29\text{ MB}$) |
| **Wyjściowy (`bufEdge`)** | Grayscale | 1 B | $8\,294\,400\text{ B}$ ($\approx 8,29\text{ MB}$) |
| **Łącznie** | — | **6 B** | **$49\,766\,400\text{ B}$ ($\approx 49,77\text{ MB}$)** |

* **Sztywny limit alokacji:** `64 MB` (obrazy przekraczające wymiar $3840 \times 2160$ są odrzucane z komunikatem błędu).
* **Wyrównanie wiersza (*stride*):** Dla obrazu 4K długość wiersza wynosi $15\,360\text{ B}$ (RGBA) oraz $3\,840\text{ B}$ (Grayscale) — obie wartości są wielokrotnościami 16 i 32 bajtów.

---

## 🗂️ Struktura projektu

Repozytorium podzielone jest na trzy współpracujące moduły w obrębie jednego rozwiązania Visual Studio (`JA_Sobel.sln`):

```text
JA_Sobel/
├── SobelGUI/          # Aplikacja okienkowa w C++ / Qt 6 (wczytywanie obrazów, walidacja 4K, stoper)
├── SobelCPP/          # Biblioteka DLL (C++) – referencyjna implementacja skalarna
├── SobelASM/          # Biblioteka DLL (x86-64 MASM) – implementacja wektorowa SIMD (AVX2) i skalarna
└── bin/               # Wspólny katalog wyjściowy dla plików wykonywalnych (.exe i .dll)
```

---

## ⚙️ Wymagania i kompilacja

1. **Środowisko:** Visual Studio 2022 z zainstalowanym pakietem *Desktop development with C++* (zawierającym asembler `ml64.exe`).
2. **Framework GUI:** Qt 6.x (dla kompilatora MSVC 64-bit) oraz rozszerzenie *Qt Visual Studio Tools*.
3. **Procesor:** Architektura x86-64 z obsługą zestawu instrukcji **AVX2**.

### Szybkie uruchomienie
1. Sklonuj repozytorium:
   ```bash
   git clone https://github.com/TWOJ_NICK/JA-Sobel-SIMD.git
   ```
2. Otwórz plik `JA_Sobel.sln` w Visual Studio 2022.
3. Ustaw konfigurację na **`Release`** oraz platformę na **`x64`**.
4. Skompiluj całe rozwiązanie (`Ctrl + Shift + B`) i uruchom projekt startowy `SobelGUI` (`F5`).

---

## 📊 Porównanie wydajności (Benchmark)

Pomiary wykonywane są wyłącznie dla samego czasu przetwarzania buforów w pamięci RAM (z pominięciem operacji dyskowych I/O), uśrednione z $N$ powtórzeń dla czterech standardowych rozdzielczości:

| Rozdzielczość | Liczba pikseli | C++ Skalarny [ms] | MASM Skalarny [ms] | MASM SIMD (AVX2) [ms] | Przyspieszenie (C++ / SIMD) |
| :--- | ---: | :---: | :---: | :---: | :---: |
| **640 × 480 (VGA)** | $307\,200$ | *TODO* | *TODO* | *TODO* | *TODO* |
| **1280 × 720 (HD)** | $921\,600$ | *TODO* | *TODO* | *TODO* | *TODO* |
| **1920 × 1080 (FHD)** | $2\,073\,600$ | *TODO* | *TODO* | *TODO* | *TODO* |
| **3840 × 2160 (4K)** | $8\,294\,400$ | *TODO* | *TODO* | *TODO* | *TODO* |

---

## 👩‍💻 Autorka

* **Weronika Michalska** 
