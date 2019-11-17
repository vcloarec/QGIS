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



QgsVector QgsMeshVectorValueInterpolator::value( const QgsPointXY &point ) const
{
  QMutexLocker locker( &mMutex );

  if ( mCacheFaceIndex != -1 && mCacheFaceIndex < mTriangularMesh.triangles().count() )
  {
    QgsMeshFace face = mTriangularMesh.triangles().at( mCacheFaceIndex );
    QgsVector res = interpolatedValuePrivate( face, point );
    if ( isVectorValid( res ) )
    {
      activeFaceFilter( res, mCacheFaceIndex );
      return res;
    }
  }

  //point is not on the face associated with mCacheIndex --> search for the face containing the point
  QList<int> potentialFaceIndexes = mTriangularMesh.faceIndexesForRectangle( QgsRectangle( point, point ) );
  mCacheFaceIndex = -1;
  for ( const int faceIndex : potentialFaceIndexes )
  {
    QgsVector res = interpolatedValuePrivate( mTriangularMesh.triangles().at( faceIndex ), point );
    if ( isVectorValid( res ) )
    {
      mCacheFaceIndex = faceIndex;
      activeFaceFilter( res, mCacheFaceIndex );
      return res;
    }
  }

  //--> no face found return non valid vector
  return ( QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) );

}

QgsVector QgsMeshVectorValueInterpolatorFromVertex::interpolatedValuePrivate( const QgsMeshFace &face, const QgsPointXY point ) const
{
  QgsPoint p1 = mTriangularMesh.vertices().at( face.at( 0 ) );
  QgsPoint p2 = mTriangularMesh.vertices().at( face.at( 1 ) );
  QgsPoint p3 = mTriangularMesh.vertices().at( face.at( 2 ) );

  QgsVector v1 = QgsVector( mDatasetValues.value( face.at( 0 ) ).x(),
                            mDatasetValues.value( face.at( 0 ) ).y() );

  QgsVector v2 = QgsVector( mDatasetValues.value( face.at( 1 ) ).x(),
                            mDatasetValues.value( face.at( 1 ) ).y() );

  QgsVector v3 = QgsVector( mDatasetValues.value( face.at( 2 ) ).x(),
                            mDatasetValues.value( face.at( 2 ) ).y() );


  return QgsMeshLayerUtils::interpolateVectorFromVerticesData(
           p1,
           p2,
           p3,
           v1,
           v2,
           v3,
           point );

}

QgsMeshVectorValueInterpolator::QgsMeshVectorValueInterpolator( const QgsTriangularMesh &triangularMesh,
    const QgsMeshDataBlock &datasetVectorValues ):
  mTriangularMesh( triangularMesh ),
  mDatasetValues( datasetVectorValues ),
  mUseScalarActiveFaceFlagValues( false )
{

}

QgsMeshVectorValueInterpolator::QgsMeshVectorValueInterpolator( const QgsTriangularMesh &triangularMesh,
    const QgsMeshDataBlock &datasetVectorValues,
    const QgsMeshDataBlock &scalarActiveFaceFlagValues ):
  mTriangularMesh( triangularMesh ),
  mDatasetValues( datasetVectorValues ),
  mActiveFaceFlagValues( scalarActiveFaceFlagValues ),
  mUseScalarActiveFaceFlagValues( true )
{

}


void QgsMeshVectorValueInterpolator::updateCacheFaceIndex( const QgsPointXY &point ) const
{
  if ( ! QgsMeshUtils::isInTriangleFace( point, mFaceCache, mTriangularMesh.vertices() ) )
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
  QMutexLocker locker( &mMutex );
  return mFieldSize;
}

QPoint QgsMeshTraceField::topLeft() const
{
  QMutexLocker locker( &mMutex );
  return mFieldTopLeftInDeviceCoordinates;
}

int QgsMeshTraceField::resolution() const
{
  QMutexLocker locker( &mMutex );
  return mFieldResolution;
}

QgsPointXY QgsMeshTraceField::positionToMapCoordinates( const QPoint &pixelPosition, const QgsPointXY &positionInPixel )
{
  QgsPointXY mapPoint = mMapToFieldPixel.toMapCoordinates( pixelPosition );
  mapPoint = mapPoint + QgsVector( positionInPixel.x() * mMapToFieldPixel.mapUnitsPerPixel(),
                                   positionInPixel.y() * mMapToFieldPixel.mapUnitsPerPixel() );
  return mapPoint;
}



void QgsMeshTraceField::updateSize( const QgsRenderContext &renderContext )
{
  stopProcessing();
  QMutexLocker locker( &mMutex );
  mMapExtent = renderContext.mapExtent();

  const QgsMapToPixel &deviceMapToPixel = renderContext.mapToPixel();

  QgsRectangle layerExtent;
  try
  {
    layerExtent = renderContext.coordinateTransform().transform( mLayerExtent );

  }
  catch ( QgsCsException &cse )
  {
    Q_UNUSED( cse );
    layerExtent = mLayerExtent;
  }

  QgsRectangle interestZoneExtent = layerExtent.intersect( mMapExtent );
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


  double mapUnitPerFieldPixel;
  if ( interestZoneExtent.width() > 0 )
    mapUnitPerFieldPixel = interestZoneExtent.width() / fieldWidthInDeviceCoordinate * mFieldResolution;
  else
    mapUnitPerFieldPixel = 1e-8;

  int fieldRightDevice = mFieldTopLeftInDeviceCoordinates.x() + mFieldSize.width() * mFieldResolution;
  int fieldBottomDevice = mFieldTopLeftInDeviceCoordinates.y() + mFieldSize.height() * mFieldResolution;
  QgsPointXY fieldRightBottomMap = deviceMapToPixel.toMapCoordinates( fieldRightDevice, fieldBottomDevice );

  int fieldTopDevice = mFieldTopLeftInDeviceCoordinates.x();
  int fieldLeftDevice = mFieldTopLeftInDeviceCoordinates.y();
  QgsPointXY fieldTopLeftMap = deviceMapToPixel.toMapCoordinates( fieldTopDevice, fieldLeftDevice );

  double xc = ( fieldRightBottomMap.x() + fieldTopLeftMap.x() ) / 2;
  double yc = ( fieldTopLeftMap.y() + fieldRightBottomMap.y() ) / 2;

  mMapToFieldPixel = QgsMapToPixel( mapUnitPerFieldPixel,
                                    xc,
                                    yc,
                                    fieldWidth,
                                    fieldHeight, 0 );

  initField();
  mValid = true;
}

void QgsMeshTraceField::updateSize( const QgsRenderContext &renderContext, int resolution )
{
  if ( renderContext.mapExtent() == mMapExtent && resolution == mFieldResolution )
    return;
  mFieldResolution = resolution;

  updateSize( renderContext );
}

bool QgsMeshTraceField::isValid() const
{
  return mValid;
}

void QgsMeshTraceField::addTrace( QgsPointXY startPoint )
{
  QPoint sp;
  mMutex.lock();
  sp = mMapToFieldPixel.transform( startPoint ).toQPointF().toPoint();
  mMutex.unlock();
  addTrace( mMapToFieldPixel.transform( startPoint ).toQPointF().toPoint() );
}

void QgsMeshTraceField::addRandomTraces( int count )
{
  for ( int i = 0; i < count; ++i )
  {
    addRandomTrace();
  }

}

void QgsMeshTraceField::addRandomTraces()
{
  while ( mPixelFillingCount < mMaxPixelFillingCount )
    addRandomTrace();
}

void QgsMeshTraceField::addRandomTrace()
{
  if ( !mValid )
    return;

  mMutex.lock();
  int xRandom =  1 + std::rand() / int( ( RAND_MAX + 1u ) / uint( mFieldSize.width() ) )  ;
  int yRandom = 1 + std::rand() / int ( ( RAND_MAX + 1u ) / uint( mFieldSize.height() ) ) ;
  mMutex.unlock();

  addTrace( QPoint( xRandom, yRandom ) );
}

void QgsMeshTraceField::addGriddedTraces()
{
  mMutex.lock();
  int fieldSpacing;
  if ( mPixelFillingDensity <= 0 )
  {
    fieldSpacing = mFieldSize.width() < mFieldSize.height() ?
                   mFieldSize.width() : mFieldSize.height();
  }
  else
  {
    fieldSpacing = int( 1 / ( mPixelFillingDensity * mPixelFillingDensity ) );
  }
  mMutex.unlock();

  int i = 0;
  while ( i < mFieldSize.width() )
  {
    int j = 0;
    while ( j < mFieldSize.height() )
    {
      addTrace( QPoint( i, j ) );
      j += fieldSpacing;
    }
    i += fieldSpacing;
  }
}

void QgsMeshTraceField::addTracesFromBorder()
{
  mMutex.lock();
  int fieldSpacing;
  if ( mPixelFillingDensity <= 0 )
  {
    fieldSpacing = mFieldSize.width() < mFieldSize.height() ?
                   mFieldSize.width() : mFieldSize.height();
  }
  else
  {
    fieldSpacing = int( 1 / mPixelFillingDensity );
  }
  mMutex.unlock();
  int i = 0;
  int j = 0;


  while ( j < size().height() )
  {
    addTrace( QPoint( i, j ) );
    j += fieldSpacing;
  }
  i = size().width() - 1;
  j = 0;
  while ( j < size().height() )
  {
    addTrace( QPoint( i, j ) );
    j += fieldSpacing;
  }
  i = 0;
  j = 0;
  while ( i < size().width() )
  {
    addTrace( QPoint( i, j ) );
    i += fieldSpacing;
  }
  i = 0;
  j = size().height() - 1;
  while ( i < size().width() )
  {
    addTrace( QPoint( i, j ) );
    i += fieldSpacing;
  }

}

void QgsMeshTraceField::addTrace( QPoint startPixel )
{
  //This is where each trace are constructed
  QMutexLocker locker( &mMutex );
  if ( !mValid )
    return;

  if ( isWayExistPrivate( startPixel ) )
    return;

  if ( !mVectorValueInterpolator )
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
    if ( mProcessingStop )
      break;
    QgsPointXY mapPosition = positionToMapCoordinates( currentPixel, QgsPointXY( x1, y1 ) );
    vector = mVectorValueInterpolator->value( mapPosition ) ;

    if ( std::isnan( vector.x() ) || std::isnan( vector.y() ) )
    {
      mPixelFillingCount++;
      endTrace( currentPixel );
      break;
    }

    /* adimentional value :  Vu=2 when the particule need dt=1 to go throught a pixel
     * The size of the side of a pixel is 2
     */
    QgsVector vu = vector / mMagMax * 2;
    double Vx = vu.x();
    double Vy = vu.y();
    double Vu = vu.length(); //nondimentional vector magnitude

    if ( qgsDoubleNear( Vu, 0 ) )
    {
      // no trace anymore
      mPixelFillingCount++;
      endTrace( currentPixel );
      break;
    }

    /*calculates where the particule will be after dt=1,
     * that permits to know which pixel the time spent have to be calculated
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
      setWayPrivate( currentPixel, dt, float( Vu / 2 ), incX, incY );
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
        mPixelFillingCount++;
        break;
      }
      x1 = x2;
      y1 = y2;
    }

    //test if the new current pixel is already defined, if yes no need to continue
    if ( isWayExistPrivate( currentPixel ) )
    {
      break;
    }

  }
}

void QgsMeshTraceField::setResolution( int width )
{
  QMutexLocker locker( &mMutex );
  mFieldResolution = width;
}

void QgsMeshTraceFieldDynamic::initField()
{
  mDirectionField = QVector<char>( mFieldSize.width() * mFieldSize.height(), char( 0 ) );
  mTimeField = QVector<float>( mFieldSize.width() * mFieldSize.height(), 0 );
  mParticleField = QVector<bool>( mFieldSize.width() * mFieldSize.height(), 0 );

  if ( mPainter )
    delete mPainter;

  mTraceImage = QImage( mFieldSize * mFieldResolution, QImage::Format_ARGB32 );
  mTraceImage.fill( QColor( 0, 0, 0, 0 ) );

  mPainter = new QPainter( &mTraceImage );
  mPainter->setRenderHint( QPainter::Antialiasing, true );
  mPainter->setCompositionMode( QPainter::CompositionMode_SourceOut );

  mShaderImg = QImage( mFieldSize * mFieldResolution, QImage::Format_ARGB32 );
  mShaderImg.fill( QColor( 0, 0, 0, 1 ) );

  mParticles.clear();
  mParticlesCount = 0;

  mProcessingStop = false;
}

void QgsMeshTraceFieldDynamic::setWayPrivate( const QPoint &pixel, float dt, float mag, int incX, int incY )
{
  char pixelValue = char( incX + 2 + ( incY + 1 ) * 3 );
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < mFieldSize.width() && j >= 0 && j < mFieldSize.height() )
  {
    int index = j * mFieldSize.width() + i;
    mDirectionField[index] = pixelValue;
    mTimeField[index] = dt;
  }
}

bool QgsMeshTraceFieldDynamic::wayPrivate( const QPoint &pixel, float &dt, int &incX, int &incY ) const
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < mFieldSize.width() && j >= 0 && j < mFieldSize.height() )
  {
    int index = j * mFieldSize.width() + i;
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

void QgsMeshTraceFieldDynamic::moveParticles( float time )
{
  mPainter->drawImage( 0, 0, mShaderImg );

  auto part = mParticles.begin();
  while ( part != mParticles.end() )
  {
    part->move( time, *this );
    part->draw( *this, 5 );

    if ( part->isLost() )
    {
      QMutexLocker locker( &mMutex );
      part = mParticles.erase( part );
      mParticlesCount--;
    }
    else
      part++;
  }


}

bool QgsMeshTraceFieldDynamic::isWayExistPrivate( const QPoint &pixel ) const
{
  int i = pixel.x();
  int j = pixel.y();
  uint direction;
  if ( i >= 0 && i < mFieldSize.width() && j >= 0 && j < mFieldSize.height() )
  {
    direction = uint( mDirectionField[j * mFieldSize.width() + i] );
    if ( direction == 0 )
      return false;
  }
  return true;

}

QgsMeshTraceColor::~QgsMeshTraceColor() {}

QgsMeshTraceFixedColor::QgsMeshTraceFixedColor( const QColor &color ):
  mColor( color )
{
}

QColor QgsMeshTraceFixedColor::color( float value ) const
{
  Q_UNUSED( value );
  return mColor;
}

QgsMeshTraceColorRamp::QgsMeshTraceColorRamp( const QgsColorRampShader &colorRampShader ):
  mColorRampShader( colorRampShader )
{

}

QgsMeshTraceColorRamp::~QgsMeshTraceColorRamp()
{

}

QgsMeshStreamLineField::QgsMeshStreamLineField( const QgsTriangularMesh &triangularMesh,
    const QgsMeshDataBlock &dataSetVectorValues,
    const QgsMeshDataBlock &scalarActiveFaceFlagValues,
    const QgsRenderContext &rendererContext,
    const QgsRectangle &layerExtent,
    double Vmax ):
  QgsMeshTraceField( triangularMesh, dataSetVectorValues, scalarActiveFaceFlagValues, layerExtent, Vmax )
{
}


QgsMeshStreamLineField::~QgsMeshStreamLineField()
{
  if ( mPainter )
    delete mPainter;
}

void QgsMeshStreamLineField::exportImage() const
{
  mTraceImage.save( "traceField", "PNG" );
}

QSize QgsMeshTraceField::imageSize() const
{
  QMutexLocker locker( &mMutex );
  return mFieldSize * mFieldResolution;
}

QPointF QgsMeshTraceField::fieldToDevice( const QPoint &pixel ) const
{
  QPointF p( pixel );
  p = mFieldResolution * p + QPointF( mFieldResolution - 1, mFieldResolution - 1 ) / 2;
  return p;
}

QImage QgsMeshTraceField::image()
{
  QMutexLocker locker( &mMutex );
  return mTraceImage.scaled( mFieldSize * mFieldResolution, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
}

bool QgsMeshTraceField::isWayExist( const QPoint &pixel ) const
{
  QMutexLocker locker( &mMutex );
  return isWayExistPrivate( pixel );
}

bool QgsMeshTraceField::way( const QPoint &pixel, float &dt, int &incX, int &incY ) const
{
  QMutexLocker locker( &mMutex );
  return wayPrivate( pixel, dt, incX, incY );
}

void QgsMeshStreamLineField::initField()
{
  mMagnitudeField = QVector<float>( mFieldSize.width() * mFieldSize.height(), -1 );

  if ( mPainter )
    delete mPainter;

  mTraceImage = QImage( mFieldSize * mFieldResolution, QImage::Format_ARGB32 );
  mTraceImage.fill( 0X00000000 );

  mPainter = new QPainter( &mTraceImage );
  mPainter->setRenderHint( QPainter::Antialiasing, true );
  mPainter->setPen( mPen );
  mProcessingStop = false;

}

void QgsMeshStreamLineField::setWayPrivate( const QPoint &pixel, float dt, float mag, int incX, int incY )
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < mFieldSize.width() && j >= 0 && j < mFieldSize.height() )
  {
    mMagnitudeField[j * mFieldSize.width() + i] = mag;
    mPen.setColor( mTraceColor->color( mag ) );
    mPainter->setPen( mPen );
    mPixelFillingCount++;

    QPointF devicePixel = fieldToDevice( pixel );
    if ( mTraceInProgress )
      mPainter->drawLine( mLastPixel, devicePixel );
    else
      mPainter->drawPoint( devicePixel );
    mLastPixel = devicePixel;
  }
}

bool QgsMeshStreamLineField::isWayExistPrivate( const QPoint &pixel ) const
{
  int i = pixel.x();
  int j = pixel.y();
  if ( i >= 0 && i < mFieldSize.width() && j >= 0 && j < mFieldSize.height() )
  {
    float mag = mMagnitudeField[j * mFieldSize.width() + i];
    if ( mag < 0 )
      return false;
  }

  return true;

}

void QgsMeshStreamLineField::startTrace( const QPoint &startPixel )
{
  mLastPixel = fieldToDevice( startPixel );
  mTraceInProgress = true;
}

void QgsMeshStreamLineField::endTrace( const QPoint &endPixel )
{
  mTraceInProgress = false;
}


void QgsMeshStreamLineField::setTraceColor( const QgsColorRampShader &shader )
{
  mTraceColor.reset( new QgsMeshTraceColorRamp( shader ) );
}

void QgsMeshStreamLineField::setTraceColor( QColor fixedColor )
{
  mTraceColor.reset( new QgsMeshTraceFixedColor( fixedColor ) );
}

void QgsMeshTraceParticle::move( float totalTime, const QgsMeshTraceFieldDynamic &field )
{
  float timeRemaining = totalTime - mTimeRemaingInTheCurrentPixel;
  QPoint position = mCurrentPositionInTheField;
  mPath.clear();
  while ( timeRemaining > 0 && !mLost )
  {
    int incX, incY;
    float dt;
    mLost = !field.way( position, dt, incX, incY );
    if ( !mLost )
    {
      mPath.append( position );
      position += QPoint( incX, -incY );
      timeRemaining -= dt;
    }
  }
  mCurrentPositionInTheField = position;
  mTimeLife -= totalTime;
  if ( mTimeLife < 0 )
  {
    mLost = true;
  }


  if ( timeRemaining < 0 )
    mTimeRemaingInTheCurrentPixel = timeRemaining;
}

void QgsMeshTraceParticle::draw( QgsMeshTraceFieldDynamic &field, double width ) const
{
  for ( auto &p : mPath )
  {
    QPointF devicePoint = field.fieldToDevice( p );

    auto elips = QRectF( devicePoint.x() - width / 2, devicePoint.y() - width / 2, width / 2, width / 2 );
    field.drawTrace( elips );
  }

}

bool QgsMeshTraceParticle::isLost() const
{
  return mLost;
}
