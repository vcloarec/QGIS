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


//qgis includes...
#include <qgis.h>
#include <qgsapplication.h>
#include <qgsmssqltransaction.h>
#include <qgsmssqlconnection.h>

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

    void threadSafeConnection();

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

void queryConnection( QString statement, QgsMssqlQuery query )
{

}

void TestQgsMssqlProvider::threadSafeConnection()
{
  QgsDataSourceUri uri;
  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QSqlDatabase dataBase = QgsMssqlConnection::getDatabaseConnection( uri, "simpleConnection" );

  QVERIFY( dataBase.isValid() );
  QVERIFY( dataBase.open() );

  QgsMssqlSharableConnection threadSafeConnection( uri );
  QVERIFY( !threadSafeConnection.isOpen() );
  threadSafeConnection.initConnection();
  QVERIFY( threadSafeConnection.isOpen() );

  QgsMssqlQuery query = threadSafeConnection.createQuery();

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

  QVERIFY( query.exec( QStringLiteral( "select name as schema_name from qgis_test.someData" ) ) );

  QVariantList names;
  while ( query.next() )
    names << query.value( 0 );


}

QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
