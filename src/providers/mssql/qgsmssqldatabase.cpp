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

QMap < QString, std::shared_ptr<QgsMssqlDatabaseConnection>> QgsMssqlDatabase::sConnectionsThread;

QgsMssqlDatabase::QgsMssqlDatabase( const QgsDataSourceUri &uri ):
  mUri( uri )
{
  mDatabaseConnectionRef = getConnection( mUri, false );
}

QgsMssqlDatabase::QgsMssqlDatabase( const QgsMssqlDatabase &other )
{
  mDatabaseConnectionRef = other.mDatabaseConnectionRef;
}

QgsMssqlDatabase::~QgsMssqlDatabase()
{
  dereferenceConnection( mDatabaseConnectionRef );
}

QgsMssqlDatabase &QgsMssqlDatabase::operator=( const QgsMssqlDatabase &other )
{
  //dereferenceConnection( mDatabaseConnection );
  mDatabaseConnectionRef = other.mDatabaseConnectionRef;
  return *this;
}

std::shared_ptr<QgsMssqlDatabaseConnection> QgsMssqlDatabase::getConnection( const QgsDataSourceUri &uri, bool transaction )
{
  QMutexLocker locker( &sMutex );

  if ( transaction )
  {
    const QStringList connectionNames = sConnectionsThread.keys();
    const QString transactionShordName = connectionName( uri );
    for ( const QString &exisingConnection : connectionNames )
    {
      if ( exisingConnection.contains( transactionShordName ) )
      {

      }
//      qDebug() << "invalidate connection ... " << connection->connectionName();
//      QEventLoop loop;
//      QObject::connect( connection, &QgsMssqlDatabaseConnection::invalidated, &loop, &QEventLoop::quit );
//      invalidateConnection( connection );
//      if ( !connection->isInvalidated() )
//      {
//        qDebug() << "wait for end of concurrent query ... " << connection->connectionName();
//        loop.exec();
//        qDebug() << "End of concurrent query ..." << connection->connectionName();
//      }
    }
  }

  //purgeConnections();

  QString connectionName = threadConnectionName( uri, transaction );

  qDebug() << "------------------------------------------------------------------: " ;
  qDebug() << "new connection to get: " << connectionName;
  QStringList keys( sConnectionsThread.keys() );
  qDebug() << "MSSQL connection presents: " << keys.join( "\n" );
  qDebug() << "------------------------------------------------------------------: ";


  if ( sConnectionsThread.contains( connectionName ) )
  {
    sConnectionsThread.value( connectionName )->ref();
    return sConnectionsThread.value( connectionName );
  }

//  qDebug() << "------------------------------------------------------------------: " ;
//  qDebug() << "new connection to add: " << connectionName;
//  keys = sConnectionsThread.keys();
//  qDebug() << "MSSQL connection presents: " << keys.join( "\n" );
//  QStringList all( QSqlDatabase::connectionNames() );
//  qDebug() << "ALL connection presents: " << all.join( "\n" );
//  qDebug() << "------------------------------------------------------------------: ";

  std::shared_ptr<QgsMssqlDatabaseConnection> newDataBase( new QgsMssqlDatabaseConnectionClassic( uri, connectionName ) );
  newDataBase->init();
  sConnectionsThread.insert( connectionName, newDataBase );

//  keys = sConnectionsThread.keys();
//  qDebug() << "------------------------------------------------------------------: " << connectionName;
//  qDebug() << "MSSQL connection presents after adding: " << keys.join( "\n" );

  newDataBase->ref();
  return newDataBase;
}


void QgsMssqlDatabase::dereferenceConnection( std::weak_ptr<QgsMssqlDatabaseConnection> mDatabaseConnectionRef )
{
  QMutexLocker locker( &sMutex );

  if ( std::shared_ptr<QgsMssqlDatabaseConnection> connection = mDatabaseConnectionRef.lock() )
  {
    if ( !connection->deref() )
    {
      QString connectionName = connection->connectionName();
      sConnectionsThread.remove( connectionName );
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
    return true;

  mDatabaseConnectionRef = getConnection( mUri, true );

//  if ( !mDatabaseConnection->open() )
//    return false;

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
    d = new Data;
    std::shared_ptr<QSqlQuery> query = connection->createQuery();
    d->mDatabaseConnection = connection.get();
    d->mSqlQueryWeakRef = query;
    d->ref.ref();
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
    d->mDatabaseConnection->removeSqlQuery( d->mSqlQueryWeakRef );
}

QgsMssqlQuery &QgsMssqlQuery::operator=( const QgsMssqlQuery &other )
{
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
  if ( isValidPrivate() )
  {
    return d->mDatabaseConnection->isValid( d->mSqlQueryWeakRef );
  }

  return false;
}

bool QgsMssqlQuery::isForwardOnly() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->isForwardOnly( d->mSqlQueryWeakRef );

  return false;
}

bool QgsMssqlQuery::exec( const QString &query )
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->exec( d->mSqlQueryWeakRef, query );

  return false;
}

bool QgsMssqlQuery::exec()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->exec( d->mSqlQueryWeakRef );

  return false;
}

bool QgsMssqlQuery::next()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->next( d->mSqlQueryWeakRef );

  return false;
}

bool QgsMssqlQuery::isActive() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->isActive( d->mSqlQueryWeakRef );

  return false;
}

QVariant QgsMssqlQuery::value( int index ) const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->value( d->mSqlQueryWeakRef, index );

  return QVariant();
}

QVariant QgsMssqlQuery::value( const QString &name ) const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->value( d->mSqlQueryWeakRef, name );
  return QVariant();
}

QSqlError QgsMssqlQuery::lastError() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->lastError( d->mSqlQueryWeakRef );

  return QSqlQuery().lastError();
}

void QgsMssqlQuery::setForwardOnly( bool forward )
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->setForwardOnly( d->mSqlQueryWeakRef, forward );
}

QSqlRecord QgsMssqlQuery::record() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->record( d->mSqlQueryWeakRef );

  return QSqlQuery().record();
}

void QgsMssqlQuery::clear()
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->clear( d->mSqlQueryWeakRef );
}

QString QgsMssqlQuery::lastQuery() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->lastQuery( d->mSqlQueryWeakRef );

  return QSqlQuery().lastQuery();
}

void QgsMssqlQuery::finish()
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->finish( d->mSqlQueryWeakRef );
}

int QgsMssqlQuery::size() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->size( d->mSqlQueryWeakRef );

  return QSqlQuery().size();
}

bool QgsMssqlQuery::prepare( const QString &query )
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->prepare( d->mSqlQueryWeakRef, query );

  return false;
}

void QgsMssqlQuery::addBindValue( const QVariant &val, QSql::ParamType paramType )
{
  ValidityChecker checker( d );
  if ( checker.isValid() )
    d->mDatabaseConnection->addBindValue( d->mSqlQueryWeakRef, val, paramType );
}

int QgsMssqlQuery::numRowsAffected() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->numRowsAffected( d->mSqlQueryWeakRef );

  return QSqlQuery().numRowsAffected();
}

bool QgsMssqlQuery::first()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->first( d->mSqlQueryWeakRef );

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

bool QgsMssqlDatabaseConnectionClassic::isTransaction() const {return false;}

void QgsMssqlDatabaseConnectionClassic::invalidate()
{
  if ( mSqlQueries.empty() )
    mIsInvalidated = true;
  else
  {
    mSqlQueries.clear(); //remove all the shared pointer of the queries and wait for pending one
    mIsInvalidationAsked = true;
  }
}

std::shared_ptr<QSqlQuery> QgsMssqlDatabaseConnectionClassic::createQuery()
{
  std::shared_ptr<QSqlQuery> query( new QSqlQuery( mDatabase ) );
  mSqlQueries.append( query );
  return query;
}

void QgsMssqlDatabaseConnectionClassic::confirmInvalidation()
{
  if ( mSqlQueries.isEmpty() )
    mIsInvalidated = true;
}

void QgsMssqlDatabaseConnectionClassic::addBindValue( QgsMssqlQuery::ValidityChecker &checker, const QVariant &val, QSql::ParamType paramType )
{
  std::shared_ptr<QSqlQuery> query = checker.md->mSqlQueryWeakRef.lock();
  if ( query )
    query->addBindValue( val, paramType );
}

void QgsMssqlDatabaseConnectionClassic::clear( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    query->clear();
  else if ( mIsInvalidationAsked )
    emit invalidated();
}

bool QgsMssqlDatabaseConnectionClassic::exec( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->exec( queryString );
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::exec( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->exec();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

void QgsMssqlDatabaseConnectionClassic::finish( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    query->finish();
  else if ( mIsInvalidationAsked )
    emit invalidated();
}

bool QgsMssqlDatabaseConnectionClassic::first( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->first();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isActive( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->isActive();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isValid( std::weak_ptr<QSqlQuery> queryRef ) const
{
  if ( std::shared_ptr<QSqlQuery> query = queryRef.lock() )
    return query->isValid();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

bool QgsMssqlDatabaseConnectionClassic::isForwardOnly( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->isForwardOnly();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.isForwardOnly();

}

QSqlError QgsMssqlDatabaseConnectionClassic::lastError( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->lastError();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.lastError();
}

QString QgsMssqlDatabaseConnectionClassic::lastQuery( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->lastQuery();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.lastQuery();
}

bool QgsMssqlDatabaseConnectionClassic::next( std::weak_ptr<QSqlQuery> queryRef )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->next();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

int QgsMssqlDatabaseConnectionClassic::numRowsAffected( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->numRowsAffected();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.numRowsAffected();
}

bool QgsMssqlDatabaseConnectionClassic::prepare( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->prepare( queryString );
  else if ( mIsInvalidationAsked )
    emit invalidated();

  return false;
}

QSqlRecord QgsMssqlDatabaseConnectionClassic::record( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->record();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.record();
}

void QgsMssqlDatabaseConnectionClassic::setForwardOnly( std::weak_ptr<QSqlQuery> queryRef, bool forward )
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->setForwardOnly( forward );
  else if ( mIsInvalidationAsked )
    emit invalidated();
}

int QgsMssqlDatabaseConnectionClassic::size( std::weak_ptr<QSqlQuery> queryRef ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->size();
  else if ( mIsInvalidationAsked )
    emit invalidated();

  QSqlQuery q;
  return q.size();
}

QVariant QgsMssqlDatabaseConnectionClassic::value( std::weak_ptr<QSqlQuery> queryRef, int index ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->value( index );
  else
    emit invalidated();

  return QVariant();
}

QVariant QgsMssqlDatabaseConnectionClassic::value( std::weak_ptr<QSqlQuery> queryRef, const QString &name ) const
{
  std::shared_ptr<QSqlQuery> query = queryRef.lock();
  if ( query )
    return query->value( name );
  else if ( mIsInvalidationAsked )
    emit invalidated();

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
  else if ( mIsInvalidationAsked )
  {
    emit invalidated();
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

QgsMssqlQuery::ValidityChecker::ValidityChecker( QgsMssqlQuery::Data *d ): md( d )
{}

QgsMssqlQuery::ValidityChecker::~ValidityChecker()
{
  if ( md && md->mSqlQueryWeakRef.expired() )
    md->mDatabaseConnection->invalidate();
}

bool QgsMssqlQuery::ValidityChecker::isValid() const
{
  return ( md != nullptr && ! md->mSqlQueryWeakRef.expired() );
}
