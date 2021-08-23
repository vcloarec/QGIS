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
#include <QRandomGenerator>

//qgis includes...
#include <qgis.h>
#include <qgsapplication.h>
#include <qgsmssqltransaction.h>
#include <qgsmssqlconnection.h>
#include <qgsvectorlayer.h>
#include <qtconcurrentrun.h>

#include "qgsmssqldatabase.h"
#include "qgsproject.h"

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
    void openLayer();

    void projectTransaction();

    void concurentQueryDuringTransaction();

  private:

    QStringList mSomeDataWktGeom;
    QStringList mSomeDataPolyWktGeom;
    QList<QVariantList> mSomeDataAttributes;

};

//runs before all tests
void TestQgsMssqlProvider::initTestCase()
{
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();

  mSomeDataWktGeom << QStringLiteral( "Point (-70.33199999999999363 66.32999999999999829)" )
                   << QStringLiteral( "Point (-68.20000000000000284 70.79999999999999716)" )
                   << QString()
                   << QStringLiteral( "Point (-65.31999999999999318 78.29999999999999716)" )
                   << QStringLiteral( "Point (-71.12300000000000466 78.23000000000000398)" );

  QVariantList varList;
  varList << 1ll << 100 << "Orange" <<  "oranGe" <<  "1" << QDateTime( QDate( 2020, 05, 03 ), QTime( 12, 13, 14 ) ) << QDate( 2020, 05, 03 ) << QTime( 12, 13, 14 ) ;
  mSomeDataAttributes << varList;
  varList.clear();
  varList << 2ll << 200 << "Apple" <<  "Apple" <<  "2" << QDateTime( QDate( 2020, 05, 04 ), QTime( 12, 14, 14 ) ) << QDate( 2020, 05, 04 ) << QTime( 12, 14, 14 ) ;
  mSomeDataAttributes << varList;
  varList.clear();
  varList << 3ll << 300 << "Pear" <<  "PEaR" <<  "3" << QDateTime() << QDate() << QTime();
  mSomeDataAttributes << varList;
  varList.clear();
  varList << 4ll << 400 << "Honey" <<  "Honey" <<  "4" << QDateTime( QDate( 2021, 05, 04 ), QTime( 13, 13, 14 ) ) << QDate( 2021, 05, 04 ) << QTime( 13, 13, 14 ) ;
  mSomeDataAttributes << varList;
  varList.clear();
  varList << 5ll << -200 << "" <<  "NuLl" <<  "5" << QDateTime( QDate( 2020, 05, 04 ), QTime( 12, 13, 14 ) ) << QDate( 2020, 05, 02 ) << QTime( 12, 13, 1 ) ;
  mSomeDataAttributes << varList;


  mSomeDataPolyWktGeom << QStringLiteral( "Polygon ((-69 81.40000000000000568, -69 80.20000000000000284, -73.70000000000000284 80.20000000000000284, -73.70000000000000284 76.29999999999999716, -74.90000000000000568 76.29999999999999716, -74.90000000000000568 81.40000000000000568, -69 81.40000000000000568))" )
                       << QStringLiteral( "Polygon ((-67.59999999999999432 81.20000000000000284, -66.29999999999999716 81.20000000000000284, -66.29999999999999716 76.90000000000000568, -67.59999999999999432 76.90000000000000568, -67.59999999999999432 81.20000000000000284))" )
                       << QStringLiteral( "Polygon ((-68.40000000000000568 75.79999999999999716, -67.5 72.59999999999999432, -68.59999999999999432 73.70000000000000284, -70.20000000000000284 72.90000000000000568, -68.40000000000000568 75.79999999999999716))" )
                       << QString();

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

void TestQgsMssqlProvider::openLayer()
{
  QString uri( QStringLiteral( "host=\"localhost\" dbname=\"qgis\" user=\"sa\" password=\"<YourStrong!Passw0rd>\" key = \"pk\" srid=4326 type=POINT schema=\"qgis_test\" table=\"someData\""
                               " (geom) sql=" ) );


  QgsVectorLayer vl( uri, QStringLiteral( "point_layer" ), QStringLiteral( "mssql" ) );

  QVERIFY( vl.isValid() );
  QCOMPARE( vl.featureCount(), 5 );

  QVERIFY( vl.isValid() );
}

void TestQgsMssqlProvider::projectTransaction()
{
  QString uriPoint( QStringLiteral( "host=\"localhost\" dbname=\"qgis\" user=\"sa\" password=\"<YourStrong!Passw0rd>\" key = \"pk\" srid=4326 type=POINT schema=\"qgis_test\" table=\"someData\""
                                    " (geom) sql=" ) );

  QString uriPolygon( QStringLiteral( "host=\"localhost\" dbname=\"qgis\" user=\"sa\" password=\"<YourStrong!Passw0rd>\" key = \"pk\" srid=4326 type=POLYGON schema=\"qgis_test\" table=\"some_poly_data\""
                                      " (geom) sql=" ) );

  QgsProject project;

  QgsVectorLayer *vectorLayerPoint = new QgsVectorLayer( uriPoint, QStringLiteral( "point_layer" ), QStringLiteral( "mssql" ) );
  QgsVectorLayer *vectorLayerPoly = new QgsVectorLayer( uriPolygon, QStringLiteral( "poly_layer" ), QStringLiteral( "mssql" ) );
  project.addMapLayer( vectorLayerPoint );
  project.addMapLayer( vectorLayerPoly );

  project.setAutoTransaction( true );

  QVERIFY( vectorLayerPoint->startEditing() );
  QVERIFY( vectorLayerPoint->isEditable() );
  QVERIFY( vectorLayerPoly->isEditable() );

  // test geometries and attributes of exisintg layer
  QgsFeatureIterator featIt = vectorLayerPoint->getFeatures();
  QgsFeature feat;
  QList<QVariantList> attributes;
  QStringList geoms;
  while ( featIt.nextFeature( feat ) )
  {
    attributes.append( feat.attributes().toList() );
    geoms.append( feat.geometry().asWkt() );
  }
  QVERIFY( attributes == mSomeDataAttributes );
  QVERIFY( geoms == mSomeDataWktGeom );

  featIt = vectorLayerPoly->getFeatures();
  geoms.clear();
  while ( featIt.nextFeature( feat ) )
    geoms.append( feat.geometry().asWkt() );

  QVERIFY( geoms == mSomeDataPolyWktGeom );

  // add a point to the layer point
  feat = QgsFeature();
  feat.setFields( vectorLayerPoint->fields(), true );
  feat.setAttribute( 0, 10 );
  feat.setGeometry( QgsGeometry( new QgsPoint( -71, 67 ) ) );

  QStringList newGeoms = mSomeDataWktGeom;
  newGeoms << QStringLiteral( "Point (-71 67)" );
  QList<QVariantList> newAttributes = mSomeDataAttributes;
  QVariantList attrList;
  attrList << 10ll << 0 << "" << "" << "" << QDateTime() << QDate() << QTime();
  newAttributes.append( attrList );

  vectorLayerPoint->addFeature( feat );

  QCOMPARE( vectorLayerPoint->featureCount(), 6 );

  featIt = vectorLayerPoint->getFeatures();
  attributes.clear();
  geoms.clear();
  while ( featIt.nextFeature( feat ) )
  {
    attributes.append( feat.attributes().toList() );
    geoms.append( feat.geometry().asWkt() );
  }
  QVERIFY( attributes == newAttributes );
  QVERIFY( geoms == newGeoms );

  vectorLayerPoint->rollBack();

  QCOMPARE( vectorLayerPoint->featureCount(), 5 );
}

static void repeatedQueryFromOtherThread( const QString &queryString, bool forwardOnly )
{
  QgsMssqlDatabase database = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
  database.open();

  QThread::msleep( QRandomGenerator::global()->bounded( 100 ) );

  QgsMssqlQuery query( database );
  query.setForwardOnly( forwardOnly );
  query.exec( queryString );

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
      query.first();

    query.next();

    QThread::msleep( QRandomGenerator::global()->bounded( 30 ) );
  }
}


void TestQgsMssqlProvider::concurentQueryDuringTransaction()
{
  for ( int i = 0; i < 1; ++i )
  {
    {
      QgsMssqlDatabase database_1 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );
      QgsMssqlDatabase dataBase_2 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );

      const QgsDataSourceUri uri;
      QString statement = QStringLiteral( "select * from qgis_test.someData" );
      QFuture<void> future_1 = QtConcurrent::run( repeatedQueryFromOtherThread, QString( "query_1" ), statement, false );

      // lets the concurrent queries starting and working a bit before starting a new one and then a transaction
      QThread::msleep( QRandomGenerator::global()->bounded( 200 ) );

      QFuture<void> future_2 = QtConcurrent::run( repeatedQueryFromOtherThread, QString( "query_2" ), statement, false );

      QVERIFY( database_1.isValid() );
      QVERIFY( database_1.open() );

      QVERIFY( database_1.transaction() );

      QFuture<void> future_5 = QtConcurrent::run( repeatedQueryFromOtherThread, QString( "query_5" ), statement, false );

      QVERIFY( !dataBase_2.isValid() );

      QgsMssqlQuery query_1( database_1 );
      QVERIFY( query_1.exec( QStringLiteral( "INSERT INTO  qgis_test.someData(pk,name) VALUES(100,'a name')" ) ) );

      QFuture<void> future_3 = QtConcurrent::run( repeatedQueryFromOtherThread, QString( "query_3" ), statement, false );

      QThread::msleep( QRandomGenerator::global()->bounded( 100 ) );
      QFuture<void> future_4 = QtConcurrent::run( repeatedQueryFromOtherThread, QString( "query_4" ), statement, false );

      dataBase_2 = QgsMssqlDatabase::database( "", "localhost", "qgis", "sa", "<YourStrong!Passw0rd>" );

      QVERIFY( dataBase_2.isValid() );
      QVERIFY( dataBase_2.open() );

      QgsMssqlQuery query_2( dataBase_2 );
      QVERIFY( query_2.exec( QStringLiteral( "select * from qgis_test.someData" ) ) );
      int rowCount = 0;
      while ( query_2.next() )
        rowCount++;

      QCOMPARE( rowCount, 6 );

      future_1.waitForFinished();
      future_2.waitForFinished();
      future_3.waitForFinished();
      future_4.waitForFinished();
      future_5.waitForFinished();

      database_1.rollback();
    }
  }
}


QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
