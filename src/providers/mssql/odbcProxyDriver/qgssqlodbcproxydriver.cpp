/***************************************************************************
  qgssqlodbcproxydriver.cpp - QgsSqlOdbcProxyDriver

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
#include "qgssqlodbcproxydriver.h"
#include "qgslogger.h"

#include <QCoreApplication>
#include <QSqlError>

QMap<QString, QgsSqlDatabaseTransaction *> QgsSqlDatabaseTransaction::sOpenedTransaction;

QgsSqlOdbcProxyDriver::QgsSqlOdbcProxyDriver( QObject *parent ):
  QSqlDriver( parent )
{
}

QgsSqlOdbcProxyDriver::~QgsSqlOdbcProxyDriver() {}

bool QgsSqlOdbcProxyDriver::hasFeature( QSqlDriver::DriverFeature f ) const
{
  if ( mTransaction )
    return mTransaction->hasFeature( f );
  else if ( mConnection && mConnection->driver() )
    return mConnection->driver()->hasFeature( f );
  else
    return false;
}

QSqlResult *QgsSqlOdbcProxyDriver::createResult() const
{
  if ( mTransaction )
    return mTransaction->createResult();
  else if ( mConnection )
  {
    return mConnection->createResult( mConnection->driver() );
  }
  return nullptr;
}

bool QgsSqlOdbcProxyDriver::open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts )
{
  mUri.setConnection( host, QString::number( port ), db, user, password );
  mConnectionOption = connOpts;

  if ( QgsSqlDatabaseTransaction::transactionIsOpen( mUri ) )
  {
    mTransaction.reset( QgsSqlDatabaseTransaction::getTransaction( this, mUri, mConnectionOption ) );
    return true;
  }

  mConnection.reset( new QgsSqlODBCDatabaseConnection( mUri, mConnectionOption ) );
  bool open =  mConnection->open();

  setOpen( open );
  setOpenError( !open );
  setLastError( mConnection->lastError() );

  return open;
}

bool QgsSqlOdbcProxyDriver::beginTransaction()
{
  if ( mTransaction )
    return true;

  mTransaction.reset( QgsSqlDatabaseTransaction::getTransaction( this, mUri, mConnectionOption ) );

  if ( mTransaction->isOpen() )
  {
    mConnection.reset();
    return true;
  }

  mTransaction.reset();
  return false;
}

bool QgsSqlOdbcProxyDriver::commitTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->commit();

  mTransaction.reset();

  mConnection.reset( new QgsSqlODBCDatabaseConnection( mUri, mConnectionOption ) );

  return result;
}

bool QgsSqlOdbcProxyDriver::rollbackTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->rollBack();

  mTransaction.reset();

  mConnection.reset( new QgsSqlODBCDatabaseConnection( mUri, mConnectionOption ) );

  return result;
}

QgsSqlOdbcProxyPlugin::QgsSqlOdbcProxyPlugin( QObject *parent ): QSqlDriverPlugin( parent )
{}

QSqlDriver *QgsSqlOdbcProxyPlugin::create( const QString &key )
{
  if ( key == QStringLiteral( "QgsODBCProxy" ) )
    return new QgsSqlOdbcProxyDriver();

  return nullptr;
}

bool QgsSqlODBCDatabaseConnection::open()
{
  QString connectionId = mUri.connectionInfo();
  QString odbcConnectionName = threadedConnectionName( connectionId );
  if ( QSqlDatabase::contains( odbcConnectionName ) )
    mODBCDatabase = QSqlDatabase::database( odbcConnectionName );
  else
  {
    mODBCDatabase = QSqlDatabase::addDatabase( QStringLiteral( "QODBC" ), odbcConnectionName );
    mODBCDatabase.setDatabaseName( mUri.database() );
    mODBCDatabase.setUserName( mUri.username() );
    mODBCDatabase.setPassword( mUri.password() );
    mODBCDatabase.setPort( mUri.port().toInt() );
    mODBCDatabase.setConnectOptions( mConnectionOptions );
  }

  return mODBCDatabase.open();
}


QgsSqlDatabaseTransaction::QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions ): mDriver( driver )
{
  mConnectionId = uri.connectionInfo();
  d = new Data;
  if ( sOpenedTransaction.contains( mConnectionId ) )
  {
    QgsSqlDatabaseTransaction *other = sOpenedTransaction.value( mConnectionId );
    d = other->d;
  }
  else
  {
    d->ref = 1;
    d->connection = new QgsSqlODBCDatabaseConnectionThreadSafe( uri, connectionOptions );
    d->thread = new QThread;
    d->connection->moveToThread( d->thread );
    connect( d->thread, &QThread::started, d->connection, &QgsSqlODBCDatabaseConnection::open );
    connect( d->thread, &QThread::finished, d->connection, &QgsSqlODBCDatabaseConnection::deleteLater );
  }

  d->ref.ref();
}

QgsSqlDatabaseTransaction::QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, QgsSqlDatabaseTransaction *other ): mDriver( driver )
{
  d = other->d;
  d->ref.ref();
}

QgsSqlDatabaseTransaction::~QgsSqlDatabaseTransaction()
{
  d->ref.deref();
  if ( d->ref == 1 )
  {
    if ( d->thread )
    {
      d->thread->quit();
      d->thread->wait();
      d->thread->deleteLater();
      d->thread = nullptr;
    }

    sOpenedTransaction.remove( mConnectionId );
    delete d;
  }
}

QSqlResult *QgsSqlDatabaseTransaction::createResult()
{
  if ( d && d->connection )
    return  new QgsSqlOdbcThreadSafeResult( mDriver, d->connection );
  else
    return nullptr;
}

bool QgsSqlDatabaseTransaction::transactionIsOpen( const QgsDataSourceUri &uri )
{
  QString connectionId = uri.connectionInfo();
  return sOpenedTransaction.contains( connectionId );
}


QgsSqlDatabaseTransaction *QgsSqlDatabaseTransaction::getTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions )
{
  QString connectionId = uri.connectionInfo();
  if ( sOpenedTransaction.contains( connectionId ) )
    return new QgsSqlDatabaseTransaction( driver, sOpenedTransaction.value( connectionId ) );
  else
    return new QgsSqlDatabaseTransaction( driver, uri, connectionOptions );
}

QgsSqlOdbcThreadSafeResult::QgsSqlOdbcThreadSafeResult( QSqlDriver *callerDriver, QgsSqlODBCDatabaseConnectionThreadSafe *connection ):
  QSqlResult( callerDriver )
  , mConnection( connection )
{
  if ( !mConnection.isNull() )
  {

    QString uuid;
    QMetaObject::invokeMethod( mConnection, "createResultPrivate", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, uuid ) );
    mUuid = uuid;
  }
}
