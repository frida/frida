#include"manualinclude.h"
#include <mocdep.h>
#include<QCoreApplication>

#include<QObject>

ManualInclude::ManualInclude() {
}

void ManualInclude::myslot(void) {
    ;
}

class MocClass : public QObject {
    Q_OBJECT
};

int main(int argc, char **argv) {
    ManualInclude mi;
    MocClass mc;
    QObject::connect(&mi, SIGNAL(mysignal(void)),
                     &mi, SLOT(myslot(void)));
    emit mi.mysignal();
    return 0;
}

#include"manualinclude.moc"
