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
#include <QDebug>

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
}

QgsMssqlDatabase::~QgsMssqlDatabase()
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
  {
    if ( !connection->isInvalidated() )
      dereferenceConnection( mDatabaseConnectionRef );
    else
      emit connection->isStopped();
  }
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
  return *this;
}

std::shared_ptr<QgsMssqlDatabaseConnection> QgsMssqlDatabase::getConnection( const QgsDataSourceUri &uri, bool transaction )
{
  QMutexLocker locker( &sMutex );

  // look if there is an associated transaction existing with the uri
  QString transactionConncectionName = threadConnectionName( uri, true );
  if ( sExistingConnections.contains( transactionConncectionName ) )
  {
    std::shared_ptr<QgsMssqlDatabaseConnection> dataBase = sExistingConnections.value( transactionConncectionName );
    dataBase->ref();
    return dataBase;
  }

  if ( transaction )
  {
    const QStringList connectionNames = sExistingConnections.keys();
    const QString transactionShordName = connectionName( uri );
    const QString currentThreadString = threadString();

    for ( const QString &existingConnectionName : connectionNames )
    {
      std::shared_ptr<QgsMssqlDatabaseConnection> otherConnection = sExistingConnections.value( existingConnectionName );
      if ( existingConnectionName.contains( transactionShordName ) )
      {
        if ( existingConnectionName.contains( currentThreadString ) )
        {
          otherConnection->invalidate();
          sExistingConnections.remove( existingConnectionName );
          otherConnection.reset();
          QSqlDatabase::removeDatabase( existingConnectionName );
        }
        else
        {
          //we need to wait the connection is finished its eventual current work
          QEventLoop loop;
          QObject::connect( otherConnection.get(), &QgsMssqlDatabaseConnection::isStopped, &loop, &QEventLoop::quit );
          otherConnection->invalidate();
          loop.exec();

          //we can't remove it because, so let it until the end of the other thread
        }
      }
    }
  }

  QString connectionName = threadConnectionName( uri, transaction );

//  qDebug() << "------------------------------------------------------------------: " ;
//  qDebug() << "new connection to get: " << connectionName;
//  QStringList keys( sExistingConnections.keys() );
//  qDebug() << "MSSQL connection presents: " << keys.join( "\n" );
//  qDebug() << "------------------------------------------------------------------: ";
//  QStringList all( QSqlDatabase::connectionNames() );
//  qDebug() << "ALL connection presents: " << all.join( "\n" );
//  qDebug() << "------------------------------------------------------------------: ";


  if ( sExistingConnections.contains( connectionName ) )
  {
    std::shared_ptr<QgsMssqlDatabaseConnection> dataBase = sExistingConnections.value( connectionName );
    dataBase->ref();
    return dataBase;
  }

//  qDebug() << "------------------------------------------------------------------: " ;
//  qDebug() << "new connection to add: " << connectionName;
//  keys = sConnectionsThread.keys();
//  qDebug() << "MSSQL connection presents: " << keys.join( "\n" );
//  QStringList all( QSqlDatabase::connectionNames() );
//  qDebug() << "ALL connection presents: " << all.join( "\n" );
//  qDebug() << "------------------------------------------------------------------: ";

  std::shared_ptr<QgsMssqlDatabaseConnection> newDataBase;

  if ( transaction )
    newDataBase.reset( new QgsMssqlDatabaseConnectionTransaction( uri, connectionName ) );
  else
    newDataBase.reset( new QgsMssqlDatabaseConnection( uri, connectionName ) );

  newDataBase->init();

  sExistingConnections.insert( connectionName, newDataBase );

  //  keys = sConnectionsThread.keys();
//  qDebug() << "------------------------------------------------------------------: " << connectionName;
//  qDebug() << "MSSQL connection presents after adding: " << keys.join( "\n" );

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
      sExistingConnections.remove( connectionName );
      connection.reset();
      QSqlDatabase::removeDatabase( connectionName );
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

  return QStringLiteral( "%1:0x%2" ).arg( connectionName( uri ) ).arg( thrdString );
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
    if ( !connection->isInvalidated() )
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
    return !connection->isInvalidated() && connection->isOpen();

  return false;
}

bool QgsMssqlDatabase::isValid() const
{
  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
    return !connection->isInvalidated() && connection->isValid();

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
    return true;

  mDatabaseConnectionRef = getConnection( mUri, true );

  connection = mDatabaseConnectionRef.lock();

  if ( !connection->open() )
    return false;

  return connection->beginTransaction();
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
  if ( d && !d->ref.deref() && !d->mSqlQueryWeakRef.expired() )
  {
    d->mDatabaseConnection->removeSqlQuery( d->mSqlQueryWeakRef );
    delete d;
  }
}

QgsMssqlQuery &QgsMssqlQuery::operator=( const QgsMssqlQuery &other )
{
  if ( d == other.d )
    return *this;

  if ( !d->ref.deref() )
    delete d;

  d = other.d;
  d->ref.ref();

  return *this;
}

bool QgsMssqlQuery::isValidPrivate() const
{
  if ( d == nullptr )
    return false;

  if ( d->mSqlQueryWeakRef.expired() )
    return false;

  return true;

  //return d != nullptr && !d->mSqlQueryWeakRef.expired();
}

bool QgsMssqlQuery::isValid() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
  {
    return d->mDatabaseConnection->isValid( queryRef );
  }

  return false;
}

bool QgsMssqlQuery::isForwardOnly() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->isForwardOnly( queryRef );

  return false;
}

bool QgsMssqlQuery::exec( const QString &query )
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->exec( queryRef, query );

  return false;
}

bool QgsMssqlQuery::exec()
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->exec( queryRef );

  return false;
}

bool QgsMssqlQuery::next()
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->next( queryRef );

  return false;
}

bool QgsMssqlQuery::isActive() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->isActive( queryRef );

  return false;
}

QVariant QgsMssqlQuery::value( int index ) const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->value( queryRef, index );

  return QVariant();
}

QVariant QgsMssqlQuery::value( const QString &name ) const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->value( queryRef, name );
  return QVariant();
}

QSqlError QgsMssqlQuery::lastError() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->lastError( queryRef );

  return QSqlQuery().lastError();
}

void QgsMssqlQuery::setForwardOnly( bool forward )
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->setForwardOnly( queryRef, forward );
}

QSqlRecord QgsMssqlQuery::record() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->record( queryRef );

  return QSqlQuery().record();
}

void QgsMssqlQuery::clear()
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->clear( queryRef );
}

QString QgsMssqlQuery::lastQuery() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->lastQuery( queryRef );

  return QSqlQuery().lastQuery();
}

void QgsMssqlQuery::finish()
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->finish( queryRef );
}

int QgsMssqlQuery::size() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->size( queryRef );

  return QSqlQuery().size();
}

bool QgsMssqlQuery::prepare( const QString &query )
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->prepare( queryRef, query );

  return false;
}

void QgsMssqlQuery::addBindValue( const QVariant &val, QSql::ParamType paramType )
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    d->mDatabaseConnection->addBindValue( queryRef, val, paramType );
}

int QgsMssqlQuery::numRowsAffected() const
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->numRowsAffected( queryRef );

  return QSqlQuery().numRowsAffected();
}

bool QgsMssqlQuery::first()
{
  MssqlQueryRef queryRef( d );
  if ( queryRef.isValid() )
    return d->mDatabaseConnection->first( queryRef );

  return false;
}

QgsMssqlDatabaseConnection::QgsMssqlDatabaseConnection( QgsDataSourceUri uri, QString connectionName )
  : mUri( uri )
  , mConnectionName( connectionName )
{}

QgsMssqlDatabaseConnection::~QgsMssqlDatabaseConnection()
{}

void QgsMssqlDatabaseConnection::init()
{
  mDatabase = QSqlDatabase::addDatabase( QStringLiteral( "QODBC" ), mConnectionName );
  mDatabase.setConnectOptions( QStringLiteral( "SQL_ATTR_CONNECTION_POOLING=SQL_CP_ONE_PER_HENV" ) );

  mDatabase.setHostName( mUri.host() );
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

  if ( !mUri.username().isEmpty() )
    mDatabase.setUserName( mUri.username() );

  if ( !mUri.password().isEmpty() )
    mDatabase.setPassword( mUri.password() );

  mDatabase.setDatabaseName( connectionString );
}

bool QgsMssqlDatabaseConnection::open()
{
  return mDatabase.open();
}

void QgsMssqlDatabaseConnection::close()
{
  mSqlQueries.clear();
  mDatabase.close();
}

bool QgsMssqlDatabaseConnection::isOpen() const
{
  return mDatabase.isOpen();
}

bool QgsMssqlDatabaseConnection::isValid() const {return mDatabase.isValid();}

QSqlError QgsMssqlDatabaseConnection::lastError() const {return mDatabase.lastError();}

QStringList QgsMssqlDatabaseConnection::tables( QSql::TableType type ) const {return mDatabase.tables( type );}

bool QgsMssqlDatabaseConnection::beginTransaction()
{
  return mDatabase.transaction();
}

bool QgsMssqlDatabaseConnection::isTransaction() const {return false;}

void QgsMssqlDatabaseConnection::invalidate()
{
  mSqlQueries.clear();
  mIsInvalidated = true;
  if ( !mIsWorking ) // if the connection is currently work (with a share query still alive), the invalidation will be confirmed at the end of this work
    confirmInvalidation();
}

MssqlQueryRef QgsMssqlDatabaseConnection::createQuery()
{
  std::shared_ptr<QSqlQuery> query( new QSqlQuery( mDatabase ) );
  mSqlQueries.append( query );
  return MssqlQueryRef( this, query );
}


void QgsMssqlDatabaseConnection::addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->addBindValue( val, paramType );
}

void QgsMssqlDatabaseConnection::clear( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->clear();
}

bool QgsMssqlDatabaseConnection::exec( MssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec( queryString );

  return false;
}

bool QgsMssqlDatabaseConnection::exec( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec();

  return false;
}

void QgsMssqlDatabaseConnection::finish( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->finish();
}

bool QgsMssqlDatabaseConnection::first( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->first();

  return false;
}

bool QgsMssqlDatabaseConnection::isActive( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isActive();

  return false;
}

bool QgsMssqlDatabaseConnection::isValid( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isValid();

  return false;
}

bool QgsMssqlDatabaseConnection::isForwardOnly( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isForwardOnly();

  QSqlQuery q;
  return q.isForwardOnly();

}

QSqlError QgsMssqlDatabaseConnection::lastError( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastError();

  QSqlQuery q;
  return q.lastError();
}

QString QgsMssqlDatabaseConnection::lastQuery( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastQuery();

  QSqlQuery q;
  return q.lastQuery();
}

bool QgsMssqlDatabaseConnection::next( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->next();

  return false;
}

int QgsMssqlDatabaseConnection::numRowsAffected( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->numRowsAffected();

  QSqlQuery q;
  return q.numRowsAffected();
}

bool QgsMssqlDatabaseConnection::prepare( MssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->prepare( queryString );

  return false;
}

QSqlRecord QgsMssqlDatabaseConnection::record( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->record();

  QSqlQuery q;
  return q.record();
}

void QgsMssqlDatabaseConnection::setForwardOnly( MssqlQueryRef &queryRef, bool forward )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->setForwardOnly( forward );
}

int QgsMssqlDatabaseConnection::size( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->size();

  QSqlQuery q;
  return q.size();
}

QVariant QgsMssqlDatabaseConnection::value( MssqlQueryRef &queryRef, int index ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( index );

  return QVariant();
}

QVariant QgsMssqlDatabaseConnection::value( MssqlQueryRef &queryRef, const QString &name ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( name );

  return QVariant();
}

void QgsMssqlDatabaseConnection::removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> queryToRemove = queryRef.lock();
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

MssqlQueryRef::MssqlQueryRef( Data *d ): data( d )
{
  if ( data )
    data->mDatabaseConnection->mIsWorking = true;
}

MssqlQueryRef::MssqlQueryRef( QgsMssqlDatabaseConnection *databaseConnection, std::shared_ptr<QSqlQuery> query )
{
  data = new Data;
  data->mDatabaseConnection = databaseConnection;
  data->mSqlQueryWeakRef = query;
  data->ref.ref();
}

MssqlQueryRef::~MssqlQueryRef()
{
  if ( data )
  {
    if ( data->mSqlQueryWeakRef.expired() && data->mDatabaseConnection->mIsInvalidated )
      data->mDatabaseConnection->confirmInvalidation();

    data->mDatabaseConnection->mIsWorking = false;

    if ( data->ref == 0 )
      delete data;
  }
}

bool MssqlQueryRef::isValid() const
{
  return ( data != nullptr && ! data->mSqlQueryWeakRef.expired() );
}

bool QgsMssqlDatabaseConnectionTransaction::isTransaction() const {return true;}

QgsMssqlDatabaseConnectionTransaction::QgsMssqlDatabaseConnectionTransaction( QgsDataSourceUri uri, QString connectionName ): QgsMssqlDatabaseConnection( uri, connectionName )
{
  mThread = new QThread;
  mThreadedConnection = new QgsMssqlDatabaseConnection( uri, connectionName );
  mThreadedConnection->moveToThread( mThread );
  connect( mThread, &QThread::started, this, &QgsMssqlDatabaseConnectionTransaction::init );
  connect( mThread, &QThread::finished, mThreadedConnection, &QgsMssqlDatabaseConnection::deleteLater );
  mThread->start();
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

MssqlQueryRef QgsMssqlDatabaseConnectionTransaction::createQuery()
{
  MssqlQueryRef ret;
  QMetaObject::invokeMethod( mThreadedConnection, "createQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( MssqlQueryRef, ret ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::invalidate()
{
  QMetaObject::invokeMethod( mThreadedConnection, "invalidate", Qt::BlockingQueuedConnection );
}

bool QgsMssqlDatabaseConnectionTransaction::beginTransaction()
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType )
{
  QMetaObject::invokeMethod( mThreadedConnection, "addBindValue", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QVariant, val ), Q_ARG( QSql::ParamType, paramType ) );
}

void QgsMssqlDatabaseConnectionTransaction::clear( MssqlQueryRef &queryRef )
{
  QMetaObject::invokeMethod( mThreadedConnection, "clear", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ) );
}

bool QgsMssqlDatabaseConnectionTransaction::exec( MssqlQueryRef &queryRef, const QString &queryString )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::exec( MssqlQueryRef &queryRef )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::finish( MssqlQueryRef &queryRef )
{
  QMetaObject::invokeMethod( mThreadedConnection, "finish", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ) );
}

bool QgsMssqlDatabaseConnectionTransaction::first( MssqlQueryRef &queryRef )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "first", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isActive( MssqlQueryRef &queryRef ) const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isValid( MssqlQueryRef &queryRef ) const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isValid", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::isForwardOnly( MssqlQueryRef &queryRef ) const
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "isForwardOnly", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

QSqlError QgsMssqlDatabaseConnectionTransaction::lastError( MssqlQueryRef &queryRef ) const
{
  QSqlError ret;
  QMetaObject::invokeMethod( mThreadedConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

QString QgsMssqlDatabaseConnectionTransaction::lastQuery( MssqlQueryRef &queryRef ) const
{
  QString ret;
  QMetaObject::invokeMethod( mThreadedConnection, "lastQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::next( MssqlQueryRef &queryRef )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "next", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

int QgsMssqlDatabaseConnectionTransaction::numRowsAffected( MssqlQueryRef &queryRef ) const
{
  int ret;
  QMetaObject::invokeMethod( mThreadedConnection, "numRowsAffected", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

bool QgsMssqlDatabaseConnectionTransaction::prepare( MssqlQueryRef &queryRef, const QString &queryString )
{
  bool ret;
  QMetaObject::invokeMethod( mThreadedConnection, "next", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
  return ret;
}

QSqlRecord QgsMssqlDatabaseConnectionTransaction::record( MssqlQueryRef &queryRef ) const
{
  QSqlRecord ret;
  QMetaObject::invokeMethod( mThreadedConnection, "record", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlRecord, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::setForwardOnly( MssqlQueryRef &queryRef, bool forward )
{
  QMetaObject::invokeMethod( mThreadedConnection, "setForwardOnly", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( bool, forward ) );
}

int QgsMssqlDatabaseConnectionTransaction::size( MssqlQueryRef &queryRef ) const
{
  int ret;
  QMetaObject::invokeMethod( mThreadedConnection, "size", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
  return ret;
}

QVariant QgsMssqlDatabaseConnectionTransaction::value( MssqlQueryRef &queryRef, int index ) const
{
  QVariant ret;
  QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( int, index ) );
  return ret;
}

QVariant QgsMssqlDatabaseConnectionTransaction::value( MssqlQueryRef &queryRef, const QString &name ) const
{
  QVariant ret;
  QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, name ) );
  return ret;
}

void QgsMssqlDatabaseConnectionTransaction::removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef )
{

}


