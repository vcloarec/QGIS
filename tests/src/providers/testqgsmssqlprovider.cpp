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


#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlRecord>
#include <QFutureWatcher>
#include "qgstest.h"


//qgis includes...
#include <qgis.h>
#include <qgsapplication.h>
#include <qgsmssqltransaction.h>
#include <qgsmssqlconnection.h>
#include <qtconcurrentrun.h>

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

    void testMultipleQuery();
    void testIsolationLevel();
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

static void queryFromOtherThread( const QgsDataSourceUri &uri, const QString &queryString, QVariantList *result )
{
  QSqlDatabase connection = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), true );
  QVERIFY( connection.isValid() );
  QVERIFY( connection.open() );
  QVERIFY( !connection.isOpenError() );
  QVERIFY( connection.isOpen() );

  QSqlQuery query( connection );
  query.exec( queryString );
  QVERIFY( query.next() );

  QSqlRecord record = query.record();

  for ( int i = 0; i < record.count(); ++i )
    result->append( record.value( i ) );
}

void TestQgsMssqlProvider::testIsolationLevel()
{
#if 0
  QgsDataSourceUri uri;
  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QSqlDatabase connection_1 = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), false );
  QSqlDatabase connection_2 = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), false );

  QVERIFY( connection_1.isValid() );
  QVERIFY( connection_1.open() );
  QVERIFY( !connection_1.isOpenError() );
  QVERIFY( connection_1.isOpen() );

  QSqlQuery settingsQuerry( connection_1 );
  settingsQuerry.exec( "ALTER DATABASE qgis SET READ_COMMITTED_SNAPSHOT ON" );
  qDebug() << "******* Result settings:" << settingsQuerry.lastError();

  QVERIFY( connection_2.isValid() );
  QVERIFY( connection_2.open() );
  QVERIFY( !connection_2.isOpenError() );
  QVERIFY( connection_2.isOpen() );

  QVERIFY( connection_1.transaction() );
  QSqlQuery query_1( connection_1 );
  QVERIFY( query_1.exec( "UPDATE qgis_test.someData SET cnt=111" ) );
  QSqlQuery query_1_1( connection_1 );
  QVERIFY( query_1_1.exec( "SELECT * FROM qgis_test.someData WHERE cnt=111" ) );
  QVERIFY( query_1_1.next() );
  qDebug() << "Value *************** " << query_1_1.value( "cnt" );

  QSqlQuery query_2( connection_2 );
  QVERIFY( query_2.exec( QStringLiteral( "SELECT * FROM qgis_test.someData WHERE cnt=123" ) ) );
  query_2.next();
  query_2.next();
  qDebug() << "Value from query_2 " << query_2.value( "cnt" );

  QStringList concurrentQueries;
  concurrentQueries << QStringLiteral( "SELECT * FROM qgis_test.someData" );

  QFuture<void> future = QtConcurrent::run( queriesFromOtherThread, uri, concurrentQueries );
  future.waitForFinished(); // If SET READ_COMMITTED_SNAPSHOT=OFF, wait for ever....

  connection_1.rollback();

#endif
}

void TestQgsMssqlProvider::testMultipleQuery()
{
  QgsDataSourceUri uri;
  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QSqlDatabase dataBase = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), true );

  QVERIFY( dataBase.isValid() );
  QVERIFY( dataBase.open() );
  QVERIFY( !dataBase.isOpenError() );
  QVERIFY( dataBase.isOpen() );

  QString statement_1 = QStringLiteral( "select s.name as schema_name from sys.schemas s" );
  QString statement_2 = QStringLiteral( "select * from qgis_test.someData" );
  QString statement_3 = QStringLiteral( "select * from qgis_test.someData" );

  QSqlQuery query_1 = dataBase.exec( statement_1 );
  QSqlQuery query_2 = dataBase.exec( statement_2 );
  QSqlQuery query_3 = dataBase.exec( statement_3 );

  QVERIFY( query_1.next() );
  QVERIFY( query_2.next() );
  QVERIFY( query_3.next() );

  QVERIFY( query_1.isValid() );
  QVERIFY( query_2.isValid() );
  QVERIFY( query_3.isValid() );

  QVERIFY( query_1.isActive() );
  QVERIFY( query_2.isActive() );
  QVERIFY( query_3.isActive() );

  QVERIFY( query_1.isSelect() );
  QVERIFY( query_2.isSelect() );
  QVERIFY( query_3.isSelect() );


  for ( int i = 0; i < 1000; i++ )
  {
    query_1.next();

    query_2.next();

    query_3.next();
    query_3.next();

    if ( !query_1.isValid() )
    {
      QVERIFY( query_1.first() );
    }

    if ( !query_2.isValid() )
    {
      QVERIFY( query_2.first() );
    }

    if ( !query_3.isValid() )
    {
      QVERIFY( query_3.first() );
    }

    QVERIFY( query_1.isValid() );
    QVERIFY( query_2.isValid() );
    QVERIFY( query_3.isValid() );

    QVERIFY( query_1.isActive() );
    QVERIFY( query_2.isActive() );
    QVERIFY( query_3.isActive() );

    if ( query_2.value( "pk" ).toInt() != 5 )
      QCOMPARE( query_2.value( "pk" ).toInt() * 100, query_2.value( "cnt" ).toInt() );

    if ( query_3.value( "pk" ).toInt() != 5 )
      QCOMPARE( query_3.value( "pk" ).toInt() * 100, query_3.value( "cnt" ).toInt() );

    if ( query_3.value( "pk" ).toInt() == query_2.value( "pk" ).toInt() )
    {
      for ( int c = 0; c < 8; ++c )
        QCOMPARE( query_2.value( c ), query_3.value( c ) );
    }
  }
}

void TestQgsMssqlProvider::transaction()
{
  QgsDataSourceUri uri;
  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QSqlDatabase dataBase = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), true );

#if 0 //used to test it is ok with non proxy driver
  QSqlDatabase normalODBCdatabase = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo(), false );
  QVERIFY( normalODBCdatabase.isValid() );
  QVERIFY( normalODBCdatabase.open() );
  QVERIFY( !normalODBCdatabase.isOpenError() );
  QVERIFY( normalODBCdatabase.isOpen() );
  QSqlQuery queryNormalODBC( normalODBCdatabase );

  QVERIFY( !queryNormalODBC.isValid() );
  QVERIFY( queryNormalODBC.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );
  QStringList schemas;
  while ( queryNormalODBC.next() )
  {
    QVERIFY( queryNormalODBC.isValid() );
    schemas << queryNormalODBC.value( 0 ).toString();
  }

  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

#endif

  //----- No transaction

  QVERIFY( dataBase.isValid() );
  QVERIFY( dataBase.open() );
  QVERIFY( !dataBase.isOpenError() );
  QVERIFY( dataBase.isOpen() );

  QVERIFY( dataBase.driver()->hasFeature( QSqlDriver::Transactions ) );

  QSqlQuery query( dataBase );
  QVERIFY( !query.isValid() );
  QVERIFY( query.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );
  QStringList schemas;
  while ( query.next() )
  {
    QVERIFY( query.isValid() );
    schemas << query.value( 0 ).toString();
  }

  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

  // Read a table
  QVERIFY( query.exec( QStringLiteral( "select * from qgis_test.someData" ) ) );
  for ( int i = 0; i < 100; i++ )
  {
    query.next();
    if ( !query.isValid() )
    {
      QVERIFY( query.first() );
    }
    if ( query.value( "pk" ).toInt() != 5 )
      QCOMPARE( query.value( "pk" ).toInt() * 100, query.value( "cnt" ).toInt() );
  }

  //----- Begin transaction

  // Read some general data
  QVERIFY( dataBase.transaction() );
  QVERIFY( dataBase.driver()->hasFeature( QSqlDriver::Transactions ) );
  query = QSqlQuery( dataBase );
  QVERIFY( !query.isValid() );
  QVERIFY( query.exec( QStringLiteral( "select s.name as schema_name from sys.schemas s" ) ) );
  schemas.clear();
  QVERIFY( query.next() );
  schemas << query.value( 0 ).toString();
  while ( query.next() )
  {
    QVERIFY( query.isValid() );
    schemas << query.value( 0 ).toString();
  }

  QVERIFY( schemas.contains( QStringLiteral( "dbo" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "guest" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "INFORMATION_SCHEMA" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "sys" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "qgis_test" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_owner" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_accessadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_securityadmin" ) ) );
  QVERIFY( schemas.contains( QStringLiteral( "db_ddladmin" ) ) );

  // Read a table
  QVERIFY( query.exec( QStringLiteral( "select * from qgis_test.someData" ) ) );
  for ( int i = 0; i < 100; i++ )
  {
    query.next();
    if ( !query.isValid() )
      QVERIFY( query.first() );

    QVERIFY( query.value( 0 ).isValid() );
    QVERIFY( query.value( "pk" ).isValid() );

    if ( query.value( "pk" ).toInt() != 5 )
      QCOMPARE( query.value( "pk" ).toInt() * 100, query.value( "cnt" ).toInt() );
  }

  //Edit the table
  QVERIFY( query.exec( "UPDATE qgis_test.someData SET cnt=123" ) );

  QVariantList result;
  // Try read from another thread
  QFuture<void> future = QtConcurrent::run( queryFromOtherThread, uri, QStringLiteral( "select cnt from qgis_test.someData" ), &result );
  future.waitForFinished();

  QCOMPARE( result.count(), 1 );
  QCOMPARE( result.at( 0 ), 123 );

  dataBase.rollback();

  query.exec( "select cnt from qgis_test.someData" );
  query.next();
  QCOMPARE( query.value( 0 ), 100 );

}



QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
