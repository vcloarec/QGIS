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
#include <QThread>

#include "qgsdatasourceuri.h"


class QgsSqlDatabaseConnection: public QObject
{
    Q_OBJECT
  public:

  public slots:
};


class QgsSqlOdbcProxyResult: public QSqlResult
{
  public:

    QVariant data( int i );
    bool isNull( int i );
    bool reset( const QString &sqlquery );
    bool fetch( int i );
    bool fetchFirst();
    bool fetchLast();
    int size();
    int numRowsAffected();
};

class QgsSqlOdbcProxyDriver : public QSqlDriver
{
  public:
    QgsSqlOdbcProxyDriver( QObject *parent = nullptr ):
      QSqlDriver( parent )
      , mConnection( new QgsSqlDatabaseConnection )
    {
      mConnectionThread = new QThread( this );
      mConnection->moveToThread( mConnectionThread );
      connect( mConnectionThread, &QThread::finished, mConnection, &QgsSqlDatabaseConnection::deleteLater );
    }

    bool hasFeature( DriverFeature f ) const;
    void close();
    QSqlResult *createResult() const;
    bool open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts )
    {
      QgsDataSourceUri uri;
      uri.setConnection( host, QString::number( port ), db, user, password );

      QString connectionName = uri.connectionInfo();
    }

  private:
    QThread *mConnectionThread;
    QgsSqlDatabaseConnection *mConnection;

    static QMap<QString, QgsSqlOdbcProxyDriver *> sOpenedConnections;
};

#endif // QGSSQLODBCPROXYDRIVER_H
