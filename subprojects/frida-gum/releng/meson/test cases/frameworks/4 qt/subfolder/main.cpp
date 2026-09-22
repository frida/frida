#include <QImage>
#include <QFile>
#include <QString>

int main(int argc, char **argv) {
  #ifndef UNITY_BUILD
  Q_INIT_RESOURCE(stuff3);
  Q_INIT_RESOURCE(stuff4);
  #endif

  for(auto fname:{":/thing.png", ":/thing4.png"})
  {
    QImage img1(fname);
    if(img1.width() != 640) {
        return 1;
    }
  }

  for(auto fname:{":/txt_resource.txt",":/txt_resource2.txt"})
  {
    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return 1;
    QString line = file.readLine();
    if(line.compare("Hello World"))
      return 1;
  }
  return 0;
}