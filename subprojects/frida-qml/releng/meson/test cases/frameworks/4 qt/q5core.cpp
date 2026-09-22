#include <QCoreApplication>
#include <QtGlobal>
#include <QString>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QDebug>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);

  QTranslator qtTranslator;
  qtTranslator.load("qt_" + QLocale::system().name(),
                  QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  app.installTranslator(&qtTranslator);

  QTranslator myappTranslator;
  if(!myappTranslator.load(QT "core_fr") )
    return 1;

  app.installTranslator(&myappTranslator);

  qDebug() << QObject::tr("Translate me!");
  // Don't actually start the main loop so this
  // can be run as a unit test.
  //return app.exec();
  return 0;
}
