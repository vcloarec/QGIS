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


QgsMssqlDataBaseConnectionBase::QgsMssqlDataBaseConnectionBase( QObject *parent ):
  QObject( parent )
{}

QgsMssqlQuery QgsMssqlDataBaseConnectionBase::createQuery()
{
  return QgsMssqlQuery( this );
}

QString QgsMssqlDataBaseConnection::createQueryPrivate()
{
  QString uid = QUuid::createUuid().toString();
  mQueries[uid] = QSharedPointer<QSqlQuery>( new QSqlQuery( mDatabaseConnection ) );
  return uid;
}

void QgsMssqlDataBaseConnection::removeQuery( const QString &uid )
{
  if ( mQueries.contains( uid ) )
    mQueries.remove( uid );
}

bool QgsMssqlDataBaseConnection::executeQuery( const QString &uid, const QString &query )
{
  if ( mQueries.contains( uid ) )
    return  mQueries.value( uid )->exec( query );

  return false;
}

QVariant QgsMssqlDataBaseConnection::queryValue( const QString &uid, int index ) const
{
  if ( mQueries.contains( uid ) )
    return mQueries.value( uid )->value( index );
  return QVariant();
}


bool QgsMssqlDataBaseConnection::isQueryActive( const QString &uid ) const
{
  if ( mQueries.contains( uid ) )
    return mQueries.value( uid )->isActive();
  return false;
}

bool QgsMssqlDataBaseConnection::queryNext( const QString &uid )
{
  if ( mQueries.contains( uid ) )
    return mQueries.value( uid )->next();
  return false;
}

void QgsMssqlDataBaseConnection::initConnection()
{
  mDatabaseConnection = QgsMssqlConnection::getDatabaseConnection( mUri, mUri.connectionInfo() );
  mDatabaseConnection.open();
}

QgsMssqlDataBaseConnection::QgsMssqlDataBaseConnection( const QgsDataSourceUri &uri, QObject *parent ):
  QgsMssqlDataBaseConnectionBase( parent )
  , mUri( uri )
{}

bool QgsMssqlDataBaseConnection::beginTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.transaction();
}

bool QgsMssqlDataBaseConnection::commitTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.commit();
}

bool QgsMssqlDataBaseConnection::rollbackTransaction()
{
  mQueries.clear();
  return mDatabaseConnection.rollback();
}

bool QgsMssqlDataBaseConnection::isOpen() const
{
  return mDatabaseConnection.isOpen();
}

//******************************

QgsMssqlSharableConnection::QgsMssqlSharableConnection( const QgsDataSourceUri &uri, QObject *parent ):
  QgsMssqlDataBaseConnectionBase( parent )
  , mConnection( new QgsMssqlDataBaseConnection( uri ) )
{
  mConnectionThread = new QThread( this );
  mConnection->moveToThread( mConnectionThread );
  connect( mConnectionThread, &QThread::started, mConnection, &QgsMssqlDataBaseConnection::initConnection );
  connect( mConnectionThread, &QThread::finished, mConnection, &QgsMssqlDataBaseConnection::deleteLater );
}

QgsMssqlSharableConnection::~QgsMssqlSharableConnection()
{
  mConnectionThread->quit();
  mConnectionThread->wait();
}

void QgsMssqlSharableConnection::initConnection()
{
  QMutexLocker locker( &mMutex );
  if ( !mConnectionThread->isRunning() )
    mConnectionThread->start();
}

bool QgsMssqlSharableConnection::beginTransaction()
{
  QMutexLocker locker( &mMutex );
  bool success = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

bool QgsMssqlSharableConnection::commitTransaction()
{
  QMutexLocker locker( &mMutex );
  bool success = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "commitTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

bool QgsMssqlSharableConnection::rollbackTransaction()
{
  QMutexLocker locker( &mMutex );
  bool success = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "rollbackTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ) );
  return success;
}

bool QgsMssqlSharableConnection::isOpen() const
{
  QMutexLocker locker( &mMutex );
  bool open = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "isOpen", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, open ) );
  return open;
}


QString QgsMssqlSharableConnection::createQueryPrivate()
{
  QMutexLocker locker( &mMutex );
  if ( !mConnection && mConnectionThread->isRunning() )
    return QString();
  QString uid;
  QMetaObject::invokeMethod( mConnection, "createQueryPrivate", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, uid ) );
  return uid;
}

void QgsMssqlSharableConnection::removeQuery( const QString &queryId )
{
  QMutexLocker locker( &mMutex );
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "removeQuery", Qt::BlockingQueuedConnection, Q_ARG( QString, queryId ) );
}


bool QgsMssqlSharableConnection::executeQuery( const QString &query, const QString &uid )
{
  QMutexLocker locker( &mMutex );
  bool success = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "executeQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ), Q_ARG( QString, query ), Q_ARG( QString, uid ) );
  return success;
}

bool QgsMssqlSharableConnection::isQueryActive( const QString &queryId ) const
{
  QMutexLocker locker( &mMutex );
  bool active = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "isQueryActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, active ), Q_ARG( QString, queryId ) );
  return active;
}

bool QgsMssqlSharableConnection::queryNext( const QString &uid )
{
  QMutexLocker locker( &mMutex );
  bool success = false;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "queryNext", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, success ), Q_ARG( QString, uid ) );
  return success;
}

QVariant QgsMssqlSharableConnection::queryValue( const QString &queryId, int index ) const
{
  QMutexLocker locker( &mMutex );
  QVariant value;
  if ( mConnection && mConnectionThread->isRunning() )
    QMetaObject::invokeMethod( mConnection, "queryValue", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, value ), Q_ARG( QString, queryId ), Q_ARG( int, index ) );
  return value;
}

QgsMssqlQuery::QgsMssqlQuery( QgsMssqlDataBaseConnectionBase *connection ):
  mConnection( connection )
{
  if ( !mConnection.isNull() )
    mUid = mConnection->createQueryPrivate();
}

QgsMssqlQuery::~QgsMssqlQuery()
{
  if ( !mConnection.isNull() )
    mConnection->removeQuery( mUid );
}

bool QgsMssqlQuery::exec( const QString &query )
{
  if ( mConnection.isNull() )
    return false;
  return mConnection->executeQuery( mUid, query );
}

bool QgsMssqlQuery::next()
{
  if ( mConnection.isNull() )
    return false;
  return mConnection->queryNext( mUid );
}

bool QgsMssqlQuery::isActive()
{
  if ( mConnection.isNull() )
    return false;
  return mConnection->isQueryActive( mUid );
}

QVariant QgsMssqlQuery::value( int index )
{
  if ( !mConnection.isNull() )
    return mConnection->queryValue( mUid, index );
  return QVariant();
}

