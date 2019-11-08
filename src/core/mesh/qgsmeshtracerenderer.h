#ifndef QGSMESHTRACERENDERER_H
#define QGSMESHTRACERENDERER_H

#define SIP_NO_FILE

#include <QVector>
#include <QSize>

#include "qgis_core.h"
#include "qgis.h"
//#include "qgsmeshdataprovider.h"
#include "qgstriangularmesh.h"
#include "qgsmeshlayer.h"
#include "qgsmeshlayerutils.h"
//#include "qgspointxy.h"


///@cond PRIVATE


class QgsMeshVectorValueInterpolator
{
  public:
    QgsMeshVectorValueInterpolator( QgsTriangularMesh *triangularMesh, const QgsMeshDataBlock &datasetVectorValues ):
      mTriangularMesh( triangularMesh ),
      mDatasetValues( datasetVectorValues )
    {

    }

    virtual QgsVector value( const QgsPointXY &point ) = 0;

  protected:
    QgsTriangularMesh *mTriangularMesh;
    const QgsMeshDataBlock &mDatasetValues;

};


/**
 * \ingroup core
 *
 * Class used to retrieve the value of the vector for a pixel
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshVectorValueInterpolatorFromNode: public QgsMeshVectorValueInterpolator
{
  public:
    QgsMeshVectorValueInterpolatorFromNode() = default;
    QgsMeshVectorValueInterpolatorFromNode( QgsTriangularMesh *triangularMesh, const QgsMeshDataBlock &datasetVectorValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues )
    {

    }

    QgsVector value( const QgsPointXY &point ) override
    {
      if ( ! QgsMeshUtils::isInTriangleFace( point, mLastFace, mTriangularMesh->vertices() ) )
      {
        //the first version of this method uses QgsGeometry::contain to check if the point is in triangles, not too time consuming for a simple triangle?
        // why not using simpler function for this check ?  version 2 is  nearly 10 time faster.
        int faceIndex = mTriangularMesh->faceIndexForPoint_v2( point );

        if ( faceIndex == -1 || faceIndex >= mTriangularMesh->triangles().count() )
          return ( QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) );
        mLastFace = mTriangularMesh->triangles().at( faceIndex );
      }


      QgsPoint p1 = mTriangularMesh->vertices().at( mLastFace[0] );
      QgsPoint p2 = mTriangularMesh->vertices().at( mLastFace[1] );
      QgsPoint p3 = mTriangularMesh->vertices().at( mLastFace[2] );

      QgsVector v1 = QgsVector( mDatasetValues.value( mLastFace[0] ).x(),
                                mDatasetValues.value( mLastFace[0] ).y() );

      QgsVector v2 = QgsVector( mDatasetValues.value( mLastFace[1] ).x(),
                                mDatasetValues.value( mLastFace[1] ).y() );

      QgsVector v3 = QgsVector( mDatasetValues.value( mLastFace[2] ).x(),
                                mDatasetValues.value( mLastFace[2] ).y() );


      return QgsMeshLayerUtils::interpolateVectorFromVerticesData(
               p1,
               p2,
               p3,
               v1,
               v2,
               v3,
               point );
    }

  private:
    QgsMeshFace mLastFace;

};

/**
 * \ingroup core
 *
 * Class used to store the pixel where trace pass (?) and the value of the vector;
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshTraceField
{

  public:

    QgsMeshTraceField( const QgsRectangle &extent, const QgsRenderContext &renderContext ):
      mExtent( extent ),
      mRenderContext( renderContext )
    {
      updateSize();
    }

    void addTrace( QPoint startPixel )
    {



      if ( ! mVectorValueInterpolator )
        return;

      bool end = false;
      //position int the pixelField
      double x0, x1;
      double y0, y1;

      QgsPointXY positionInPixel( 0, 0 );

      QPoint currentPixel = startPixel;
      QgsPointXY mapPosition = positionToMapCoordinnate( currentPixel, positionInPixel );
      QgsVector vector;

      int dt = 0;

      while ( !end )
      {
        vector = mVectorValueInterpolator->value( mapPosition ) ;
        QgsVector vu = vector / mVmax;
        double Vx = vector.x() / mVmax; //adimentional vector
        double Vy = vector.y() / mVmax;
        double Vu = vector.length() / mVmax; //adimentional vector magnitude

        if ( qgsDoubleNear( Vu, 0 ) )
        {
          // no trace anymore
          break;
        }

        //calculate where the particule will be after  dt=1, that permits to know where the particule will go after dt>1
        QgsPointXY  nextPosition = positionInPixel + vu;
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
            x2 = x1 + ( 1 - incY * y1 ) * Vx / Vy;
            y2 = y1 + ( 1 - incX * x1 ) * Vy / Vx;

            if ( fabs( x2 ) > 1 )
              y2 = incY;
            if ( fabs( y2 ) > 1 )
              x2 = incX;
          }
          else if ( incX != 0 )
          {
            x2 = incX;
            y2 = y1 + ( 1 - incX * x1 ) * Vy / Vx;
          }
          else //incY != 0
          {
            x2 = x1 + ( 1 - incY * y1 ) * Vx / Vy;
            y2 = incY;
          }
          //move the current position,adjust position in pixel and store the direction and dt in the pixel
          currentPixel += QPoint( incX, incY );
          x1 = x2 - 2 * incX;
          x1 = y2 - 2 * incY;

        }
        else
        {
          //the particule still in the pixel --> push with the vector value to join a border and calculate the time spent
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
            x2 = x1 + ( 1 - y1 ) * Vx / Vy;
            y2 = y1 + ( 1 - x1 ) * Vy / Vx;

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

          dt += int( dl / Vu ); //adimentional time step : this the time needed to go throught the pixel

          //move to next pixel with incX and incY
          currentPixel = currentPixel + QPoint( incX, incY );
        }
      }
    }

    void setParticuleWidth( int width )
    {
      mParticuleWidth = width;
    }

  private:
    void updateSize()
    {
      int mapWidth = mRenderContext.mapToPixel().mapWidth();
      int mapHeight = mRenderContext.mapToPixel().mapHeight();

      int fieldWidth = int( mapWidth / mParticuleWidth );
      int fieldHeight = int( mapHeight / mParticuleWidth );
      if ( mapWidth % mParticuleWidth > 0 )
        fieldWidth++;
      if ( mapHeight % mParticuleWidth > 0 )
        fieldHeight++;

      mFieldSize = QSize( fieldWidth, fieldHeight );
      mTraceField = QImage( mFieldSize, QImage::Format_Indexed8 );


    }

    QgsPointXY positionToMapCoordinnate( const QPoint &pixelPosition, const QgsPointXY &positionInPixel )
    {
      return QgsPointXY();//TODO
    }

    void setDt( const QPoint &pixel, int dt, int incX, int incY )
    {
      char byte = '\0';
      //TODO
      mTraceField.setPixel( pixel, uint( byte ) );
    }

    //bounding box;
    QgsRectangle mExtent;
    QgsRectangle mFieldsExtent;
    QgsRenderContext mRenderContext;
    int mParticuleWidth = 1;
    QSize mFieldSize;

    /*the direction and the speed to join the next pixel is defined with a char value
     *
     *     0  1  2
     *     3  4  5
     *     6  7  8
     *
     *     convenient to retrives the indexes of the next pixel :
     *     Xnext= v%3-1
     *     Ynext = v/3-1
     *
     *      the value of the char minus 8 represent the time spent by the particule in the pixel
     *      1 -> near 0 value speed
     *      247 --> max value speed
     *
     *      The byte is store in a QImage with format QImage::Format_Indexed8
     *
     */
    QImage mTraceField;



    QgsMeshVectorValueInterpolator *mVectorValueInterpolator;



    double mVmax;

};


class QgsMeshTraceParticule
{

  private:

    //store the real positon in the pixel
    QPointF mCurrentPositionInPixel;
    QgsMeshTraceParticule *mCurrentTrace;
    QList<char>::iterator mCurrentPixelPosition;
};



class QgsMeshTraceRenderer
{
  public:
    QgsMeshTraceRenderer();
};

#endif // QGSMESHTRACERENDERER_H
