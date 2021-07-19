/***************************************************************************
  qgis_mssqlprovidertest.cpp - %{Cpp:License:ClassName}

 ---------------------
 begin                : 16.3.2021
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


#include "qgstest.h"

#include <QSqlError>

//qgis includes...
#include <qgis.h>
#include <qgsapplication.h>
#include <qgsmssqltransaction.h>
#include <qgsmssqlconnection.h>
#include <qgsvectorlayer.h>
#include <qtconcurrentrun.h>

#include "qgsmssqldatabase.h"

/**
 * \ingroup UnitTests
 * This is a unit test for the gdal provider
 */
class TestQgsMssqlProvider : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {}// will be called before each testfunction is executed.
    void cleanup() {}// will be called after every testfunction.

    void openLayer();
    void createConnection();
    void concurentQueryDuringTransaction();

    void transaction();

  private:

};

//runs before all tests
void TestQgsMssqlProvider::initTestCase()
{
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
}


//runs after all tests
void TestQgsMssqlProvider::cleanupTestCase()
{
  QgsApplication::exitQgis();
}


void TestQgsMssqlProvider::createConnection()
{
  QgsMssqlDatabase database1 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QgsMssqlDatabase database2;

  QgsMssqlDatabase database3 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );

  QVERIFY( database1.isValid() );
  QVERIFY( database1.open() );
  QVERIFY( database1.isOpen() );

  QVERIFY( !database2.isValid() );
  QVERIFY( !database2.open() );
  QVERIFY( !database2.isOpen() );

  database2 = database1;

  QVERIFY( database2.isValid() );
  QVERIFY( database2.isOpen() );

  QgsMssqlQuery query( database2 );

  QVERIFY( query.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );

  QStringList schemas;
  while ( query.next() )
    schemas << query.value( 0 ).toString();

  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

  database1.close();

  QVERIFY( database1.isValid() );
  QVERIFY( !database1.isOpen() );

  QVERIFY( database2.isValid() );
  QVERIFY( !database2.isOpen() );

  QVERIFY( database2.open() );
  QVERIFY( database2.isOpen() );
  QVERIFY( database1.isOpen() );

  query = QgsMssqlQuery( database1 );

  QgsMssqlQuery otherQuery = query;
  schemas.clear();

  QVERIFY( otherQuery.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );
  while ( otherQuery.next() )
    schemas << otherQuery.value( 0 ).toString();
  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

  database2.close();
}

void TestQgsMssqlProvider::transaction()
{
//  QgsMssqlDatabase database = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
//  QVERIFY( database.isValid() );
//  QVERIFY( database.open() );
//  QVERIFY( database.isOpen() );

  //QVERIFY( database.transaction() );
}

void TestQgsMssqlProvider::openLayer()
{
  QString uri( QStringLiteral( "host=\"localhost\" dbname=\"qgis\" user=\"sa\" password=\"<YourStrong!Passw0rd>\" key = \"pk\" srid=4326 type=POINT schema=\"qgis_test\" table=\"someData\""
                               " (geom) sql=" ) );


  QgsVectorLayer vl( uri, QStringLiteral( "point_layer" ), QStringLiteral( "mssql" ) );

  QVERIFY( vl.isValid() );
  QCOMPARE( vl.featureCount(), 5 );

  QVERIFY( vl.isValid() );
}


static void repeatedQueryFromOtherThread( const QgsDataSourceUri &uri, const QString &queryString, bool forwardOnly )
{
  QgsMssqlDatabase connection = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QVERIFY( connection.isValid() );
  QVERIFY( connection.open() );
  QVERIFY( connection.isOpen() );

  QThread::msleep( 0 );

  QgsMssqlQuery query( connection );
  query.setForwardOnly( forwardOnly );
  query.exec( queryString );
  bool r = query.next();

  for ( int i = 0; i < 10; ++i )
  {
    if ( !query.isValid() )
    {
      if ( query.isForwardOnly() )
      {
        query.exec( queryString );
        query.next();
      }
      else
        query.first();
    }

    QVariant var = query.value( 0 );
    if ( !var.isValid() )
    {
      query.first();
      qDebug() << "query " << i << " Invalid ";
    }
    else
      qDebug() << "query " << i << " value: " << var;
    query.next();

    QThread::msleep( 50 );
  }

  qDebug() << "concurrent query end";
}


void TestQgsMssqlProvider::concurentQueryDuringTransaction()
{
  for ( int i = 0; i < 1; ++i )
  {
    QgsMssqlDatabase database_1 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
    QgsMssqlDatabase dataBase_2 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );

    const QgsDataSourceUri uri;
    QString statement = QStringLiteral( "select * from qgis_test.someData" );
    QFuture<void> future_1 = QtConcurrent::run( repeatedQueryFromOtherThread, uri, statement, false );

    // lets the concurrent query starting and working a bit before starting a trnasction
    QThread::msleep( 100 );

    QVERIFY( database_1.isValid() );
    QVERIFY( database_1.open() );

    QVERIFY( database_1.transaction() );
    QVERIFY( !dataBase_2.isValid() );

    QgsMssqlQuery query_1( database_1 );
    QVERIFY( query_1.exec( QStringLiteral( "INSERT INTO  qgis_test.someData(pk,name) VALUES(100,'a name')" ) ) );

    dataBase_2 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );

    QVERIFY( dataBase_2.isValid() );
    QVERIFY( dataBase_2.open() );

    QgsMssqlQuery query_2( dataBase_2 );
    QVERIFY( query_2.exec( QStringLiteral( "select * from qgis_test.someData" ) ) );
    QVERIFY( query_2.next() );

    future_1.waitForFinished();
  }


//  ( dataBase_1.rollback() );
}


QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
