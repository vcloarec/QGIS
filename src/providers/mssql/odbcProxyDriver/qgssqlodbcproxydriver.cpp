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
#include <QSqlQuery>

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

void QgsSqlOdbcProxyDriver::close()
{
  if ( mConnection )
    mConnection->close();

  if ( mTransaction )
    mTransaction->close();

  setOpen( false );
}

QSqlResult *QgsSqlOdbcProxyDriver::createResult() const
{
  if ( mTransaction )
    return mTransaction->createResult();
  else if ( mConnection )
    return mConnection->createResult( );

  return nullptr;
}

bool QgsSqlOdbcProxyDriver::reopen()
{
  bool open = false;
  if ( QgsSqlDatabaseTransaction::isTransactionExist( mUri ) )
  {
    mTransaction.reset( QgsSqlDatabaseTransaction::getTransaction( this, mUri, mConnectionOption ) );
    open = mTransaction->isOpen();
    setLastError( mTransaction->lastError() );
  }
  else
  {
    mConnection.reset( new QgsSqlODBCDatabaseConnection( mUri, mConnectionOption ) );
    setLastError( mConnection->lastError() );
    open = mConnection->open();
  }

  setOpen( open );
  setOpenError( !open );

  return open;
}

bool QgsSqlOdbcProxyDriver::open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts )
{
  mUri.setConnection( host, QString::number( port ), db, user, password );
  mConnectionOption = connOpts;

  return reopen();
}

bool QgsSqlOdbcProxyDriver::beginTransaction()
{
  if ( mTransaction )
    return true;

  mTransaction.reset( QgsSqlDatabaseTransaction::getTransaction( this, mUri, mConnectionOption ) );

  bool res = mTransaction->isOpen();

  if ( !res )
    mTransaction.reset();

  return res;
}

bool QgsSqlOdbcProxyDriver::commitTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->commit();

  mTransaction->closeAll();
  mTransaction.reset();
  setOpen( false );

  return result;
}

bool QgsSqlOdbcProxyDriver::rollbackTransaction()
{
  if ( !mTransaction )
    return false;

  bool result = mTransaction->rollBack();

  mTransaction->closeAll();
  mTransaction.reset();
  setOpen( false );

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

QgsSqlODBCDatabaseConnection::QgsSqlODBCDatabaseConnection( const QgsDataSourceUri &uri, const QString &connectionOptions, bool marsConnection ):
  mUri( uri ),
  mConnectionOptions( connectionOptions ),
  mIsMarsConnection( marsConnection )
{}

QSqlError QgsSqlODBCDatabaseConnection::lastError() const
{
  return mODBCDatabase.lastError();
}

QSqlResult *QgsSqlODBCDatabaseConnection::createResult()
{
  if ( !mODBCDatabase.isOpen() )
    mODBCDatabase.open();

  if ( mODBCDatabase.driver() )
    return mODBCDatabase.driver()->createResult();

  return nullptr;
}

QSqlDriver *QgsSqlODBCDatabaseConnection::driver() const
{
  return mODBCDatabase.driver();
}

bool QgsSqlODBCDatabaseConnection::open()
{
  QString connectionId = mUri.connectionInfo();
  QString odbcConnectionName = threadedConnectionName( connectionId );
  if ( QSqlDatabase::contains( odbcConnectionName ) )
    mODBCDatabase = QSqlDatabase::database( odbcConnectionName );
  else
  {
    QString connectionString = mUri.database();
    if ( mIsMarsConnection )
      connectionString += QStringLiteral( ";MARS_Connection=yes" );
    mODBCDatabase = QSqlDatabase::addDatabase( QStringLiteral( "QODBC" ), odbcConnectionName );
    mODBCDatabase.setDatabaseName( connectionString );
    mODBCDatabase.setUserName( mUri.username() );
    mODBCDatabase.setPassword( mUri.password() );
    mODBCDatabase.setPort( mUri.port().toInt() );
    mODBCDatabase.setConnectOptions( mConnectionOptions );
  }

  return mODBCDatabase.open();
}

bool QgsSqlODBCDatabaseConnection::isOpen() const
{
  return mODBCDatabase.isOpen();
}

bool QgsSqlODBCDatabaseConnection::hasFeature( QSqlDriver::DriverFeature f ) const
{
  return ( driver() && driver()->hasFeature( f ) );
}

void QgsSqlODBCDatabaseConnection::close()
{
  mODBCDatabase.close();
}

QString QgsSqlODBCDatabaseConnection::threadedConnectionName( const QString &name )
{
  return QStringLiteral( "%1:0x%2" ).arg( name ).arg( reinterpret_cast<quintptr>( QThread::currentThread() ), 2 * QT_POINTER_SIZE, 16, QLatin1Char( '0' ) );
}


QgsSqlDatabaseTransaction::QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions ): mDriver( driver )
{
  mConnectionId = uri.connectionInfo();

  if ( sOpenedTransaction.contains( mConnectionId ) )
  {
    QgsSqlDatabaseTransaction *other = sOpenedTransaction.value( mConnectionId );
    d = other->d;
    d->ref.ref();
  }
  else
  {
    d = new Data;
    d->ref = 1;
    d->connection = new QgsSqlODBCDatabaseTransactionConnection( uri, connectionOptions );
    d->thread = new QThread;
    d->connection->moveToThread( d->thread );
    connect( d->thread, &QThread::started, d->connection, &QgsSqlODBCDatabaseTransactionConnection::open );
    connect( d->thread, &QThread::finished, d->connection, &QgsSqlODBCDatabaseTransactionConnection::deleteLater );
    d->thread->start();

    beginTransaction();
    sOpenedTransaction[mConnectionId] = this;
  }

  d->ref.ref();
}

QgsSqlDatabaseTransaction::QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, QgsSqlDatabaseTransaction *other ): mDriver( driver )
{
  d = other->d;
  d->ref.ref();
  mConnectionId = other->mConnectionId;
}


bool QgsSqlDatabaseTransaction::connectionIsValid() const
{
  return ( d && !d->connection.isNull() && !d->thread.isNull() && d->thread->isRunning() );
}

void QgsSqlDatabaseTransaction::beginTransaction()
{
  if ( !connectionIsValid() )
    return;

  bool ret = false;
  QMetaObject::invokeMethod( d->connection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );

  d->transactionIsStarted = ret;
}


bool QgsSqlDatabaseTransaction::commit()
{
  if ( !connectionIsValid() )
    return false;

  bool ret = false;
  QMetaObject::invokeMethod( d->connection, "commit", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

bool QgsSqlDatabaseTransaction::rollBack()
{
  if ( !connectionIsValid() )
    return false;

  bool ret = false;
  QMetaObject::invokeMethod( d->connection, "rollBack", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

QgsSqlDatabaseTransaction::~QgsSqlDatabaseTransaction()
{
  close();
}

bool QgsSqlDatabaseTransaction::hasFeature( QSqlDriver::DriverFeature f ) const
{
  if ( !d || d->connection.isNull() || ( d->thread.isNull() && !d->thread->isRunning() ) )
    return false;

  bool ret = false;
  QMetaObject::invokeMethod( d->connection, "hasFeature", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QSqlDriver::DriverFeature, f ) );

  return ret;
}

bool QgsSqlDatabaseTransaction::isOpen()
{
  if ( !connectionIsValid() )
    return false;

  bool ret = false;
  QMetaObject::invokeMethod( d->connection, "isOpen", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );

  return ret &&  d->transactionIsStarted;
}


void QgsSqlDatabaseTransaction::stopConnection()
{
  if ( d )
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
    d = nullptr;
  }
}


void QgsSqlDatabaseTransaction::close()
{
  if ( d )
    d->ref.deref();

  if ( d && d->ref == 1 )
    stopConnection();
  else
  {
    d = nullptr;
    mConnectionId = QString();
  }
}

void QgsSqlDatabaseTransaction::closeAll()
{
  stopConnection();
}


QSqlResult *QgsSqlDatabaseTransaction::createResult()
{
  if ( d && d->connection )
    return  new QgsSqlOdbcTransactionResult( mDriver, d->connection, d->thread );
  else
    return nullptr;
}

QSqlError QgsSqlDatabaseTransaction::lastError() const
{
  if ( !connectionIsValid() )
    return QSqlError( QObject::tr( "ODBCTransactionProxy: Invalid Transaction Connection" ), QString(), QSqlError::ConnectionError );

  QSqlError ret;
  QMetaObject::invokeMethod( d->connection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ) );
  return ret;
}

bool QgsSqlDatabaseTransaction::isTransactionExist( const QgsDataSourceUri &uri )
{
  QString connectionId = uri.connectionInfo();
  return sOpenedTransaction.contains( connectionId );
}


QgsSqlDatabaseTransaction *QgsSqlDatabaseTransaction::getTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions )
{
  QString connectionId = uri.connectionInfo();
  if ( ! sOpenedTransaction.contains( connectionId ) )
    sOpenedTransaction[connectionId] = new QgsSqlDatabaseTransaction( driver, uri, connectionOptions );

  return new QgsSqlDatabaseTransaction( driver, sOpenedTransaction.value( connectionId ) );
}

bool QgsSqlOdbcTransactionResult::connectionIsValid() const
{
  return !mConnection.isNull() && !mThread.isNull() && mThread->isRunning();
}

QgsSqlOdbcTransactionResult::QgsSqlOdbcTransactionResult( QSqlDriver *callerDriver, QgsSqlODBCDatabaseTransactionConnection *connection, QThread *thread ):
  QSqlResult( callerDriver )
  , mConnection( connection )
  , mThread( thread )
{
  if ( connectionIsValid() )
  {
    QString uuid;
    QMetaObject::invokeMethod( mConnection, "createTransactionResult", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, uuid ) );
    mUuid = uuid;
  }
  setLastError( lastErrorTransactionResult() );
}

QgsSqlOdbcTransactionResult::~QgsSqlOdbcTransactionResult()
{
  if ( connectionIsValid() )
  {
    QMetaObject::invokeMethod( mConnection, "removeTransactionResult", Qt::BlockingQueuedConnection, Q_ARG( QString, mUuid ) );
  }
}

QVariant QgsSqlOdbcTransactionResult::data( int i )
{
  QVariant ret;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "data", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( QString, mUuid ), Q_ARG( int, i ) );

  setLastError( lastErrorTransactionResult() );
  return ret;
}

void QgsSqlOdbcTransactionResult::setForwardOnly( bool forward )
{
  QSqlResult::setForwardOnly( forward );

  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "setForwardOnly", Qt::BlockingQueuedConnection,  Q_ARG( QString, mUuid ), Q_ARG( bool, forward ) );

  setLastError( lastErrorTransactionResult() );
}

bool QgsSqlOdbcTransactionResult::isNull( int i )
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "isNull", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ), Q_ARG( int, i ) );

  setLastError( lastErrorTransactionResult() );
  return ret;
}

bool QgsSqlOdbcTransactionResult::reset( const QString &sqlquery )
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "reset", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ), Q_ARG( QString, sqlquery ) );

  setSelect( isTransactionResultSelect() );
  setActive( isTransactionResultActive() );
  QSqlResult::setAt( atTransactionResult() );
  setLastError( lastErrorTransactionResult() );
  return ret;
}

bool QgsSqlOdbcTransactionResult::fetchNext()
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "fetchNext", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ) );

  QSqlResult::setAt( atTransactionResult() );
  setLastError( lastErrorTransactionResult() );
  return ret;
}

bool QgsSqlOdbcTransactionResult::fetch( int i )
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "fetch", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ), Q_ARG( int, i ) );

  QSqlResult::setAt( atTransactionResult() );
  setLastError( lastErrorTransactionResult() );
  return ret;
}

bool QgsSqlOdbcTransactionResult::fetchFirst()
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "fetchFirst", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ) );

  QSqlResult::setAt( atTransactionResult() );
  setLastError( lastErrorTransactionResult() );
  return ret;
}

bool QgsSqlOdbcTransactionResult::fetchLast()
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "fetchLast", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ) );

  QSqlResult::setAt( atTransactionResult() );
  setLastError( lastErrorTransactionResult() );
  return ret;
}

int QgsSqlOdbcTransactionResult::size()
{
  int ret = -1;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "size", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( QString, mUuid ) );

  setLastError( lastErrorTransactionResult() );
  return ret;
}

int QgsSqlOdbcTransactionResult::numRowsAffected()
{
  int ret = -1;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "numRowsAffected", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( QString, mUuid ) );

  setLastError( lastErrorTransactionResult() );
  return ret;
}

QSqlRecord QgsSqlOdbcTransactionResult::record() const
{
  QSqlRecord ret;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "record", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlRecord, ret ), Q_ARG( QString, mUuid ) );

  return ret;

}

void QgsSqlOdbcTransactionResult::setAt( int index )
{
  if ( index == atTransactionResult() )
    return;

  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "setAt", Qt::BlockingQueuedConnection,  Q_ARG( QString, mUuid ), Q_ARG( int, index ) );

  QSqlResult::setAt( atTransactionResult() );
}

bool QgsSqlOdbcTransactionResult::isTransactionResultSelect() const
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "isSelect", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ) );

  return ret;
}

bool QgsSqlOdbcTransactionResult::isTransactionResultActive() const
{
  bool ret = false;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "isActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QString, mUuid ) );

  return ret;
}

int QgsSqlOdbcTransactionResult::atTransactionResult() const
{
  int ret = QSql::BeforeFirstRow;
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "at", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( QString, mUuid ) );

  return ret;
}

QSqlError QgsSqlOdbcTransactionResult::lastErrorTransactionResult() const
{
  QSqlError ret = QSqlError();
  if ( connectionIsValid() )
    QMetaObject::invokeMethod( mConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ), Q_ARG( QString, mUuid ) );
  else
    ret = QSqlError( QObject::tr( "ODBCTransactionProxy: Invalid Transaction Connection" ), QString(), QSqlError::ConnectionError );

  return ret;
}

QString QgsSqlODBCDatabaseTransactionConnection::createTransactionResult()
{
  QString uid = QUuid::createUuid().toString();
  mQueries[uid] = QSharedPointer<QSqlQuery>( new QSqlQuery( createResult() ) );
  return uid;
}

void QgsSqlODBCDatabaseTransactionConnection::removeTransactionResult( const QString &uuid )
{
  if ( !mQueries.contains( uuid ) )
    return;

  QSqlQuery *query = mQueries.value( uuid ).get();
  query->clear();

  mQueries.remove( uuid );
}

QVariant QgsSqlODBCDatabaseTransactionConnection::data( const QString &uuid, int i ) const
{
  if ( !mQueries.contains( uuid ) )
    return QVariant();
  QSqlQuery *query = mQueries[uuid].get();
  if ( query && query->isActive() )
    return query->value( i );
  else
    return QVariant();
}

bool QgsSqlODBCDatabaseTransactionConnection::reset( const QString &uuid, const QString &stringQuery )
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->exec( stringQuery );

  return false;
}

void QgsSqlODBCDatabaseTransactionConnection::setForwardOnly( const QString &uuid, bool forward )
{
  if ( !mQueries.contains( uuid ) )
    return;
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->setForwardOnly( forward );
}

bool QgsSqlODBCDatabaseTransactionConnection::fetch( const QString &uuid, int index )
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  if ( !query || !query->isActive() || !query->isSelect() )
    return false;

  if ( !query->isValid() && !query->first() )
    return false;

  return query->seek( index );

}

bool QgsSqlODBCDatabaseTransactionConnection::fetchFirst( const QString &uuid )
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  if ( !query || !query->isActive() || !query->isSelect() )
    return false;

  return query->first();
}

bool QgsSqlODBCDatabaseTransactionConnection::fetchLast( const QString &uuid )
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  if ( !query || !query->isActive() || !query->isSelect() )
    return false;

  return query->last();
}

bool QgsSqlODBCDatabaseTransactionConnection::fetchNext( const QString &uuid )
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  if ( !query || !query->isActive() || !query->isSelect() )
    return false;

  return query->next();
}

void QgsSqlODBCDatabaseTransactionConnection::setAt( const QString &uuid, int index )
{
  if ( !mQueries.contains( uuid ) )
    return;
  QSqlQuery *query = mQueries[uuid].get();

  if ( !query || !query->isActive() || !query->isSelect() )
    return;

  if ( index != QSql::AfterLastRow )
    query->seek( index );
  else
  {
    query->last();
    query->next();
  }
}

bool QgsSqlODBCDatabaseTransactionConnection::isSelect( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  return query && query->isSelect();
}

bool QgsSqlODBCDatabaseTransactionConnection::isActive( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  return ( query && query->isActive() );
}

bool QgsSqlODBCDatabaseTransactionConnection::isNull( const QString &uuid, int i ) const
{
  if ( !mQueries.contains( uuid ) )
    return false;
  QSqlQuery *query = mQueries[uuid].get();

  return  !query || query->isNull( i );
}

int QgsSqlODBCDatabaseTransactionConnection::at( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return QSql::BeforeFirstRow;
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->at();
  else
    return QSql::BeforeFirstRow;
}

int QgsSqlODBCDatabaseTransactionConnection::size( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return -1;
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->size();
  else
    return -1;
}

int QgsSqlODBCDatabaseTransactionConnection::numRowsAffected( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return -1;
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->numRowsAffected();
  else
    return -1;
}

QSqlRecord QgsSqlODBCDatabaseTransactionConnection::record( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return QSqlRecord();
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->record();
  else
    return QSqlRecord();
}

QSqlError QgsSqlODBCDatabaseTransactionConnection::lastError( const QString &uuid ) const
{
  if ( !mQueries.contains( uuid ) )
    return QSqlError();
  QSqlQuery *query = mQueries[uuid].get();

  if ( query )
    return query->lastError();
  else
    return QSqlError();
}

QSqlError QgsSqlODBCDatabaseTransactionConnection::lastError() const
{
  return mODBCDatabase.lastError();
}


bool QgsSqlODBCDatabaseTransactionConnection::beginTransaction()
{
  if ( !isOpen() )
    if ( !open() )
      return false;

  return mODBCDatabase.transaction();

}

bool QgsSqlODBCDatabaseTransactionConnection::commit()
{
  const QList<QSharedPointer<QSqlQuery>> &queries = mQueries.values();
  for ( QSharedPointer<QSqlQuery> query : queries )
  {
    if ( query->isActive() && query->isSelect() )
      query->finish();
  }

  return mODBCDatabase.commit();
}

bool QgsSqlODBCDatabaseTransactionConnection::rollBack()
{
  const QList<QSharedPointer<QSqlQuery>> &queries = mQueries.values();
  for ( QSharedPointer<QSqlQuery> query : queries )
  {
    if ( query->isActive() && query->isSelect() )
      query->finish();
  }

  return mODBCDatabase.rollback();
}
