#ifndef MANUALINCLUDE_H_
#define MANUALINCLUDE_H_

#include<QObject>

class ManualInclude : public QObject {
    Q_OBJECT

public:
    ManualInclude();
#if defined(MOC_EXTRA_FLAG)
public slots:
#endif
    void myslot(void);

#if defined(MOC_EXTRA_FLAG)
signals:
#endif
    int mysignal();
};

#endif
