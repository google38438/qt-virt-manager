#ifndef DNS_WIDGET_H
#define DNS_WIDGET_H

#include "forwarder.h"

class DNS_Widget : public _Checked_Widget
{
    Q_OBJECT
public:
    DNS_Widget(
            QWidget *parent = NULL,
            QString  tag = "DNS");

private:
    QCheckBox       *forwardPlainNames;
    Forwarder       *forwarder;

public slots:
    QDomDocument     getDataDocument() const;
};

#endif // DNS_WIDGET_H