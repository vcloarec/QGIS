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

#include "qgsdatasourceuri.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QMutex QgsMssqlDatabase::sMutex { QMutex::Recursive };
#else
QRecursiveMutex QgsMssqlDatabase::sMutex;
#endif

QMap < QString, QgsMssqlDatabaseConnection *> QgsMssqlDatabase::sConnections;

QgsMssqlDatabase::QgsMssqlDatabase( QgsMssqlDatabaseConnection *connection ):
  mDatabaseConnection( connection )
{}

QgsMssqlDatabase::QgsMssqlDatabase( const QgsMssqlDatabase &other )
{
  if ( other.mDatabaseConnection )
  {
    mDatabaseConnection = other.mDatabaseConnection;
    mDatabaseConnection->ref();
  }
}

QgsMssqlDatabase::~QgsMssqlDatabase()
{
  dereferenceConnection( mDatabaseConnection );
}

QgsMssqlDatabase &QgsMssqlDatabase::operator=( const QgsMssqlDatabase &other )
{
  dereferenceConnection( mDatabaseConnection );
  mDatabaseConnection = other.mDatabaseConnection;
  mDatabaseConnection->ref();
  return *this;
}

QgsMssqlDatabaseConnection *QgsMssqlDatabase::getConnection( const QgsDataSourceUri &uri )
{
  QMutexLocker locker( &sMutex );

  QString connectionName = QStringLiteral( "%1:0x%2" )
                           .arg( uri.connectionInfo() )
                           .arg( reinterpret_cast<quintptr>( QThread::currentThread() ), 2 * QT_POINTER_SIZE, 16, QLatin1Char( '0' ) );

  if ( sConnections.contains( connectionName ) )
  {
    sConnections.value( connectionName )->ref();
    return sConnections.value( connectionName );
  }

  std::unique_ptr<QgsMssqlDatabaseConnection> newDataBase( new QgsMssqlDatabaseConnectionClassic( uri, connectionName ) );
  newDataBase->init();
  sConnections.insert( connectionName, newDataBase.get() );

  newDataBase->ref();
  return newDataBase.release();
}

void QgsMssqlDatabase::dereferenceConnection( QgsMssqlDatabaseConnection *databaseConnection )
{
  QMutexLocker locker( &sMutex );

  if ( databaseConnection && databaseConnection->deref() == 0 )
  {
    QString connectionName = sConnections.key( databaseConnection );
    sConnections.remove( connectionName );
    delete databaseConnection;
    QSqlDatabase::removeDatabase( connectionName );
  }
}

bool QgsMssqlDatabase::open()
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->open();

  return false;
}

void QgsMssqlDatabase::close()
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->close();
}

bool QgsMssqlDatabase::isOpen() const
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->isOpen();

  return false;
}

bool QgsMssqlDatabase::isValid() const
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->isValid();

  return false;
}

QSqlError QgsMssqlDatabase::lastError() const
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->lastError();

  return QSqlDatabase().lastError();
}

QStringList QgsMssqlDatabase::tables( QSql::TableType type ) const
{
  if ( mDatabaseConnection )
    return mDatabaseConnection->tables( type );

  return QSqlDatabase().tables( type );
}

bool QgsMssqlDatabase::transaction()
{
  //TODO
}

QgsMssqlQuery QgsMssqlDatabase::createQuery()
{
  return QgsMssqlQuery( *this );
}


QgsMssqlDatabase QgsMssqlDatabase::database( const QgsDataSourceUri &uri )
{
  return QgsMssqlDatabase( getConnection( uri ) );
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
  if ( database.mDatabaseConnection )
  {
    d = new Data;
    std::shared_ptr<QSqlQuery> query = database.mDatabaseConnection->createQuery();
    d->mDatabaseConnection = database.mDatabaseConnection;
    d->mSqlQuery = query.get();
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
  if ( !d->ref.deref() )
    d->mDatabaseConnection->removeSqlQuery( d->mSqlQuery );
}

QgsMssqlQuery &QgsMssqlQuery::operator=( const QgsMssqlQuery &other )
{
  d = other.d;
  d->ref.ref();
  return *this;
}

bool QgsMssqlQuery::isValidPrivate() const
{
  return d && !d->mSqlQueryWeakRef.expired();
}

bool QgsMssqlQuery::isValid() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->isValid( d->mSqlQuery );

  return false;
}

bool QgsMssqlQuery::exec( const QString &query )
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->exec( d->mSqlQuery, query );

  return false;
}

bool QgsMssqlQuery::exec()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->exec( d->mSqlQuery );

  return false;
}

bool QgsMssqlQuery::next()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->next( d->mSqlQuery );

  return false;
}

bool QgsMssqlQuery::isActive() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->isActive( d->mSqlQuery );

  return false;
}

QVariant QgsMssqlQuery::value( int index ) const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->value( d->mSqlQuery, index );

  return QVariant();
}

QVariant QgsMssqlQuery::value( const QString &name ) const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->value( d->mSqlQuery, name );
  return QVariant();
}

QSqlError QgsMssqlQuery::lastError() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->lastError( d->mSqlQuery );

  return QSqlQuery().lastError();
}

void QgsMssqlQuery::setForwardOnly( bool forward )
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->setForwardOnly( d->mSqlQuery, forward );
}

QSqlRecord QgsMssqlQuery::record() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->record( d->mSqlQuery );

  return QSqlQuery().record();
}

void QgsMssqlQuery::clear()
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->clear( d->mSqlQuery );
}

QString QgsMssqlQuery::lastQuery() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->lastQuery( d->mSqlQuery );

  return QSqlQuery().lastQuery();
}

void QgsMssqlQuery::finish()
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->finish( d->mSqlQuery );
}

int QgsMssqlQuery::size() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->size( d->mSqlQuery );

  return QSqlQuery().size();
}

bool QgsMssqlQuery::prepare( const QString &query )
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->prepare( d->mSqlQuery, query );

  return false;
}

void QgsMssqlQuery::addBindValue( const QVariant &val, QSql::ParamType paramType )
{
  if ( isValidPrivate() )
    d->mDatabaseConnection->addBindValue( d->mSqlQuery, val, paramType );
}

int QgsMssqlQuery::numRowsAffected() const
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->numRowsAffected( d->mSqlQuery );

  return QSqlQuery().numRowsAffected();
}

bool QgsMssqlQuery::first()
{
  if ( isValidPrivate() )
    return d->mDatabaseConnection->first( d->mSqlQuery );

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

std::shared_ptr<QSqlQuery> QgsMssqlDatabaseConnectionClassic::createQuery()
{
  std::shared_ptr<QSqlQuery> query( new QSqlQuery( mDatabase ) );
  mSqlQueries.append( query );
  return query;
}

void QgsMssqlDatabaseConnectionClassic::addBindValue( QSqlQuery *query, const QVariant &val, QSql::ParamType paramType )
{
  query->addBindValue( val, paramType );
}

void QgsMssqlDatabaseConnectionClassic::clear( QSqlQuery *query )
{
  query->clear();
}

bool QgsMssqlDatabaseConnectionClassic::exec( QSqlQuery *query, const QString &queryString )
{
  return query->exec( queryString );
}

bool QgsMssqlDatabaseConnectionClassic::exec( QSqlQuery *query )
{
  return query->exec();
}

void QgsMssqlDatabaseConnectionClassic::finish( QSqlQuery *query )
{
  query->finish();
}

bool QgsMssqlDatabaseConnectionClassic::first( QSqlQuery *query )
{
  return query->first();
}

bool QgsMssqlDatabaseConnectionClassic::isActive( QSqlQuery *query ) const
{
  return query->isActive();
}

bool QgsMssqlDatabaseConnectionClassic::isValid( QSqlQuery *query ) const
{
  return query->isValid();
}

QSqlError QgsMssqlDatabaseConnectionClassic::lastError( QSqlQuery *query ) const
{
  return query->lastError();
}

QString QgsMssqlDatabaseConnectionClassic::lastQuery( QSqlQuery *query ) const
{
  return query->lastQuery();
}

bool QgsMssqlDatabaseConnectionClassic::next( QSqlQuery *query )
{
  return query->next();
}

int QgsMssqlDatabaseConnectionClassic::numRowsAffected( QSqlQuery *query ) const
{
  return query->numRowsAffected();
}

bool QgsMssqlDatabaseConnectionClassic::prepare( QSqlQuery *query, const QString &queryString )
{
  return query->prepare( queryString );
}

QSqlRecord QgsMssqlDatabaseConnectionClassic::record( QSqlQuery *query ) const
{
  return query->record();
}

void QgsMssqlDatabaseConnectionClassic::setForwardOnly( QSqlQuery *query, bool forward )
{
  return query->setForwardOnly( forward );
}

int QgsMssqlDatabaseConnectionClassic::size( QSqlQuery *query ) const
{
  return query->size();
}

QVariant QgsMssqlDatabaseConnectionClassic::value( QSqlQuery *query, int index ) const
{
  return query->value( index );
}

QVariant QgsMssqlDatabaseConnectionClassic::value( QSqlQuery *query, const QString &name ) const
{
  return query->value( name );
}

void QgsMssqlDatabaseConnectionClassic::removeSqlQuery( QSqlQuery *sqlQuery )
{
  for ( int i = 0; i < mSqlQueries.count(); ++i )
  {
    if ( mSqlQueries.at( i ).get() == sqlQuery )
    {
      mSqlQueries.removeAt( i );
      return;
    }
  }
}

