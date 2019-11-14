/***************************************************************************
                         testqgsmeshtrace.cpp
                         -------------------------
    begin                : November 2019
    copyright            : (C) 2019 by Vincent Cloarec
    email                : vcloarec at gmail dot com
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
 * This is a unit test for mesh traces
 */
class TestQgsMeshTrace : public QObject
{
    Q_OBJECT

  public:
    TestQgsMeshTrace() = default;

  private:
    QgsMeshLayer *mMeshLayer;
    QgsMeshDataProvider *mDataProvider;
    QgsMeshDataBlock mDataBlock;
    QgsMeshDataBlock mScalarActiveFaceFlagValues;
    QgsMeshTraceField *mTraceField;
    double mVmax;

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void vectorInterpolatorTest();
    void traceFieldTest();

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

  QgsMeshDatasetIndex dataIndex( 0, 0 );
  int count = mMeshLayer->nativeMesh()->vertexCount();
  mDataBlock = mDataProvider->datasetValues( dataIndex, 0, count );
  mScalarActiveFaceFlagValues = mMeshLayer->dataProvider()->areFacesActive(
                                  dataIndex,
                                  0,
                                  mMeshLayer->nativeMesh()->faces.count() );

  mVmax = mDataProvider->datasetMetadata( dataIndex ).maximum();

}


void TestQgsMeshTrace::vectorInterpolatorTest()
{
  int size = 1000;
  QgsRectangle extent = mMeshLayer->extent();
  double dx = extent.width() / 1000;
  double dy = extent.height() / 1000;
  QgsVector incPointX( -dx, 0 );
  QgsVector incPointY( 0, dy );
  QgsPoint startPoint( extent.xMaximum(), extent.yMinimum() );
  QgsPointXY point( startPoint );

  std::unique_ptr<QgsMeshVectorValueInterpolatorFromVertex> interpolator =
    std::unique_ptr<QgsMeshVectorValueInterpolatorFromVertex>( new QgsMeshVectorValueInterpolatorFromVertex( *mMeshLayer->triangularMesh(), mDataBlock, mScalarActiveFaceFlagValues ) );

  QgsVector vect;
  QTime time;
  time.start();
  int goodPixelCount = 0;
  for ( int i = 0; i < size; i++ )
  {
    for ( int j = 0; j < size; ++j )
    {
      vect = interpolator->value( point );
      point = point + incPointX;
      if ( vect != QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) )
        goodPixelCount++;
    }
    point = point + incPointY;
    point.setX( startPoint.x() );
  }

  QCOMPARE( goodPixelCount, 516641 );
}

void TestQgsMeshTrace::traceFieldTest()
{
  QgsRenderContext rc;
  QgsRectangle mapExtent = mMeshLayer->extent();
  QSize outputSize( 1000, 1000 );
  double mapUnitPerOutputPixel = mapExtent.width() / outputSize.width();
  QgsMapToPixel maptToPixel( mapUnitPerOutputPixel,
                             mapExtent.xMinimum() + mapExtent.width() / 2,
                             mapExtent.yMinimum() + mapExtent.height() / 2,
                             outputSize.width(), outputSize.width(), 0 );
  rc.setMapToPixel( maptToPixel );

  std::unique_ptr<QgsMeshVectorValueInterpolatorFromVertex> interpolator =
    std::unique_ptr<QgsMeshVectorValueInterpolatorFromVertex>( new QgsMeshVectorValueInterpolatorFromVertex( *mMeshLayer->triangularMesh(), mDataBlock, mScalarActiveFaceFlagValues ) );
  QgsMeshTraceFieldStatic field( rc, mMeshLayer->extent(), mVmax, Qt::blue );

  QTime time;
  time.start();
  std::srand( std::time( nullptr ) );
  for ( int i = 0; i < 10000; ++i )
    field.addRandomTrace();
  qDebug() << "Time elapsed " << time.elapsed();
  field.exportImage();
}

void TestQgsMeshTrace::cleanupTestCase()
{
  delete mMeshLayer;
  QgsApplication::exitQgis();
}


QGSTEST_MAIN( TestQgsMeshTrace )
#include "testqgsmeshtrace.moc"
