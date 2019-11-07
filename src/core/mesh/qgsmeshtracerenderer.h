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



class QgsMeshTrace;

/**
 * \ingroup core
 *
 * Class used to retrieve the value of the vector for a pixel
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshVectorValueInterpolatorFromNode
{
  public:
    QgsMeshVectorValueInterpolatorFromNode() = default;
    QgsMeshVectorValueInterpolatorFromNode( QgsTriangularMesh *triangularMesh, const QgsMeshDataBlock &datasetVectorValues ):
      mTriangularMesh( triangularMesh ),
      mDatasetValues( datasetVectorValues )
    {

    }
    QgsVector value( const QgsPointXY &point )
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
    QgsTriangularMesh *mTriangularMesh;
    const QgsMeshDataBlock &mDatasetValues;
    QgsMeshFace mLastFace;

};

struct wayPoint
{

};

typedef QPair<QPointF, QgsMeshTrace *> QgsMeshTraceWayPoint ;

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

    QgsMeshTraceField( const QSize size ): mTraceField( QVector<QVector<char>>( size.width(), QVector<char>( size.height(), static_cast<char>( 0 ) ) ) )
    {

    }

    void addTrace( QPoint startPixel )
    {
      bool end = false;
      double x0, x1;
      double y0, y1;
      QPoint currentPixel = startPixel;
      QPointF vector/*( mVectorValues.vectorValue( currentPixel ) )*/;

      double Vx = vector.x();
      double Vy = vector.y();

      //initialise position of the first point int the pixel
      if ( qgsDoubleNear( Vx, 0 ) )
        x0 = x1 = 0;
      else if ( Vx < 0 )
        x0 = x1 = -0.5;
      else
        x0 = x1 = +0.5;

      if ( qgsDoubleNear( Vy, 0 ) )
        y0 = y1 = 0;
      else if ( Vy < 0 )
        y0 = y1 = -0.5;
      else
        y0 = y1 = +0.5;


      while ( !end )
      {
        QPointF vector/*( mVectorValues.vectorValue( currentPixel ) )*/;
        double Vx = vector.x();
        double Vy = vector.y();
        double Vu = sqrt( Vx * Vx + Vy * Vy ) / mVmax; //adimentional vector magnitude


        if ( qgsDoubleNear( Vy, 0 ) && qgsDoubleNear( Vx, 0 ) )
        {
          // no trace anymore
          break;
        }

        double x2, y2;
        int incX = 0;
        int incY = 0;

        if ( qgsDoubleNear( Vy, 0 ) )
        {
          y2 = y1;
          if ( Vx > 0 )
            incX = +1;
          else
            incX = -1;

          x2 = incX * 0.5;

        }
        else if ( qgsDoubleNear( Vx, 0 ) )
        {
          x2 = x1;
          if ( Vy > 0 )
            incY = +1;
          else
            incY = -1;

          y2 = incY * 0.5;
        }
        else
        {
          x2 = x1 + ( 0.5 - y1 ) * Vx / Vy;
          y2 = y1 + ( 0.5 - x1 ) * Vy / Vx;

          if ( x2 >= 0.5 )
          {
            x2 = 0.5;
            incX = +1;
          }
          if ( x2 <= -0.5 )
          {
            x2 = -0.5;
            incX = -1;
          }
          if ( y2 >= 0.5 )
          {
            y2 = 0.5;
            incY = +1;
          }
          if ( y2 <= -0.5 )
          {
            y2 = -0.5;
            incY = -1;
          }
        }

        //calculate distance
        double dx = x2 - x1;
        double dy = y2 - y1;
        double dl = sqrt( dx * dx + dy * dy );

        int dt = int( dl / Vu ); //adimentional time step : this the time needed to go thought the pixel

        //move to next pixel with incX and Incy
        currentPixel = currentPixel + QPoint( incX, incY );
      }
    }

  private:
    //bounding box;
    QgsRectangle mExtent;

    QVector<QVector<char>> mTraceField;



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

/**
 * \ingroup core
 *
 * Class used to define a trace in a vector field
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshTrace
{
  public:



  private:

    // start point of the trace
    QPointF mStartPoint;

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
     *      the value of the char minus 8 represent a factor to represent the speed of the particule :
     *      1 -> near 0 value speed
     *      247 --> max value speed
     *
     */
    QList<char> mDirections;

    /* a trace can be connected with another trace dowstream
     * if null : no trace connected
     * can be connected with itself (loop trace)
     */
    QgsMeshTrace *mDownStreamTrace;

    //pointer to the trace field used to cosntruct the trace

};





/**
 * \ingroup core
 *
 * Class used so store to all the traces in a bounding box
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshTraces
{
  public:

  private:
    //bounding box;
    QgsRectangle mExtent;
};



class QgsMeshTraceRenderer
{
  public:
    QgsMeshTraceRenderer();
};

#endif // QGSMESHTRACERENDERER_H
