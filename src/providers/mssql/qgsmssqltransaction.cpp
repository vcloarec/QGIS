/***************************************************************************
  qgsmssqltransaction.cpp - QgsMssqlTransaction

 ---------------------
 begin                : 11.3.2021
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
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>
#include <QDebug>
#include <QUuid>

#include "qgsmssqltransaction.h"
#include "qgsmssqlconnection.h"
#include "qgsdatasourceuri.h"
#include "qgsmessagelog.h"


QgsMssqlTransaction::QgsMssqlTransaction( const QString &connString ): QgsTransaction( connString )
{
  QgsDataSourceUri uri( connString );

  mDataBase = QgsMssqlConnection::getDatabaseConnection( uri, connString + QStringLiteral( "-transaction" ) );
}

QgsMssqlTransaction::~QgsMssqlTransaction()
{
  if ( QSqlDatabase::contains( mDataBase.connectionName() ) )
    QSqlDatabase::removeDatabase( mDataBase.connectionName() );
}

bool QgsMssqlTransaction::executeSql( const QString &sql, QString &error, bool isDirty, const QString &name )
{
  if ( !mDataBase.isValid() )
    return false;

  if ( !mDataBase.isOpen() )
    mDataBase.open();

  if ( !mDataBase.isOpen() )
    return false;

  if ( isDirty )
  {
    QgsTransaction::createSavepoint( error );
    if ( ! error.isEmpty() )
    {
      return false;
    }
  }

  QSqlQuery query( mDataBase );
  if ( !query.exec( sql ) )
  {
    if ( isDirty )
      rollbackToSavepoint( savePoints().last(), error );

    QString msg = tr( "MSSQL query failed: %1" ).arg( query.lastError().text() );
    qDebug() << msg;
    if ( error.isEmpty() )
      error = msg;
    else
      error = QStringLiteral( "%1\n%2" ).arg( error, msg );

    return false;
  }

  if ( isDirty )
  {
    dirtyLastSavePoint();
    emit dirtied( sql, name );
  }

  return true;
}

QString QgsMssqlTransaction::createSavepoint( const QString &savePointId, QString &error )
{
  if ( !mTransactionActive )
    return QString();

  if ( !executeSql( QStringLiteral( "SAVE TRAN %1" ).arg( QgsExpression::quotedColumnRef( savePointId ) ), error ) )
  {
    QgsMessageLog::logMessage( tr( "Could not create savepoint (%1)" ).arg( error ) );
    return QString();
  }

  mSavepoints.push( savePointId );
  mLastSavePointIsDirty = false;
  return savePointId;
}

QSqlQuery QgsMssqlTransaction::createQuery()
{
  if ( !mDataBase.isOpen() )
  {
    if ( !QgsMssqlConnection::openDatabase( mDataBase ) )
    {
      QgsDataSourceUri uri = mConnString;
      mDataBase = QgsMssqlConnection::getDatabaseConnection( uri, mConnString );
    }

    QgsMssqlConnection::openDatabase( mDataBase );
    if ( !mDataBase.isOpen() )
      return QSqlQuery();
  }

  return QSqlQuery( mDataBase );
}

bool QgsMssqlTransaction::transactionCommand( const QString command, QString &error )
{
  if ( mDataBase.isValid() )
  {
    if ( !mDataBase.isOpen() )
      mDataBase.open();

    if ( mDataBase.isOpen() )
    {
      if ( executeSql( command, error ) )
        return true;
    }
  }

  QString msg = tr( "Unable to %1 transaction (MSSQL)" ).arg( command );
  if ( error.isEmpty() )
    error = msg;
  else
    error = QStringLiteral( "%1\n%2" ).arg( error, msg );

  return false;
}

bool QgsMssqlTransaction::beginTransaction( QString &error, int )
{
  if ( mDataBase.isValid() )
  {

    if ( !mDataBase.isOpen() )
      mDataBase.open();

    if ( mDataBase.isOpen() )
    {

      if ( !mDataBase.driver()->hasFeature( QSqlDriver::Transactions ) )
        return false;

      if ( mDataBase.transaction() )
      {
        return true;
      }
    }
  }
  error = mDataBase.lastError().text();
  return false;
}

bool QgsMssqlTransaction::commitTransaction( QString &error )
{
  if ( mDataBase.isValid() && mDataBase.isOpen() )
  {
    mDataBase.commit();
    return true;
  }

  return false;
}

bool QgsMssqlTransaction::rollbackTransaction( QString &error )
{
  if ( mDataBase.isValid() && mDataBase.isOpen() )
  {
    mDataBase.rollback();
    return true;
  }

  return false;

}

//****************************** thread safe connection wrapper

QString QgsMssqlConnectionWrapper::createQuery()
{
  QString uid = QUuid::createUuid().toString();
  mQueries[uid] = QSharedPointer<QSqlQuery>( new QSqlQuery( mDatabaseConnection ) );
  return uid;
}

void QgsMssqlConnectionWrapper::removeQuery( const QString &uid )
{
  if ( mQueries.contains( uid ) )
    mQueries.remove( uid );
}

bool QgsMssqlConnectionWrapper::executeQuery( const QString &uid, const QString &query )
{
  if ( mQueries.contains( uid ) )
    return mQueriesSuccesful[uid] = mQueries.value( uid )->exec( query );

  return false;
}

QVariant QgsMssqlConnectionWrapper::queryValue( const QString &uid, int index ) const
{
  if ( !mQueries.contains( uid ) )
    return QVariant();
  return mQueries.value( uid )->value( index );
}


bool QgsMssqlConnectionWrapper::isQueryActive( const QString &uid ) const
{
  if ( !mQueries.contains( uid ) )
    return mQueries.value( uid )->isActive();
  return false;
}

QgsMssqlConnectionWrapper::QgsMssqlConnectionWrapper( const QgsDataSourceUri &uri ) :
  QObject()
  , mUri( uri )
{}

void QgsMssqlConnectionWrapper::createConnection()
{
  mDatabaseConnection = QgsMssqlConnection::getDatabaseConnection( mUri, mUri.connectionInfo() );
}

bool QgsMssqlConnectionWrapper::beginTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.transaction();
}

bool QgsMssqlConnectionWrapper::commitTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.commit();
}

bool QgsMssqlConnectionWrapper::rollbackTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.rollback();
}

//******************************

QgsMssqlThreadSafeConnection::QgsMssqlThreadSafeConnection( const QgsDataSourceUri &uri ):
  mConnection( new QgsMssqlConnectionWrapper( uri ) )
{
  mTransactionThread = new QThread;
  mConnection->moveToThread( mTransactionThread );
  connect( mTransactionThread, &QThread::started, mConnection, &QgsMssqlConnectionWrapper::createConnection );
  connect( mTransactionThread, &QThread::finished, mConnection, &QgsMssqlConnectionWrapper::deleteLater );
  mTransactionThread->start();
}

QgsMssqlThreadSafeConnection::~QgsMssqlThreadSafeConnection()
{
  mTransactionThread->quit();
  mTransactionThread->wait();
}

bool QgsMssqlThreadSafeConnection::beginTransaction()
{
  bool success = false;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

bool QgsMssqlThreadSafeConnection::commitTransaction()
{
  bool success = false;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "commitTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

bool QgsMssqlThreadSafeConnection::rollbackTransaction()
{
  bool success = false;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "rollbackTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

QgsMssqlThreadSafeConnection::Query QgsMssqlThreadSafeConnection::createQuery()
{
  return QgsMssqlThreadSafeConnection::Query( this );
}


QString QgsMssqlThreadSafeConnection::createQueryPrivate()
{
  if ( !mConnection )
    return QString();
  QString uid;
  QMetaObject::invokeMethod( mConnection, "createQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, uid ) );
  return uid;
}

void QgsMssqlThreadSafeConnection::removeQuery( const QString &queryId )
{
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "removeQuery", Qt::BlockingQueuedConnection, Q_ARG( QString, queryId ) );
}


bool QgsMssqlThreadSafeConnection::executeQuery( const QString &query, const QString &uid )
{
  bool success = false;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "executeQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ), Q_ARG( QString, query ), Q_ARG( QString, uid ) );
  return success;
}

bool QgsMssqlThreadSafeConnection::queryIsActive( const QString &queryId ) const
{
  bool active = false;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "isQueryActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, active ), Q_ARG( QString, queryId ) );
  return false;
}

QVariant QgsMssqlThreadSafeConnection::queryValue( const QString &queryId, int index ) const
{
  QVariant value;
  if ( mConnection )
    QMetaObject::invokeMethod( mConnection, "queryValue", Qt::BlockingQueuedConnection, Q_ARG( QString, queryId ), Q_ARG( int, index ) );
  return value;
}

QgsMssqlThreadSafeConnection::Query::Query( QgsMssqlThreadSafeConnection *connection ):
  mConnection( connection )
{
  if ( !mConnection.isNull() )
    mUid = mConnection->createQueryPrivate();
}

QgsMssqlThreadSafeConnection::Query::~Query()
{
  if ( !mConnection.isNull() )
    mConnection->removeQuery( mUid );
}

bool QgsMssqlThreadSafeConnection::Query::exec( const QString query )
{
  if ( mConnection.isNull() )
    return false;
  return mConnection->executeQuery( mUid, query );

}

bool QgsMssqlThreadSafeConnection::Query::isActive()
{
  if ( mConnection.isNull() )
    return false;
  return mConnection->queryIsActive( mUid );
}

QVariant QgsMssqlThreadSafeConnection::Query::value( int index )
{
  if ( !mConnection.isNull() )
    return mConnection->queryValue( mUid, index );
  return QVariant();
}
