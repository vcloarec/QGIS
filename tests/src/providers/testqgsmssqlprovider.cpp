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

    void connectionCreation();
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

void TestQgsMssqlProvider::connectionCreation()
{
  QSqlDatabase db = QSqlDatabase::addDatabase( QStringLiteral( "QgsODBCProxy" ) );

  QgsDataSourceUri uri;
  uri.setConnection( "localhost", "", "qgis", "sa", "<YourStrong!Passw0rd>" );
  QSqlDatabase dataBase = QgsMssqlConnection::getDatabaseConnection( uri, uri.connectionInfo() );

  QVERIFY( dataBase.isValid() );
  QVERIFY( dataBase.open() );


}



QGSTEST_MAIN( TestQgsMssqlProvider )
#include "testqgsmssqlprovider.moc"
