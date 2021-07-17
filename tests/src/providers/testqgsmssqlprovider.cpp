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

    void createConnection();
    void transaction();
    void openLayer();

    void threadSafeConnection();

    void concurentQueryDuringTransaction();

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

  database2.close();
}

void TestQgsMssqlProvider::transaction()
{
//  QgsMssqlDatabase database = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
//  QVERIFY( database.isValid() );
//  QVERIFY( database.open() );
//  QVERIFY( database.isOpen() );
}

void TestQgsMssqlProvider::openLayer()
{
  QString uri( QStringLiteral( "host=\"localhost\" dbname=\"qgis\" user=\"sa\" password=\"<YourStrong!Passw0rd>\" key = \"pk\" srid=4326 type=POINT schema=\"qgis_test\" table=\"someData\""
                               " (geom) sql=" ) );

  QgsVectorLayer vl( uri, QStringLiteral( "point_layer" ), QStringLiteral( "mssql" ) );

  QVERIFY( vl.isValid() );
  QCOMPARE( vl.featureCount(), 5 );
}


void TestQgsMssqlProvider::threadSafeConnection()
{
//  QgsDataSourceUri uri;
//  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
//  QSqlDatabase dataBase = QgsMssqlConnection::getDatabaseConnection( uri, "simpleConnection" );

//  QVERIFY( dataBase.isValid() );
//  QVERIFY( dataBase.open() );

//  QgsMssqlSharableConnection threadSafeConnection( uri );
//  QVERIFY( !threadSafeConnection.isOpen() );
//  threadSafeConnection.initConnection();
//  QVERIFY( threadSafeConnection.isOpen() );

//  QgsMssqlQuery query = threadSafeConnection.createQuery();

//  QVERIFY( query.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );

//  QStringList schemas;
//  while ( query.next() )
//    schemas << query.value( 0 ).toString();

//  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
//  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

//  QVERIFY( query.exec( QStringLiteral( "select name as schema_name from qgis_test.someData" ) ) );

//  QVariantList names;
//  while ( query.next() )
//    names << query.value( 0 );
}

void TestQgsMssqlProvider::concurentQueryDuringTransaction()
{
//  QgsDataSourceUri uri;
//  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
//  QSqlDatabase dataBase_1 = QgsMssqlConnection::getDatabaseConnection( uri, "simpleConnection_1" );
//  QSqlDatabase dataBase_2 = QgsMssqlConnection::getDatabaseConnection( uri, "simpleConnection_2" );

//  QVERIFY( dataBase_1.isValid() );
//  QVERIFY( dataBase_1.open() );

//  qDebug() << "Myyyyyyyyyy connection name _1:" << dataBase_1.connectionName();

//  QVERIFY( dataBase_1.transaction() );

//  QSqlQuery query_1( dataBase_1 );
//  QVERIFY( query_1.exec( QStringLiteral( "INSERT INTO  qgis_test.someData(pk,name) VALUES(100,'a name')" ) ) );

//  qDebug() << query_1.lastError().text();

//  QVERIFY( dataBase_2.isValid() );
//  QVERIFY( dataBase_2.open() );

//  qDebug() << "Myyyyyyyyyy connection name _2:" << dataBase_2.connectionName();

//  QSqlQuery query_2( dataBase_2 );
//  QVERIFY( query_2.exec( QStringLiteral( "select * from qgis_test.someData" ) ) );
//  query_2.next();


//  QVERIFY( dataBase_1.rollback() );
}


QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
