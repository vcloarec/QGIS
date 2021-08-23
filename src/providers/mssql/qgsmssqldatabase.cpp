/***************************************************************************
  qgsmssqldatabase.cpp - QgsMssqlDatabase

 ---------------------
 begin                : 15.7.2021
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
#include "qgsmssqldatabase.h"

#include <QMutex>
#include <QSqlError>
#include <QSqlRecord>
#include <QThread>
#include <QEventLoop>

#include "qgsdatasourceuri.h"


#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QMutex QgsMssqlDatabase::sMutex { QMutex::Recursive };
#else
QRecursiveMutex QgsMssqlDatabase::sMutex;
#endif

QMap < QString, std::shared_ptr<QgsMssqlDatabaseConnection>> QgsMssqlDatabase::sExistingConnections;


QgsMssqlDatabase::QgsMssqlDatabase( const QgsDataSourceUri &uri ):
  mUri( uri )
{
  mDatabaseConnectionRef = getConnection( mUri, false );
}

QgsMssqlDatabase::QgsMssqlDatabase( const QgsMssqlDatabase &other )
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = other.mDatabaseConnectionRef.lock() )
  {
    mDatabaseConnectionRef = connection;
    connection->ref();
  }

  mUri = other.mUri;
}

QgsMssqlDatabase::~QgsMssqlDatabase()
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
  {
    if ( connection->isInvalidated() )
      emit connection->finished();
  }

  dereferenceConnection( mDatabaseConnectionRef );
}

QgsMssqlDatabase &QgsMssqlDatabase::operator=( const QgsMssqlDatabase &other )
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = other.mDatabaseConnectionRef.lock() )
  {
    if ( mDatabaseConnectionRef.lock() == connection )
      return *this;

    dereferenceConnection( mDatabaseConnectionRef );
    mDatabaseConnectionRef = connection;
    connection->ref();
  }

  mUri = other.mUri;

  return *this;
}

std::shared_ptr<QgsMssqlDatabaseConnection> QgsMssqlDatabase::getConnection( const QgsDataSourceUri &uri, bool transaction )
{
  QMutexLocker locker( &sMutex );

  QString transactionConnectionName = threadConnectionName( uri, true );
  if ( sExistingConnections.contains( transactionConnectionName ) )
  {
    std::shared_ptr<QgsMssqlDatabaseConnection> dataBase = sExistingConnections.value( transactionConnectionName );
    dataBase->ref();
    return dataBase;
  }

  if ( transaction )
  {
    const QStringList connectionNames = sExistingConnections.keys();
    const QString transactionShortName = connectionName( uri );
    const QString currentThreadString = threadString();

    // here we invalidate and remove current existing connection with the same uri
    QStringList connectionsToRemove;
    for ( const QString &existingConnectionName : connectionNames )
    {
      std::shared_ptr<QgsMssqlDatabaseConnection> otherConnection = sExistingConnections.value( existingConnectionName );
      if ( existingConnectionName.contains( transactionShortName ) )
      {
        connectionsToRemove.append( existingConnectionName );
        if ( existingConnectionName.contains( currentThreadString ) )
        {
          // this connection is in the same thread, we can simply invalidate and remove it
          otherConnection->invalidate();
          QSqlDatabase::removeDatabase( existingConnectionName );
        }
        else
        {
          // this connection is not in the same thread, we need to wait the connection finishes its eventual current works
          QEventLoop loop;
          QObject::connect( otherConnection.get(), &QgsMssqlDatabaseConnection::finished, &loop, &QEventLoop::quit );
          otherConnection->invalidate();
          if ( !otherConnection->mIsFinished )
            loop.exec();

          // in certain circumstances the connection could not be finished correctly, be sure it is the case before removing it
          if ( !otherConnection->isFinished() )
            otherConnection->onQueryFinish();

          QSqlDatabase::removeDatabase( existingConnectionName );
        }
      }
    }

    for ( const QString &connectionToRemove : std::as_const( connectionsToRemove ) )
      sExistingConnections.remove( connectionToRemove );
  }

  // Check if there is a existing connection corresponding, if yes return it, if not create a new one
  QString connectionName = threadConnectionName( uri, transaction );
  if ( sExistingConnections.contains( connectionName ) )
  {
    std::shared_ptr<QgsMssqlDatabaseConnection> dataBase = sExistingConnections.value( connectionName );
    dataBase->ref();
    return dataBase;
  }

  std::shared_ptr<QgsMssqlDatabaseConnection> newDataBase;
  if ( transaction )
    newDataBase.reset( new QgsMssqlDatabaseConnectionTransaction( uri, connectionName ) );
  else
    newDataBase.reset( new QgsMssqlDatabaseConnection( uri, connectionName ) );

  newDataBase->init();

  sExistingConnections.insert( connectionName, newDataBase );

  newDataBase->ref();
  return newDataBase;
}


void QgsMssqlDatabase::dereferenceConnection( std::weak_ptr<QgsMssqlDatabaseConnection> mDatabaseConnectionRef )
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
  {
    if ( !connection->deref() )
    {
      QString connectionName = connection->connectionName();
      if ( !connection->isInvalidated() )
      {
        QMutexLocker locker( &sMutex );
        sExistingConnections.remove( connectionName );
        connection->invalidate();
        QSqlDatabase::removeDatabase( connectionName );
      }
    }
  }
}

QString QgsMssqlDatabase::threadConnectionName( const QgsDataSourceUri &uri, bool transaction )
{
  QString thrdString;
  if ( transaction )
    thrdString = QStringLiteral( "transaction" );
  else
    thrdString = threadString();

  return QStringLiteral( "%1:0x%2" ).arg( connectionName( uri ), thrdString );
}

QString QgsMssqlDatabase::threadString()
{
  return QString( "%1" ).arg( reinterpret_cast<quintptr>( QThread::currentThread() ), 2 * QT_POINTER_SIZE, 16, QLatin1Char( '0' ) );
}

QString QgsMssqlDatabase::connectionName( const QgsDataSourceUri &uri )
{
  return  uri.connectionInfo();
}

bool QgsMssqlDatabase::open()
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return connection->open();

  return false;
}

void QgsMssqlDatabase::close()
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    connection->close();
}

bool QgsMssqlDatabase::isOpen() const
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return connection->isOpen();

  return false;
}

bool QgsMssqlDatabase::isValid() const
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return connection->isValid();

  return false;
}

QSqlError QgsMssqlDatabase::lastError() const
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return connection->lastError();

  return QSqlDatabase().lastError();
}

QStringList QgsMssqlDatabase::tables( QSql::TableType type ) const
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return connection->tables( type );

  return QSqlDatabase().tables( type );
}

bool QgsMssqlDatabase::transaction()
{
  std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock();
  if ( !connection )
    return false;

  if ( connection->isTransaction() )
    return connection->beginTransaction();

  connection.reset();

  dereferenceConnection( mDatabaseConnectionRef );

  mDatabaseConnectionRef = getConnection( mUri, true );

  connection = mDatabaseConnectionRef.lock();

  if ( !connection->open() )
    return false;

  return connection->beginTransaction();
}

bool QgsMssqlDatabase::commit()
{
  std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock();
  if ( !connection )
    return false;

  return connection->commit();
}

bool QgsMssqlDatabase::rollback()
{
  std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock();
  if ( !connection )
    return false;

  return connection->rollback();
}

QgsMssqlQuery QgsMssqlDatabase::createQuery()
{
  return QgsMssqlQuery( *this );
}


QgsMssqlDatabase QgsMssqlDatabase::database( const QgsDataSourceUri &uri )
{
  return QgsMssqlDatabase( uri );
}

QgsMssqlDatabase QgsMssqlDatabase::database( const QString &service, const QString &host, const QString &db, const QString &username, const QString &password )
{
  QgsDataSourceUri uri;
  if ( !service.isEmpty() )
    uri.setConnection( service, db, username, password );
  else
    uri.setConnection( host, QString(), db, username, password );

  return database( uri );
}

QgsMssqlQuery::QgsMssqlQuery( QgsMssqlDatabase &database )
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = database.mDatabaseConnectionRef.lock() )
  {
    if ( !connection->isInvalidated() )
    {
      d = connection->createQuery().data;
      if ( d )
        d->mConnectionWeakRef = connection;
    }
  }
}

QgsMssqlQuery::QgsMssqlQuery( const QgsMssqlQuery &other )
{
  d = other.d;
  d->ref.ref();
}

QgsMssqlQuery::~QgsMssqlQuery()
{
  if ( d )
  {
    if ( !d->ref.deref() )
    {
      if ( !d->mSqlQueryWeakRef.expired() )
      {
        QgsMssqlQueryRef queryRef( d );
        d->mDatabaseConnection->removeSqlQuery( queryRef );
      }

      QgsMssqlDatabase::dereferenceConnection( d->mConnectionWeakRef );
      delete d;
    }
  }
}

QgsMssqlQuery &QgsMssqlQuery::operator=( const QgsMssqlQuery &other )
{
  if ( d == other.d )
    return *this;

  if ( !d->ref.deref() )
  {
    QgsMssqlDatabase::dereferenceConnection( d->mConnectionWeakRef );
    delete d;
  }

  d = other.d;
  d->ref.ref();

  return *this;
}

bool QgsMssqlQuery::isValid() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
  {
    return d->mDatabaseConnection->isValid( queryRef );
  }

  return false;
}

bool QgsMssqlQuery::isForwardOnly() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->isForwardOnly( queryRef );

  return false;
}

bool QgsMssqlQuery::exec( const QString &query )
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->exec( queryRef, query );

  return false;
}

bool QgsMssqlQuery::exec()
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->exec( queryRef );

  return false;
}

bool QgsMssqlQuery::next()
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->next( queryRef );

  return false;
}

bool QgsMssqlQuery::isActive() const
{
  QgsMssqlQueryRef queryRef( d );

  if ( queryRef.isValid() )
    return d->mDatabaseConnection->isActive( queryRef );

  return false;
}

QVariant QgsMssqlQuery::value( int index ) const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->value( queryRef, index );

  return QVariant();
}

QVariant QgsMssqlQuery::value( const QString &name ) const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->value( queryRef, name );
  return QVariant();
}

QSqlError QgsMssqlQuery::lastError() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->lastError( queryRef );

  return QSqlQuery().lastError();
}

void QgsMssqlQuery::setForwardOnly( bool forward )
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->setForwardOnly( queryRef, forward );
}

QSqlRecord QgsMssqlQuery::record() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->record( queryRef );

  return QSqlQuery().record();
}

void QgsMssqlQuery::clear()
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->clear( queryRef );
}

QString QgsMssqlQuery::lastQuery() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->lastQuery( queryRef );

  return QSqlQuery().lastQuery();
}

void QgsMssqlQuery::finish()
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->finish( queryRef );
}

int QgsMssqlQuery::size() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->size( queryRef );

  return QSqlQuery().size();
}

bool QgsMssqlQuery::prepare( const QString &query )
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->prepare( queryRef, query );

  return false;
}

void QgsMssqlQuery::addBindValue( const QVariant &val, QSql::ParamType paramType )
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->addBindValue( queryRef, val, paramType );
}

int QgsMssqlQuery::numRowsAffected() const
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->numRowsAffected( queryRef );

  return QSqlQuery().numRowsAffected();
}

bool QgsMssqlQuery::first()
{
  QgsMssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->first( queryRef );

  return false;
}

QgsMssqlDatabaseConnection::QgsMssqlDatabaseConnection( const QgsDataSourceUri &uri, const QString &connectionName, bool useMultipleActiveResultSets )
  : mUri( uri )
  , mConnectionName( connectionName )
  , mUseMultipleActiveResultSets( useMultipleActiveResultSets )
{}

QgsMssqlDatabaseConnection::~QgsMssqlDatabaseConnection()
{}

void QgsMssqlDatabaseConnection::init()
{
  mDatabase.reset( new QSqlDatabase( QSqlDatabase::addDatabase( QStringLiteral( "QODBC" ), mConnectionName ) ) );
  mDatabase->setConnectOptions( QStringLiteral( "SQL_ATTR_CONNECTION_POOLING=SQL_CP_ONE_PER_HENV" ) );

  mDatabase->setHostName( mUri.host() );
  QString connectionString;
  if ( !mUri.service().isEmpty() )
  {
    // driver was specified explicitly
    connectionString = mUri.service();
  }
  else
  {
#ifdef Q_OS_WIN
    connectionString = "driver={SQL Server}";
#elif defined (Q_OS_MAC)
    QString freeTDSDriver( QCoreApplication::applicationDirPath().append( "/lib/libtdsodbc.so" ) );
    if ( QFile::exists( freeTDSDriver ) )
    {
      connectionString = QStringLiteral( "driver=%1;port=1433;TDS_Version=auto" ).arg( freeTDSDriver );
    }
    else
    {
      connectionString = QStringLiteral( "driver={FreeTDS};port=1433;TDS_Version=auto" );
    }
#else
    // It seems that FreeTDS driver by default uses an ancient TDS protocol version (4.2) to communicate with MS SQL
    // which was causing various data corruption errors, for example:
    // - truncating data from varchar columns to 255 chars - failing to read WKT for CRS
    // - truncating binary data to 4096 bytes (see @@TEXTSIZE) - failing to parse larger geometries
    // The added "TDS_Version=auto" should negotiate more recent version (manually setting e.g. 7.2 worked fine too)
    connectionString = QStringLiteral( "driver={FreeTDS};port=1433;TDS_Version=auto" );
#endif
  }

  if ( !mUri.host().isEmpty() )
    connectionString += ";server=" + mUri.host();

  if ( !mUri.database().isEmpty() )
    connectionString += ";database=" + mUri.database();

  if ( mUri.password().isEmpty() )
    connectionString += QLatin1String( ";trusted_connection=yes" );
  else
    connectionString += ";uid=" + mUri.username() + ";pwd=" + mUri.password();

  if ( mUseMultipleActiveResultSets )
    connectionString += QStringLiteral( ";MARS_Connection=yes" );

  if ( !mUri.username().isEmpty() )
    mDatabase->setUserName( mUri.username() );

  if ( !mUri.password().isEmpty() )
    mDatabase->setPassword( mUri.password() );

  mDatabase->setDatabaseName( connectionString );
}

bool QgsMssqlDatabaseConnection::open()
{
  QMutexLocker locker( &mMutex );
  if ( mIsInvalidated )
    return false;

  if ( !mDatabase->isOpen() )
    return mDatabase->open();
  else
    return true;
}

void QgsMssqlDatabaseConnection::close()
{
  QMutexLocker locker( &mMutex );
  mSqlQueries.clear();

  if ( !mIsInvalidated )
    mDatabase->close();
}

bool QgsMssqlDatabaseConnection::isOpen() const
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->isOpen();

  return false;
}

bool QgsMssqlDatabaseConnection::isValid() const
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->isValid();

  return false;
}

QSqlError QgsMssqlDatabaseConnection::lastError() const
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->lastError();

  return QSqlError();
}

QStringList QgsMssqlDatabaseConnection::tables( QSql::TableType type ) const
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->tables( type );

  return QStringList();

}

bool QgsMssqlDatabaseConnection::beginTransaction()
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->transaction();

  return false;
}

bool QgsMssqlDatabaseConnection::commit()
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->commit();

  return false;
}

bool QgsMssqlDatabaseConnection::rollback()
{
  QMutexLocker locker( &mMutex );
  if ( !mIsInvalidated )
    return mDatabase->rollback();

  return false;
}

bool QgsMssqlDatabaseConnection::isTransaction() const {return false;}

bool QgsMssqlDatabaseConnection::ref()
{
  return mRef.ref();
}

bool QgsMssqlDatabaseConnection::deref()
{
  return mRef.deref();
}

void QgsMssqlDatabaseConnection::invalidate()
{
  QMutexLocker locker( &mMutex );
  mSqlQueries.clear();

  if ( mIsFinished )
    return;

  mIsInvalidated = true;

  if ( !mIsWorking ) // if the connection is currently working (with a shared query still alive), the connection will finish at the end of this work
    onQueryFinish();
}

QgsMssqlQueryRef QgsMssqlDatabaseConnection::createQuery()
{
  QMutexLocker locker( &mMutex );

  if ( mIsInvalidated )
    return QgsMssqlQueryRef();

  std::shared_ptr<QSqlQuery> query( new QSqlQuery( *mDatabase.get() ) );
  mSqlQueries.append( query );
  return QgsMssqlQueryRef( this, query );
}


void QgsMssqlDatabaseConnection::addBindValue( QgsMssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->addBindValue( val, paramType );
}

void QgsMssqlDatabaseConnection::clear( QgsMssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->clear();
}

bool QgsMssqlDatabaseConnection::exec( QgsMssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec( queryString );

  return false;
}

bool QgsMssqlDatabaseConnection::exec( QgsMssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec();

  return false;
}

void QgsMssqlDatabaseConnection::finish( QgsMssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->finish();
}

bool QgsMssqlDatabaseConnection::first( QgsMssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->first();

  return false;
}

bool QgsMssqlDatabaseConnection::isActive( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isActive();

  return false;
}

bool QgsMssqlDatabaseConnection::isValid( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isValid();

  return false;
}

bool QgsMssqlDatabaseConnection::isForwardOnly( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isForwardOnly();

  QSqlQuery q;
  return q.isForwardOnly();

}

QSqlError QgsMssqlDatabaseConnection::lastError( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastError();

  QSqlQuery q;
  return q.lastError();
}

QString QgsMssqlDatabaseConnection::lastQuery( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastQuery();

  QSqlQuery q;
  return q.lastQuery();
}

bool QgsMssqlDatabaseConnection::next( QgsMssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->next();

  return false;
}

int QgsMssqlDatabaseConnection::numRowsAffected( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->numRowsAffected();

  QSqlQuery q;
  return q.numRowsAffected();
}

bool QgsMssqlDatabaseConnection::prepare( QgsMssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->prepare( queryString );

  return false;
}

QSqlRecord QgsMssqlDatabaseConnection::record( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->record();

  QSqlQuery q;
  return q.record();
}

void QgsMssqlDatabaseConnection::setForwardOnly( QgsMssqlQueryRef &queryRef, bool forward )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->setForwardOnly( forward );
}

int QgsMssqlDatabaseConnection::size( QgsMssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->size();

  QSqlQuery q;
  return q.size();
}

QVariant QgsMssqlDatabaseConnection::value( QgsMssqlQueryRef &queryRef, int index ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( index );

  return QVariant();
}

QVariant QgsMssqlDatabaseConnection::value( QgsMssqlQueryRef &queryRef, const QString &name ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( name );

  return QVariant();
}

void QgsMssqlDatabaseConnection::removeSqlQuery( QgsMssqlQueryRef &queryRef )
{
  QMutexLocker locker( &mMutex );
  std::shared_ptr<QSqlQuery> queryToRemove = queryRef.data->mSqlQueryWeakRef.lock();
  if ( queryToRemove )
    for ( int i = 0; i < mSqlQueries.count(); ++i )
    {
      if ( mSqlQueries.at( i ).get() == queryToRemove.get() )
      {
        mSqlQueries.removeAt( i );
        return;
      }
    }
}

bool QgsMssqlDatabaseConnection::isInvalidated() const
{
  return mIsInvalidated;
}

QString QgsMssqlDatabaseConnection::connectionName() const
{
  return mConnectionName;
}

bool QgsMssqlDatabaseConnection::isFinished() const
{
  return mIsFinished;
}

void QgsMssqlDatabaseConnection::onQueryFinish()
{
  if ( mIsInvalidated )
  {
    mIsFinished = true;
    mDatabase.reset();
    emit finished();
  }
}

QgsMssqlQueryRef::QgsMssqlQueryRef( Data *d ): data( d )
{
  if ( data && !data->mDatabaseConnection->mIsInvalidated )
    data->mDatabaseConnection->mIsWorking = true;
  else if ( data )
    data->isInvalidate = true;
}

QgsMssqlQueryRef::QgsMssqlQueryRef( QgsMssqlDatabaseConnection *databaseConnection, std::shared_ptr<QSqlQuery> query )
{
  data = new Data;
  data->mDatabaseConnection = databaseConnection;
  databaseConnection->ref();
  data->mSqlQueryWeakRef = query;
  data->ref.ref();
  data->isInvalidate = false;
}

QgsMssqlQueryRef::~QgsMssqlQueryRef()
{
  if ( data )
  {
    if ( data->mSqlQueryWeakRef.expired() && !data->isInvalidate )
    {
      data->mDatabaseConnection->onQueryFinish();
      data->isInvalidate = true;
    }
    data->mDatabaseConnection->mIsWorking = false;
  }
}

bool QgsMssqlQueryRef::isValid() const
{
  return ( data != nullptr && !data->mSqlQueryWeakRef.expired() && !data->isInvalidate );
}

bool QgsMssqlDatabaseConnectionTransaction::isTransaction() const {return true;}

QgsMssqlDatabaseConnectionTransaction::QgsMssqlDatabaseConnectionTransaction( const QgsDataSourceUri &uri, const QString &connectionName ): QgsMssqlDatabaseConnection( uri, connectionName, true )
{
  mThread = new QThread( this );
  mThreadedConnection = new QgsMssqlDatabaseConnection( uri, connectionName, true );
  mThreadedConnection->moveToThread( mThread );
  mThread->start();
}

QgsMssqlDatabaseConnectionTransaction::~QgsMssqlDatabaseConnectionTransaction()
{
  mThread->quit();
  mThread->wait();
  delete mThreadedConnection;
}

void QgsMssqlDatabaseConnectionTransaction::init()
{
  QMetaObject::invokeMethod( mThreadedConnection, "init", Qt::BlockingQueuedConnection );
}

bool QgsMssqlDatabaseConnectionTransaction::open()
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "open", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::close()
{
  if ( mRef == 1 )
    QMetaObject::invokeMethod( mThreadedConnection, "close", Qt::BlockingQueuedConnection );
}

bool QgsMssqlDatabaseConnectionTransaction::isOpen() const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isOpen", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isValid() const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isValid", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

QSqlError QgsMssqlDatabaseConnectionTransaction::lastError() const
{
  QSqlError ret;
  QMetaObject::invokeMethod( mThreadedConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ) );
  return ret;
}

QStringList QgsMssqlDatabaseConnectionTransaction::tables( QSql::TableType type ) const
{
  QStringList ret;
  QMetaObject::invokeMethod( mThreadedConnection, "tables", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QStringList, ret ), Q_ARG( QSql::TableType, type ) );
  return ret;
}

QgsMssqlQueryRef QgsMssqlDatabaseConnectionTransaction::createQuery()
{
  QgsMssqlQueryRef ret;
  QMetaObject::invokeMethod( mThreadedConnection, "createQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QgsMssqlQueryRef, ret ) );
  ret.data->mDatabaseConnection = this;
  ref();
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::invalidate()
{
  QMetaObject::invokeMethod( mThreadedConnection, "invalidate", Qt::BlockingQueuedConnection );
  mIsInvalidated = true;
}

bool QgsMssqlDatabaseConnectionTransaction::beginTransaction()
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::commit()
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "commit", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::rollback()
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "rollback", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::addBindValue( QgsMssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType )
{
  QMetaObject::invokeMethod( mThreadedConnection, "addBindValue", Qt::BlockingQueuedConnection, Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( QVariant, val ), Q_ARG( QSql::ParamType, paramType ) );
}

void QgsMssqlDatabaseConnectionTransaction::clear( QgsMssqlQueryRef &queryRef )
{
  QMetaObject::invokeMethod( mThreadedConnection, "clear", Qt::BlockingQueuedConnection, Q_ARG( QgsMssqlQueryRef &, queryRef ) );
}

bool QgsMssqlDatabaseConnectionTransaction::exec( QgsMssqlQueryRef &queryRef, const QString &queryString )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::exec( QgsMssqlQueryRef &queryRef )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::finish( QgsMssqlQueryRef &queryRef )
{
  QMetaObject::invokeMethod( mThreadedConnection, "finish", Qt::BlockingQueuedConnection, Q_ARG( QgsMssqlQueryRef &, queryRef ) );
}

bool QgsMssqlDatabaseConnectionTransaction::first( QgsMssqlQueryRef &queryRef )
{
  bool ret = false;
  QMetaObject::invokeMethod( mThreadedConnection, "first", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isActive( QgsMssqlQueryRef &queryRef ) const
{
  bool ret = false;
  QMetaObject::invokeMethod( mThreadedConnection, "isActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isValid( QgsMssqlQueryRef &queryRef ) const
{
  bool ret = false;
  QMetaObject::invokeMethod( mThreadedConnection, "isValid", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isForwardOnly( QgsMssqlQueryRef &queryRef ) const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isForwardOnly", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

QSqlError QgsMssqlDatabaseConnectionTransaction::lastError( QgsMssqlQueryRef &queryRef ) const
{
  QSqlError ret;
  QMetaObject::invokeMethod( mThreadedConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

QString QgsMssqlDatabaseConnectionTransaction::lastQuery( QgsMssqlQueryRef &queryRef ) const
{
  QString ret;
  QMetaObject::invokeMethod( mThreadedConnection, "lastQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::next( QgsMssqlQueryRef &queryRef )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "next", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

int QgsMssqlDatabaseConnectionTransaction::numRowsAffected( QgsMssqlQueryRef &queryRef ) const
{
  int ret;
  QMetaObject::invokeMethod( mThreadedConnection, "numRowsAffected", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::prepare( QgsMssqlQueryRef &queryRef, const QString &queryString )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "prepare", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
  return ret;
}

QSqlRecord QgsMssqlDatabaseConnectionTransaction::record( QgsMssqlQueryRef &queryRef ) const
{
  QSqlRecord ret;
  QMetaObject::invokeMethod( mThreadedConnection, "record", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlRecord, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::setForwardOnly( QgsMssqlQueryRef &queryRef, bool forward )
{
  QMetaObject::invokeMethod( mThreadedConnection, "setForwardOnly", Qt::BlockingQueuedConnection, Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( bool, forward ) );
}

int QgsMssqlDatabaseConnectionTransaction::size( QgsMssqlQueryRef &queryRef ) const
{
  int ret;
  QMetaObject::invokeMethod( mThreadedConnection, "size", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ) );
  return ret;
}

QVariant QgsMssqlDatabaseConnectionTransaction::value( QgsMssqlQueryRef &queryRef, int index ) const
{
  QVariant ret;
  QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( int, index ) );
  return ret;
}

QVariant QgsMssqlDatabaseConnectionTransaction::value( QgsMssqlQueryRef &queryRef, const QString &name ) const
{
  QVariant ret;
  QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( QgsMssqlQueryRef &, queryRef ), Q_ARG( QString, name ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::removeSqlQuery( QgsMssqlQueryRef &queryRef )
{
  QMetaObject::invokeMethod( mThreadedConnection, "removeSqlQuery", Qt::BlockingQueuedConnection,  Q_ARG( QgsMssqlQueryRef &, queryRef ) );
}
