#include <iostream>

#include <QApplication>
#include <QPixmap>

#include "ui/AppIcon.hpp"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  const QIcon icon = cliphist::CreateCliphistIcon();
  if (icon.isNull()) {
    std::cerr << "expected app icon to load successfully\n";
    return 1;
  }

  const QPixmap small = icon.pixmap(16, 16);
  if (small.isNull()) {
    std::cerr << "expected 16x16 app icon pixmap\n";
    return 1;
  }

  const QPixmap medium = icon.pixmap(32, 32);
  if (medium.isNull()) {
    std::cerr << "expected 32x32 app icon pixmap\n";
    return 1;
  }

  return 0;
}
