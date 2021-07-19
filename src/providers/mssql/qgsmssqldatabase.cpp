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
    newDataBase.reset( new QgsMssqlDatabaseConnectionClassic( uri, connectionName ) );

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

QgsMssqlDatabaseConnectionClassic::QgsMssqlDatabaseConnectionClassic( QgsDataSourceUri uri, QString connectionName )
  : QgsMssqlDatabaseConnection( uri, connectionName )
{}

QgsMssqlDatabaseConnectionClassic::~QgsMssqlDatabaseConnectionClassic()
{}

void QgsMssqlDatabaseConnectionClassic::init()
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

bool QgsMssqlDatabaseConnectionClassic::open()
{
  return mDatabase.open();
}

void QgsMssqlDatabaseConnectionClassic::close()
{
  mSqlQueries.clear();
  mDatabase.close();
}

bool QgsMssqlDatabaseConnectionClassic::isOpen() const
{
  return mDatabase.isOpen();
}

bool QgsMssqlDatabaseConnectionClassic::isValid() const {return mDatabase.isValid();}

QSqlError QgsMssqlDatabaseConnectionClassic::lastError() const {return mDatabase.lastError();}

QStringList QgsMssqlDatabaseConnectionClassic::tables( QSql::TableType type ) const {return mDatabase.tables( type );}

bool QgsMssqlDatabaseConnectionClassic::beginTransaction()
{
  return mDatabase.transaction();
}

bool QgsMssqlDatabaseConnectionClassic::isTransaction() const {return false;}

void QgsMssqlDatabaseConnectionClassic::invalidate()
{
  mSqlQueries.clear();
  mIsInvalidated = true;
  if ( !mIsWorking ) // if the connection is currently work (with a share query still alive), the invalidation will be confirmed at the end of this work
    confirmInvalidation();
}

MssqlQueryRef QgsMssqlDatabaseConnectionClassic::createQuery()
{
  std::shared_ptr<QSqlQuery> query( new QSqlQuery( mDatabase ) );
  mSqlQueries.append( query );
  return MssqlQueryRef( this, query );
}


void QgsMssqlDatabaseConnectionClassic::addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->addBindValue( val, paramType );
}

void QgsMssqlDatabaseConnectionClassic::clear( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->clear();
}

bool QgsMssqlDatabaseConnectionClassic::exec( MssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec( queryString );

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::exec( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->exec();

  return false;
}

void QgsMssqlDatabaseConnectionClassic::finish( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    query->finish();
}

bool QgsMssqlDatabaseConnectionClassic::first( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->first();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isActive( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isActive();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isValid( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isValid();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isForwardOnly( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->isForwardOnly();

  QSqlQuery q;
  return q.isForwardOnly();

}

QSqlError QgsMssqlDatabaseConnectionClassic::lastError( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastError();

  QSqlQuery q;
  return q.lastError();
}

QString QgsMssqlDatabaseConnectionClassic::lastQuery( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->lastQuery();

  QSqlQuery q;
  return q.lastQuery();
}

bool QgsMssqlDatabaseConnectionClassic::next( MssqlQueryRef &queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->next();

  return false;
}

int QgsMssqlDatabaseConnectionClassic::numRowsAffected( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->numRowsAffected();

  QSqlQuery q;
  return q.numRowsAffected();
}

bool QgsMssqlDatabaseConnectionClassic::prepare( MssqlQueryRef &queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->prepare( queryString );

  return false;
}

QSqlRecord QgsMssqlDatabaseConnectionClassic::record( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->record();

  QSqlQuery q;
  return q.record();
}

void QgsMssqlDatabaseConnectionClassic::setForwardOnly( MssqlQueryRef &queryRef, bool forward )
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->setForwardOnly( forward );
}

int QgsMssqlDatabaseConnectionClassic::size( MssqlQueryRef &queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->size();

  QSqlQuery q;
  return q.size();
}

QVariant QgsMssqlDatabaseConnectionClassic::value( MssqlQueryRef &queryRef, int index ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( index );

  return QVariant();
}

QVariant QgsMssqlDatabaseConnectionClassic::value( MssqlQueryRef &queryRef, const QString &name ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.data->mSqlQueryWeakRef.lock();
  if ( query )
    return query->value( name );

  return QVariant();
}

void QgsMssqlDatabaseConnectionClassic::removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef )
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


