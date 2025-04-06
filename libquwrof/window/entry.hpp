#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

private slots:
  void openFileDialog();

private:
  QPushButton* filePickerButton;
  QLabel*      filePathLabel;
  QLabel*      titleLabel;
  QLabel*      descriptionLabel;
  QWidget*     centralWidget;
};
