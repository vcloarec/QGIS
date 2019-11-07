/***************************************************************************
                         testqgsmeshlayer.cpp
                         --------------------
    begin                : April 2018
    copyright            : (C) 2018 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"
#include <QObject>
#include <QString>

//qgis includes...
#include "qgsmeshlayer.h"
#include "qgstriangularmesh.h"
#include "qgsmeshlayerutils.h"

#include "qgsmeshtracerenderer.h"

/**
 * \ingroup UnitTests
 * This is a unit test for a mesh layer
 */
class TestQgsMeshTrace : public QObject
{
    Q_OBJECT

  public:
    TestQgsMeshTrace() = default;

  private:
    QgsMeshLayer *mMeshLayer;
    QgsMeshDataProvider *mDataProvider;

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void vectorInterpolatorTest();

};



void TestQgsMeshTrace::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsApplication::showSettings();

  mMeshLayer = new QgsMeshLayer( "/home/vincent/prog/cpp/MDAL/tests/data/grib/Madagascar.wind.7days.grb", "test mesh", "mdal" );
  QVERIFY( mMeshLayer->isValid() );
  mMeshLayer->reload();
  mDataProvider = mMeshLayer->dataProvider();

  auto velocityDataSetGroup = mDataProvider->datasetGroupMetadata( 0 );

  QVERIFY( velocityDataSetGroup.name() == "wind [m/s]" );

  QgsRenderContext rc;
  mMeshLayer->triangularMesh()->update( mMeshLayer->nativeMesh(), &rc );


}


void TestQgsMeshTrace::vectorInterpolatorTest()
{



  QgsMeshDatasetIndex dataIndex( 0, 0 );
  int count = mMeshLayer->nativeMesh()->vertexCount();



  QgsMeshDataBlock dataBlock = mDataProvider->datasetValues( dataIndex, 0, count );
  QgsMeshVectorValueInterpolatorFromNode interpolator( mMeshLayer->triangularMesh(), dataBlock );

  int size = 1000;
  QgsRectangle extent = mMeshLayer->extent();
  double dx = extent.width() / 1000;
  double dy = extent.height() / 1000;
  QgsVector incPointX( -dx, 0 );
  QgsVector incPointY( 0, dy );
  QgsPoint startPoint( extent.xMaximum() - 30, extent.yMinimum() );
  QgsPointXY point( startPoint );

  QgsVector vect;
  QTime time;
  time.start();
  int goodPixelCount = 0;
  for ( int i = 0; i < size; i++ )
  {
    for ( int j = 0; j < size; ++j )
    {
      vect = interpolator.value( point );
      point = point + incPointX;
      if ( vect != QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) )
        goodPixelCount++;
    }
    point = point + incPointY;
    point.setX( startPoint.x() );
  }


  QCOMPARE( goodPixelCount, 516641 );


}

void TestQgsMeshTrace::cleanupTestCase()
{
  delete mMeshLayer;
  QgsApplication::exitQgis();
}


QGSTEST_MAIN( TestQgsMeshTrace )
#include "testqgsmeshtrace.moc"
