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


