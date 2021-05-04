/***************************************************************************
  qgssqlodbcproxydriver.h - QgsSqlOdbcProxyDriver

 ---------------------
 begin                : 3.5.2021
 copyright            : (C) 2021 by Vincent Cloarec
 email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSSQLODBCPROXYDRIVER_H
#define QGSSQLODBCPROXYDRIVER_H

#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlDriverPlugin>
#include <QThread>
#include <QDebug>
#include <QSqlDatabase>
#include <QMutexLocker>

#include "qgsdatasourceuri.h"


class QgsSqlDatabaseTransaction: public QObject
{
    Q_OBJECT
  public:
    // ************** NOT IMPLEMENTED ***************
    QgsSqlDatabaseTransaction( const QgsDataSourceUri &uri ) {}
    bool hasFeature( QSqlDriver::DriverFeature f ) const {return false;}

    bool isOpen() {return true;}
    bool commit()  {return true;}
    bool rollBack() {return true;}

};

class QgsSqlOdbcProxyResult: public QSqlResult
{
  public:
    // ************** NOT IMPLEMENTED ***************
    QVariant data( int i ) {return QVariant();}
    bool isNull( int i ) {return false;}
    bool reset( const QString &sqlquery ) {return false;}
    bool fetch( int i ) {return false;}
    bool fetchFirst() {return false;}
    bool fetchLast() {return false;}
    int size() {return 0;}
    int numRowsAffected() {return 0;}
};


class QgsSqlOdbcProxyDriver : public QSqlDriver
{
  public:
// ************** PARTIALLY IMPLEMENTED ***************
    QgsSqlOdbcProxyDriver( QObject *parent = nullptr );
    ~QgsSqlOdbcProxyDriver();

    bool hasFeature( DriverFeature f ) const override;
    void close() {}
    QSqlResult *createResult() const {return nullptr;}
    bool open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts ) override;

    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;

  private:
    QgsDataSourceUri mUri;
    QString mConnectionOption;
    QgsSqlDatabaseTransaction *mTransaction = nullptr;
    QSqlDatabase mODBCDatabase;

    static QMap<QString, QgsSqlDatabaseTransaction *> sOpenedTransaction;

    static QString threadedConnectionName( const QString &name );
    void initOdbcDatabase();
};

class QgsSqlOdbcProxyPlugin : public QSqlDriverPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA( IID "org.qt-project.Qt.QSqlDriverFactoryInterface" FILE "qgsodbcproxy.json" )

  public:
    QgsSqlOdbcProxyPlugin( QObject *parent = nullptr );
    QSqlDriver *create( const QString &key ) override;
};

#endif // QGSSQLODBCPROXYDRIVER_H
