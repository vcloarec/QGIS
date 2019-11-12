/***************************************************************************
                         qgsmeshtracerenderer.cpp
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

#include "qgsmeshtracerenderer.h"



QgsVector QgsMeshVectorValueInterpolatorFromVertex::value( const QgsPointXY &point )
{
  updateFace( point );

  if ( mCacheFaceIndex == -1 || mCacheFaceIndex >= mTriangularMesh.triangles().count() )
    return ( QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) );

  if ( ! mActiveFaceFlagValue.active( mTriangularMesh.trianglesToNativeFaces()[mCacheFaceIndex] ) )
    return ( QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) );

  face = mTriangularMesh.triangles().at( mCacheFaceIndex );


  QgsPoint p1 = mTriangularMesh.vertices().at( face[0] );
  QgsPoint p2 = mTriangularMesh.vertices().at( face[1] );
  QgsPoint p3 = mTriangularMesh.vertices().at( face[2] );

  QgsVector v1 = QgsVector( mDatasetValues.value( face[0] ).x(),
                            mDatasetValues.value( face[0] ).y() );

  QgsVector v2 = QgsVector( mDatasetValues.value( face[1] ).x(),
                            mDatasetValues.value( face[1] ).y() );

  QgsVector v3 = QgsVector( mDatasetValues.value( face[2] ).x(),
                            mDatasetValues.value( face[2] ).y() );


  return QgsMeshLayerUtils::interpolateVectorFromVerticesData(
           p1,
           p2,
           p3,
           v1,
           v2,
           v3,
           point );
}

QgsMeshVectorValueInterpolator::QgsMeshVectorValueInterpolator( QgsTriangularMesh triangularMesh,
    QgsMeshDataBlock datasetVectorValues,
    QgsMeshDataBlock scalarActiveFaceFlagValues ):
  mTriangularMesh( triangularMesh ),
  mDatasetValues( datasetVectorValues ),
  mActiveFaceFlagValue( scalarActiveFaceFlagValues )
{

}

QgsMeshVectorValueInterpolator::~QgsMeshVectorValueInterpolator() {}

void QgsMeshVectorValueInterpolator::updateFace( const QgsPointXY &point )
{
  if ( ! QgsMeshUtils::isInTriangleFace( point, face, mTriangularMesh.vertices() ) )
  {
    /*the first version of this method uses QgsGeometry::contain to check if the point is in triangles,
    *  not too time consuming for a simple triangle?
    * why not using simpler function for this check ?  version 2 is  nearly 10 time faster.
    */
    mCacheFaceIndex = mTriangularMesh.faceIndexForPoint_v2( point );
  }

}

QSize QgsMeshTraceField::size() const
{
  return mFieldSize;
}

QPoint QgsMeshTraceField::topLeft() const
{
  return mFieldTopLeftInDeviceCoordinates;
}

int QgsMeshTraceField::particleWidth() const
{
  return mFieldResolution;
}

QgsPointXY QgsMeshTraceField::positionToMapCoordinnate( const QPoint &pixelPosition, const QgsPointXY &positionInPixel )
{
  QgsPointXY mapPoint = mMapToFieldPixel.toMapCoordinates( pixelPosition );
  mapPoint = mapPoint + QgsVector( positionInPixel.x() * mMapToFieldPixel.mapUnitsPerPixel(),
                                   positionInPixel.y() * mMapToFieldPixel.mapUnitsPerPixel() );
  return mapPoint;
}

QgsMeshTraceField::QgsMeshTraceField( const QgsRenderContext &renderContext, QgsMeshVectorValueInterpolator *interpolator, const QgsRectangle &layerExtent, double Vmax ):
  mLayerExtent( layerExtent ),
  mRenderContext( renderContext ),
  mVectorValueInterpolator( interpolator ),
  mVmax( Vmax )
{
}

void QgsMeshTraceField::updateSize()
{
  const QgsMapToPixel &deviceMapToPixel = mRenderContext.mapToPixel();

  QgsRectangle layerExtent = mRenderContext.coordinateTransform().transform( mLayerExtent );
  QgsRectangle mapExtent = mRenderContext.extent();

  QgsRectangle interestZoneExtent = layerExtent.intersect( mapExtent );
  if ( interestZoneExtent == QgsRectangle() )
  {
    mValid = false;
    mFieldSize = QSize();
    mFieldTopLeftInDeviceCoordinates = QPoint();
    initField();
    return;
  }

  QgsPointXY interestZoneTopLeft = deviceMapToPixel.transform( QgsPointXY( interestZoneExtent.xMinimum(), interestZoneExtent.yMaximum() ) );
  QgsPointXY interestZoneBottomRight = deviceMapToPixel.transform( QgsPointXY( interestZoneExtent.xMaximum(), interestZoneExtent.yMinimum() ) );


  mFieldTopLeftInDeviceCoordinates = interestZoneTopLeft.toQPointF().toPoint();
  QPoint mFieldBottomRightInDeviceCoordinates = interestZoneBottomRight.toQPointF().toPoint();
  int fieldWidthInDeviceCoordinate = mFieldBottomRightInDeviceCoordinates.x() - mFieldTopLeftInDeviceCoordinates.x();
  int fieldHeightInDeviceCoordinate = mFieldBottomRightInDeviceCoordinates.y() - mFieldTopLeftInDeviceCoordinates.y();


  int fieldWidth = int( fieldWidthInDeviceCoordinate / mFieldResolution );
  int fieldHeight = int( fieldHeightInDeviceCoordinate / mFieldResolution );


  //increase the field size if this size is not adjusted to extent of zone of interest in device coordinates
  if ( fieldWidthInDeviceCoordinate % mFieldResolution > 0 )
    fieldWidth++;
  if ( fieldHeightInDeviceCoordinate % mFieldResolution > 0 )
    fieldHeight++;

  mFieldSize.setWidth( fieldWidth );
  mFieldSize.setHeight( fieldHeight );


  double mapUnitperFieldPixel;
  if ( interestZoneExtent.width() > 0 )
    mapUnitperFieldPixel = interestZoneExtent.width() / fieldWidthInDeviceCoordinate * mFieldResolution;
  else
    mapUnitperFieldPixel = 1e-8;

  int fieldRightDevice = mFieldTopLeftInDeviceCoordinates.x() + mFieldSize.width() * mFieldResolution;
  int fieldBottomDevice = mFieldTopLeftInDeviceCoordinates.y() + mFieldSize.height() * mFieldResolution;
  QgsPointXY fieldRightBottomMap = deviceMapToPixel.toMapCoordinates( fieldRightDevice, fieldBottomDevice );

  int fieldTopDevice = mFieldTopLeftInDeviceCoordinates.x();
  int fieldLeftDevice = mFieldTopLeftInDeviceCoordinates.y();
  QgsPointXY fieldTopLeftMap = deviceMapToPixel.toMapCoordinates( fieldTopDevice, fieldLeftDevice );

  double xc = ( fieldRightBottomMap.x() + fieldTopLeftMap.x() ) / 2;
  double yc = ( fieldTopLeftMap.y() + fieldRightBottomMap.y() ) / 2;

  mMapToFieldPixel = QgsMapToPixel( mapUnitperFieldPixel,
                                    xc,
                                    yc,
                                    fieldWidth,
                                    fieldHeight, 0 );

  initField();
  mValid = true;
}

void QgsMeshTraceField::addTrace( QgsPointXY startPoint )
{
  addTrace( mMapToFieldPixel.transform( startPoint ).toQPointF().toPoint() );
}

void QgsMeshTraceField::addRandomTraces( int count )
{
  for ( int i = 0; i < count; ++i )
    addRandomTrace();
}

void QgsMeshTraceField::addRandomTrace()
{
  if ( !mValid )
    return;

  int xRandom =  1 + std::rand() / int( ( RAND_MAX + 1u ) / uint( mFieldSize.width() ) )  ;
  int yRandom = 1 + std::rand() / int ( ( RAND_MAX + 1u ) / uint( mFieldSize.height() ) ) ;

  addTrace( QPoint( xRandom, yRandom ) );
}

void QgsMeshTraceField::addGriddedTraces( int pixelSpace )
{
  int fieldSpace = pixelSpace / mFieldResolution;
  int i = 0;
  while ( i < size().width() )
  {
    int j = 0;
    while ( j < size().height() )
    {
      addTrace( QPoint( i, j ) );
      j += fieldSpace;
    }
    i += fieldSpace;
  }
}

void QgsMeshTraceField::addTracesFromBorder( int pixelSpace )
{
  int fieldSpace = pixelSpace / mFieldResolution;
  int i = 0;
  int j = 0;
  while ( j < size().height() )
  {
    addTrace( QPoint( i, j ) );
    j += fieldSpace;
  }
  i = size().width() - 1;
  j = 0;
  while ( j < size().height() )
  {
    addTrace( QPoint( i, j ) );
    j += fieldSpace;
  }
  i = 0;
  j = 0;
  while ( i < size().width() )
  {
    addTrace( QPoint( i, j ) );
    i += fieldSpace;
  }
  i = 0;
  j = size().height() - 1;
  while ( i < size().width() )
  {
    addTrace( QPoint( i, j ) );
    i += fieldSpace;
  }

}

void QgsMeshTraceField::addTrace( QPoint startPixel )
{
  //This is where each trace are contructed

  if ( !mValid )
    return;

  if ( isWayExist( startPixel ) )
    return;

  if ( ! mVectorValueInterpolator )
    return;

  bool end = false;
  //position int the pixelField
  double x1 = 0;
  double y1 = 0;

  QPoint currentPixel = startPixel;
  QgsVector vector;
  startTrace( currentPixel );

  float dt = 0;

  int i = 0;
  while ( !end )
  {
    QgsPointXY mapPosition = positionToMapCoordinnate( currentPixel, QgsPointXY( x1, y1 ) );
    vector = mVectorValueInterpolator->value( mapPosition ) ;

    if ( std::isnan( vector.x() ) || std::isnan( vector.y() ) )
    {
      endTrace( currentPixel );
      break;
    }

    /* adimentional value :  Vu=2 when the particule need dt=1 to go throught a pixel
     * The size of the side of a pixel is 2
     */
    QgsVector vu = vector / mVmax * 2;
    double Vx = vu.x();
    double Vy = vu.y();
    double Vu = vu.length(); //adimentional vector magnitude

    if ( qgsDoubleNear( Vu, 0 ) )
    {
      // no trace anymore
      endTrace( currentPixel );
      break;
    }

    /*calculates where the particule will be after dt=1,
     * that permits to know i, which pixel the time spent have to be calculated
     */

    QgsPointXY  nextPosition = QgsPointXY( x1, y1 ) + vu;
    int incX = 0;
    int incY = 0;
    if ( nextPosition.x() > 1 )
      incX = +1;
    if ( nextPosition.x() < -1 )
      incX = -1;
    if ( nextPosition.y() > 1 )
      incY = +1;
    if ( nextPosition.y() < -1 )
      incY = -1;

    double x2, y2;


    if ( incX != 0 || incY != 0 )
    {
      //the particule leave the current pixel --> calculate where the particule is entered and change the current pixel
      if ( incX * incY != 0 )
      {
        x2 = x1 + ( incY - y1 ) * Vx / Vy;
        y2 = y1 + ( incX - x1 ) * Vy / Vx;

        if ( fabs( x2 ) > 1 )
          y2 = incY;
        if ( fabs( y2 ) > 1 )
          x2 = incX;
      }
      else if ( incX != 0 )
      {
        x2 = incX;
        y2 = y1 + ( incX - x1 ) * Vy / Vx;
      }
      else //incY != 0
      {
        x2 = x1 + ( incY - y1 ) * Vx / Vy;
        y2 = incY;
      }
      //move the current position, adjust position in pixel and store the direction and dt in the pixel
      setWay( currentPixel, dt, float( Vu / 2 ), incX, incY );
      dt = 1;
      currentPixel += QPoint( incX, -incY );
      ++i;
      x1 = x2 - 2 * incX;
      y1 = y2 - 2 * incY;
    }
    else
    {
      /*the particule still in the pixel --> push with the vector value to join a border
       * and calculate the time spent to go through the pixel
       */
      if ( qgsDoubleNear( Vy, 0 ) )
      {
        y2 = y1;
        if ( Vx > 0 )
          incX = +1;
        else
          incX = -1;

        x2 = incX ;

      }
      else if ( qgsDoubleNear( Vx, 0 ) )
      {
        x2 = x1;
        if ( Vy > 0 )
          incY = +1;
        else
          incY = -1;

        y2 = incY ;
      }
      else
      {
        if ( Vy > 0 )
          x2 = x1 + ( 1 -  y1 ) * Vx / fabs( Vy ) ;
        else
          x2 = x1 + ( 1 + y1 ) * Vx / fabs( Vy ) ;
        if ( Vx > 0 )
          y2 = y1 + ( 1 - x1 ) * Vy / fabs( Vx ) ;
        else
          y2 = y1 + ( 1 + x1 ) * Vy / fabs( Vx ) ;

        if ( x2 >= 1 )
        {
          x2 = 1;
          incX = +1;
        }
        if ( x2 <= -1 )
        {
          x2 = -1;
          incX = -1;
        }
        if ( y2 >= 1 )
        {
          y2 = 1;
          incY = +1;
        }
        if ( y2 <= -1 )
        {
          y2 = -1;
          incY = -1;
        }
      }

      //calculate distance
      double dx = x2 - x1;
      double dy = y2 - y1;
      double dl = sqrt( dx * dx + dy * dy );

      dt += dl / Vu ; //adimentional time step : this the time needed to go throught the pixel

      if ( dt > 10000 ) //Guard to prevent that the particle never leave the pixel
      {
        endTrace( currentPixel );
        break;
      }
      x1 = x2;
      y1 = y2;
    }

    //test if the new current pixel is already defined, if yes no need to continue
    if ( isWayExist( currentPixel ) )
    {
      break;
    }

  }
}

void QgsMeshTraceField::setParticleWidth( int width )
{
  mFieldResolution = width;
}

void QgsMeshTraceFieldDynamic::initField()
{
  mDirectionField = QVector<char>( size().width() * size().height(), char( 0 ) );
  mTimeField = QVector<float>( size().width() * size().height(), 0 );
}

void QgsMeshTraceFieldDynamic::setWay( const QPoint &pixel, float dt, float mag, int incX, int incY )
{
  char pixelValue = char( incX + 2 + ( incY + 1 ) * 3 );
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < size().width() && j >= 0 && j < size().height() )
  {
    mDirectionField[j * size().width() + i] = pixelValue;
  }
}

bool QgsMeshTraceFieldDynamic::way( const QPoint &pixel, float &dt, int &incX, int &incY ) const
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < size().width() && j >= 0 && j < size().height() )
  {
    int index = j * size().width() + i;
    int direction = int( mDirectionField[index] );
    if ( direction != 0 )
    {
      incX = ( direction - 1 ) % 3 - 1;
      incY = ( direction - 1 ) / 3 - 1;
      dt = mTimeField[index];
      return true;
    }
  }
  return false;

}

bool QgsMeshTraceFieldDynamic::isWayExist( const QPoint &pixel )
{
  int i = pixel.x();
  int j = pixel.y();
  uint direction;
  if ( i >= 0 && i < size().width() && j >= 0 && j < size().height() )
  {
    direction = uint( mDirectionField[j * size().width() + i] );
    if ( direction == 0 )
      return false;
  }

  return true;

}

QgsMeshTraceColor::~QgsMeshTraceColor() {}

QgsMeshTraceUniqueColor::QgsMeshTraceUniqueColor( const QColor &color ):
  mColor( color )
{
}

QColor QgsMeshTraceUniqueColor::color( float value ) const
{
  Q_UNUSED( value );
  return mColor;
}

QgsMeshTraceColorRamp::QgsMeshTraceColorRamp( QgsColorRamp *colorRamp ):
  mColorRamp( colorRamp )
{

}

QgsMeshTraceColorRamp::~QgsMeshTraceColorRamp()
{
  delete mColorRamp;
}

QgsMeshTraceFieldStatic::QgsMeshTraceFieldStatic( const QgsRenderContext &renderContext, QgsMeshVectorValueInterpolator *interpolator, const QgsRectangle &layerExtent, double Vmax, QgsMeshTraceColor &traceColor ):
  QgsMeshTraceField( renderContext, interpolator, layerExtent, Vmax ),
  mTraceColor( traceColor )
{
  mPen.setWidthF( 0.5 );
}

QgsMeshTraceFieldStatic::~QgsMeshTraceFieldStatic()
{
  if ( mPainter )
    delete mPainter;
}

void QgsMeshTraceFieldStatic::exportImage() const
{
  mTraceImage.save( "traceField", "PNG" );
}

QSize QgsMeshTraceFieldStatic::imageSize() const
{
  return size() * particleWidth();
}

QImage QgsMeshTraceFieldStatic::image() const
{
  return mTraceImage.scaled( imageSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
}

void QgsMeshTraceFieldStatic::initField()
{
  mMagnitudeField = QVector<float>( size().width() * size().height(), -1 );
  mTraceImage = QImage( imageSize(), QImage::Format_ARGB32 );
  mTraceImage.fill( 0X00000000 );
  if ( mPainter )
    delete mPainter;
  mPainter = new QPainter( &mTraceImage );
  mPainter->setRenderHint( QPainter::Antialiasing, true );
  mPainter->setPen( mPen );
}

void QgsMeshTraceFieldStatic::setWay( const QPoint &pixel, float dt, float mag, int incX, int incY )
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < size().width() && j >= 0 && j < size().height() )
  {
    mMagnitudeField[j * size().width() + i] = mag;
    mPen.setColor( mTraceColor.color( mag ) );
    mPainter->setPen( mPen );

    QPointF devicePixel = fieldToDevice( pixel );
    if ( mTraceInProgress )
      mPainter->drawLine( mLastPixel, devicePixel );
    else
      mPainter->drawPoint( devicePixel );
    mLastPixel = devicePixel;
  }
}

bool QgsMeshTraceFieldStatic::isWayExist( const QPoint &pixel )
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < size().width() && j >= 0 && j < size().height() )
  {
    float mag = mMagnitudeField[j * size().width() + i];
    if ( mag < 0 )
      return false;
  }

  return true;

}

void QgsMeshTraceFieldStatic::startTrace( const QPoint &startPixel )
{
  mLastPixel = fieldToDevice( startPixel );
  mTraceInProgress = true;
}

void QgsMeshTraceFieldStatic::endTrace( const QPoint &endPixel )
{
  mTraceInProgress = false;
}
