#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SobelGUI.h"

class SobelGUI : public QMainWindow
{
    Q_OBJECT

public:
    SobelGUI(QWidget *parent = nullptr);
    ~SobelGUI();

private:
    Ui::SobelGUIClass ui;
};

