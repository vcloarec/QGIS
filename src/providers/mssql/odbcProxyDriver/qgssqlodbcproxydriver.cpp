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

QMap<QString, QgsSqlDatabaseTransaction *> QgsSqlOdbcProxyDriver::sOpenedTransaction;

QgsSqlOdbcProxyDriver::QgsSqlOdbcProxyDriver( QObject *parent ):
  QSqlDriver( parent )
{
}

QgsSqlOdbcProxyDriver::~QgsSqlOdbcProxyDriver() {}

bool QgsSqlOdbcProxyDriver::hasFeature( QSqlDriver::DriverFeature f ) const
{
  if ( mTransaction )
    return mTransaction->hasFeature( f );
  else if ( mODBCDatabase.driver() )
    return mODBCDatabase.driver()->hasFeature( f );
  else
    return false;
}

bool QgsSqlOdbcProxyDriver::open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts )
{
  mUri.setConnection( host, QString::number( port ), db, user, password );
  mConnectionOption = connOpts;

  QString connectionId = mUri.connectionInfo();
  if ( sOpenedTransaction.contains( connectionId ) )
  {
    mTransaction = sOpenedTransaction.value( connectionId );
    return true;
  }

  initOdbcDatabase();
  return mODBCDatabase.open();
}

bool QgsSqlOdbcProxyDriver::beginTransaction()
{
  if ( mTransaction )
    return true;

  QString connectionId = mUri.connectionInfo();
  if ( sOpenedTransaction.contains( mUri.connectionInfo() ) )
  {
    mTransaction = sOpenedTransaction.value( connectionId );
    return true;
  }

  mODBCDatabase.close();

  std::unique_ptr<QgsSqlDatabaseTransaction> transaction = std::make_unique<QgsSqlDatabaseTransaction>( mUri );
  if ( transaction->isOpen() )
  {
    mTransaction = transaction.release();
    sOpenedTransaction.insert( connectionId, mTransaction );
    return true;
  }
  else
    return false;
}

bool QgsSqlOdbcProxyDriver::commitTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->commit();

  mTransaction->deleteLater();
  mTransaction = nullptr;

  initOdbcDatabase();

  return result;
}

bool QgsSqlOdbcProxyDriver::rollbackTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->rollBack();

  mTransaction->deleteLater();
  mTransaction = nullptr;

  initOdbcDatabase();

  return result;
}



QString QgsSqlOdbcProxyDriver::threadedConnectionName( const QString &name )
{
  // Starting with Qt 5.11, sharing the same connection between threads is not allowed.
  // We use a dedicated connection for each thread requiring access to the database,
  // using the thread address as connection name.
  return QStringLiteral( "%1:0x%2" ).arg( name ).arg( reinterpret_cast<quintptr>( QThread::currentThread() ), 2 * QT_POINTER_SIZE, 16, QLatin1Char( '0' ) );
}

void QgsSqlOdbcProxyDriver::initOdbcDatabase()
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
    mODBCDatabase.setConnectOptions( mConnectionOption );
  }

}
