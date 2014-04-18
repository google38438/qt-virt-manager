#include "domain_control.h"

VirtDomainControl::VirtDomainControl(QWidget *parent) :
    QMainWindow(parent)
{
    setObjectName("VirtDomainControl");
    setWindowTitle("Domain Control");
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setWindowIcon(QIcon::fromTheme("domain"));
    domainModel = new DomainModel();
    domainList = new QTreeView(this);
    domainList->setItemsExpandable(false);
    domainList->setRootIsDecorated(false);
    domainList->setModel(domainModel);
    domainList->setFocus();
    domainList->setContextMenuPolicy(Qt::CustomContextMenu);
    //connect(domainList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(domainDoubleClicked(const QModelIndex&)));
    connect(domainList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(domainClicked(const QPoint&)));
    setCentralWidget(domainList);
    settings.beginGroup("VirtDomainControl");
    domainList->setColumnWidth(0, settings.value("column0", 132).toInt());
    domainList->setColumnWidth(1, settings.value("column1", 32).toInt());
    domainList->setColumnWidth(2, settings.value("column2", 32).toInt());
    domainList->setColumnWidth(3, settings.value("column3", 32).toInt());
    int area_int = settings.value("ToolBarArea", 4).toInt();
    //bool check = settings.value("CheckList", false).toBool();
    interval = settings.value("UpdateTime", 3).toInt();
    settings.endGroup();
    toolBar = new DomainToolBar(this);
    addToolBar(toolBar->get_ToolBarArea(area_int), toolBar);
    connect(toolBar, SIGNAL(fileForMethod(const QStringList&)), this, SLOT(newVirtDomainFromXML(const QStringList&)));
    connect(toolBar, SIGNAL(execMethod(const QStringList&)), this, SLOT(execAction(const QStringList&)));
    domControlThread = new DomControlThread(this);
    connect(domControlThread, SIGNAL(started()), this, SLOT(changeDockVisibility()));
    connect(domControlThread, SIGNAL(finished()), this, SLOT(changeDockVisibility()));
    connect(domControlThread, SIGNAL(resultData(DomActions, QStringList)), this, SLOT(resultReceiver(DomActions, QStringList)));
    connect(domControlThread, SIGNAL(errorMsg(QString)), this, SLOT(msgRepeater(QString)));
}
VirtDomainControl::~VirtDomainControl()
{
    settings.beginGroup("VirtDomainControl");
    settings.setValue("column0", domainList->columnWidth(0));
    settings.setValue("column1", domainList->columnWidth(1));
    settings.setValue("column2", domainList->columnWidth(2));
    settings.setValue("column3", domainList->columnWidth(3));
    //settings.setValue("UpdateTime", docContent->getUpdateTime());
    //settings.setValue("CheckList", docContent->getCheckList());
    settings.setValue("ToolBarArea", toolBarArea(toolBar));
    settings.endGroup();
    settings.sync();
    //disconnect(domainList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(domainDoubleClicked(const QModelIndex&)));
    disconnect(domainList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(domainClicked(const QPoint&)));
    disconnect(toolBar, SIGNAL(fileForMethod(const QStringList&)), this, SLOT(newVirtDomainFromXML(const QStringList&)));
    disconnect(toolBar, SIGNAL(execMethod(const QStringList&)), this, SLOT(execAction(const QStringList&)));
    disconnect(domControlThread, SIGNAL(started()), this, SLOT(changeDockVisibility()));
    disconnect(domControlThread, SIGNAL(finished()), this, SLOT(changeDockVisibility()));
    disconnect(domControlThread, SIGNAL(resultData(DomActions, QStringList)), this, SLOT(resultReceiver(DomActions, QStringList)));
    disconnect(domControlThread, SIGNAL(errorMsg(QString)), this, SLOT(msgRepeater(QString)));

    if ( createVirtDomain!=NULL ) {
        delete createVirtDomain;
        createVirtDomain = 0;
    };

    stopProcessing();
    domControlThread->terminate();
    delete domControlThread;
    domControlThread = 0;

    delete toolBar;
    toolBar = 0;

    if (domainModel!=NULL) {
        delete domainModel;
        domainModel = 0;
    };

    if (domainList!=NULL) {
        delete domainList;
        domainList = 0;
    };
}

/* public slots */
bool VirtDomainControl::getThreadState() const
{
    return domControlThread->isFinished() || !domControlThread->isRunning();
}
void VirtDomainControl::stopProcessing()
{
    if ( timerId ) {
        killTimer(timerId);
        timerId = 0;
    }
    if ( domControlThread!=NULL ) {
        domControlThread->stop();
    };

    if ( currWorkConnect!=NULL ) {
        virConnectClose(currWorkConnect);
        currWorkConnect = NULL;
    };

    // clear Domain list
    while ( domainModel->virtDomDataList.count() ) {
        domainModel->removeRow(0);
    };
    domainModel->setHeaderData(0, Qt::Horizontal, QString("Name"), Qt::EditRole);
    setEnabled(false);

}
bool VirtDomainControl::setCurrentWorkConnect(virConnect *conn)
{
    stopProcessing();
    currWorkConnect = conn;
    int ret = virConnectRef(currWorkConnect);
    if ( ret<0 ) {
        virErrorPtr virtErrors = virGetLastError();
        if ( virtErrors!=NULL ) {
            QString time = QTime::currentTime().toString();
            QString msg = QString("%3 VirtError(%1) : %2").arg(virtErrors->code).arg(virtErrors->message).arg(time);
            emit domMsg( msg );
            virResetError(virtErrors);
        };
        currWorkConnect = NULL;
        return false;
    } else {
        domControlThread->setCurrentWorkConnect(currWorkConnect);
        timerId = startTimer(interval*1000);
        return true;
    };
}
void VirtDomainControl::setListHeader(QString &connName)
{
    domainModel->setHeaderData(0, Qt::Horizontal, QString("Name (Conn: \"%1\")").arg(connName), Qt::EditRole);
    currConnName = connName;
    setEnabled(true);
}
virConnect* VirtDomainControl::getConnect() const
{
    return currWorkConnect;
}

/* private slots */
void VirtDomainControl::timerEvent(QTimerEvent *event)
{
    int _timerId = event->timerId();
    if ( _timerId && timerId==_timerId && isVisible() ) {
        //qDebug()<<"get domain list";
        domControlThread->execAction(GET_ALL_DOMAIN, QStringList());
    };
}
void VirtDomainControl::resultReceiver(DomActions act, QStringList data)
{
    //qDebug()<<act<<data<<"result";
    if ( act == GET_ALL_DOMAIN ) {
        if ( data.count() > domainModel->virtDomDataList.count() ) {
            int _diff = data.count() - domainModel->virtDomDataList.count();
            for ( int i = 0; i<_diff; i++ ) {
                domainModel->insertRow(1);
                //qDebug()<<i<<"insert";
            };
        };
        if ( domainModel->virtDomDataList.count() > data.count() ) {
            int _diff = domainModel->virtDomDataList.count() - data.count();
            for ( int i = 0; i<_diff; i++ ) {
                domainModel->removeRow(0);
                //qDebug()<<i<<"remove";
            };
        };
        int i = 0;
        foreach (QString _data, data) {
            QStringList chain = _data.split(" ");
            if (chain.isEmpty()) continue;
            int count = chain.size();
            for (int j=0; j<count; j++) {
                domainModel->setData(domainModel->index(i,j), chain.at(j), Qt::EditRole);
            };
            i++;
        };
    } else if ( act == CREATE_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == DEFINE_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == START_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == PAUSE_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == DESTROY_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == RESET_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == REBOOT_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == SHUTDOWN_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == SAVE_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == UNDEFINE_DOMAIN ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == CHANGE_DOM_AUTOSTART ) {
        if ( !data.isEmpty() ) msgRepeater(data.join(" "));
    } else if ( act == GET_DOM_XML_DESC ) {
        if ( !data.isEmpty() ) {
            QString xml = data.first();
            data.removeFirst();
            data.append(QString("in <a href='%1'>%1</a>").arg(xml));
            msgRepeater(data.join(" "));
            QDesktopServices::openUrl(QUrl(xml));
        };
    };
}
void VirtDomainControl::msgRepeater(QString msg)
{
    QString time = QTime::currentTime().toString();
    QString title = QString("Connect '%1'").arg(currConnName);
    QString errorMsg = QString("<b>%1 %2:</b><br>%3").arg(time).arg(title).arg(msg);
    emit domMsg(errorMsg);
}
void VirtDomainControl::changeDockVisibility()
{
    toolBar->setEnabled( !toolBar->isEnabled() );
    domainList->setEnabled( !domainList->isEnabled() );
}

void VirtDomainControl::domainClicked(const QPoint &p)
{
    //qDebug()<<"custom Menu request";
    QModelIndex idx = domainList->indexAt(p);
    if ( idx.isValid() ) {
        //qDebug()<<domainModel->virtDomDataList.at(idx.row())->getName();
        QStringList params;
        params<<domainModel->virtDomDataList.at(idx.row())->getName();
        params<<domainModel->virtDomDataList.at(idx.row())->getState().split(":").first();
        params<<domainModel->virtDomDataList.at(idx.row())->getAutostart();
        params<<domainModel->virtDomDataList.at(idx.row())->getPersistent();
        DomainControlMenu *domControlMenu = new DomainControlMenu(this, params);
        connect(domControlMenu, SIGNAL(execMethod(const QStringList&)), this, SLOT(execAction(const QStringList&)));
        domControlMenu->move(QCursor::pos());
        domControlMenu->exec();
        disconnect(domControlMenu, SIGNAL(execMethod(const QStringList&)), this, SLOT(execAction(const QStringList&)));
        domControlMenu->deleteLater();
    } else {
        domainList->clearSelection();
    }
}
void VirtDomainControl::domainDoubleClicked(const QModelIndex &index)
{
    if ( index.isValid() ) {
        qDebug()<<domainModel->virtDomDataList.at(index.row())->getName();
    }
}
void VirtDomainControl::execAction(const QStringList &l)
{
    QStringList args;
    QModelIndex idx = domainList->currentIndex();
    if ( idx.isValid() ) {
        QString domainName = domainModel->virtDomDataList.at(idx.row())->getName();
        args.append(domainName);
        if        ( l.first()=="startVirtDomain" ) {
            domControlThread->execAction(START_DOMAIN, args);
        } else if ( l.first()=="pauseVirtDomain" ) {
            args.append(domainModel->virtDomDataList.at(idx.row())->getState().split(":").last());
            domControlThread->execAction(PAUSE_DOMAIN, args);
        } else if ( l.first()=="destroyVirtDomain" ) {
            domControlThread->execAction(DESTROY_DOMAIN, args);
        } else if ( l.first()=="resetVirtDomain" ) {
            domControlThread->execAction(RESET_DOMAIN, args);
        } else if ( l.first()=="rebootVirtDomain" ) {
            domControlThread->execAction(REBOOT_DOMAIN, args);
        } else if ( l.first()=="shutdownVirtDomain" ) {
            domControlThread->execAction(SHUTDOWN_DOMAIN, args);
        } else if ( l.first()=="saveVirtDomain" ) {
            QString to = QFileDialog::getSaveFileName(this, "Save to", "~");
            if ( !to.isEmpty() ) {
                args.append(to);
                args.append(domainModel->virtDomDataList.at(idx.row())->getState().split(":").last());
                domControlThread->execAction(SAVE_DOMAIN, args);
            };
        } else if ( l.first()=="restoreVirtDomain" ) {
            QString from = QFileDialog::getOpenFileName(this, "Restore from", "~");
            if ( !from.isEmpty() ) {
                args.append(from);
                // TODO: get restore domain state
                domControlThread->execAction(RESTORE_DOMAIN, args);
            };
        } else if ( l.first()=="undefineVirtDomain" ) {
            domControlThread->execAction(UNDEFINE_DOMAIN, args);
        } else if ( l.first()=="setAutostartVirtDomain" ) {
            /* set the opposite value */
            QString autostartState =
                (domainModel->virtDomDataList.at(idx.row())->getAutostart()=="yes")
                 ? "0" : "1";
            args.append(autostartState);
            domControlThread->execAction(CHANGE_DOM_AUTOSTART, args);
        } else if ( l.first()=="getVirtDomXMLDesc" ) {
            domControlThread->execAction(GET_DOM_XML_DESC, args);
        };
    }
}
void VirtDomainControl::newVirtDomainFromXML(const QStringList &_args)
{
    DomActions act;
    if ( !_args.isEmpty() ) {
        if ( _args.first().startsWith("create") ) act = CREATE_DOMAIN;
        else act = DEFINE_DOMAIN;
        QStringList args = _args;
        args.removeFirst();
        if ( !args.isEmpty() ) {
            if ( args.first()=="manually" ) {
                args.removeFirst();
                //QString source = args.first();
                args.removeFirst();
                QString capabilities, xml;
                // show SRC Creator widget
                capabilities = QString("%1").arg(virConnectGetCapabilities(currWorkConnect));
                createVirtDomain = new CreateVirtDomain(this, QString("%1").arg(virConnectGetType(currWorkConnect)));
                int result = createVirtDomain->exec();
                if ( createVirtDomain!=NULL && result ) {
                    // get path for method
                    xml = createVirtDomain->getXMLDescFileName();
                    QStringList data;
                    data.append("New Domain XML'ed");
                    data.append(QString("in <a href='%1'>%1</a>").arg(xml));
                    msgRepeater(data.join(" "));
                    QDesktopServices::openUrl(QUrl(xml));
                };
                delete createVirtDomain;
                createVirtDomain = 0;
                //qDebug()<<xml<<"path"<<result;
                args.prepend(xml);
            };
            domControlThread->execAction(act, args);
        };
    };
}
