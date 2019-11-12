/***************************************************************************
                         qgsmeshtracerenderer.h
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

#ifndef QGSMESHTRACERENDERER_H
#define QGSMESHTRACERENDERER_H

#define SIP_NO_FILE

#include <QVector>
#include <QSize>

#include "qgis_core.h"
#include "qgis.h"
#include "qgstriangularmesh.h"
#include "qgsmeshlayer.h"
#include "qgsmeshlayerutils.h"


///@cond PRIVATE

/**
 * \ingroup core
 *
 * Abstract class used to interpolate the value of the vector for a pixel
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshVectorValueInterpolator
{
  public:
    //! Constructor
    QgsMeshVectorValueInterpolator( QgsTriangularMesh triangularMesh,
                                    QgsMeshDataBlock datasetVectorValues,
                                    QgsMeshDataBlock scalarActiveFaceFlagValues );
    //! Destructor
    virtual ~QgsMeshVectorValueInterpolator();

    //! Returns the interpolated vector
    virtual QgsVector value( const QgsPointXY &point ) = 0;

  protected:
    void updateFace( const QgsPointXY &point );

    QgsTriangularMesh mTriangularMesh;
    QgsMeshDataBlock mDatasetValues;
    QgsMeshDataBlock mActiveFaceFlagValue;
    QgsMeshFace face;
    int mCacheFaceIndex;
};


/**
 * \ingroup core
 *
 * Class used to retrieve the value of the vector for a pixel from vertex
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class  CORE_EXPORT QgsMeshVectorValueInterpolatorFromVertex: public QgsMeshVectorValueInterpolator //CORE_EXPORT needed to no have v-table error ????
{
  public:
    //! Constructor
    QgsMeshVectorValueInterpolatorFromVertex( QgsTriangularMesh triangularMesh,
        QgsMeshDataBlock datasetVectorValues,
        QgsMeshDataBlock scalarActiveFaceFlagValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues, scalarActiveFaceFlagValues )
    {

    }

    QgsVector value( const QgsPointXY &point ) override;


};

/**
 * \ingroup core
 *
 * Abstract class used to cache information about trace;
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshTraceField
{

  public:

    QgsMeshTraceField( const QgsRenderContext &renderContext, QgsMeshVectorValueInterpolator *interpolator, const QgsRectangle &layerExtent, double Vmax );

    virtual ~QgsMeshTraceField() {}

    //! update the size of the fiels and the QgsMapToPixel instance to retrieve map point
    //! from pixel in the field depending on the resolution of the device
    void updateSize();

    //! Returns the size of the field
    QSize size() const;

    //! Returns the topLeft of the field in the device coordinate
    QPoint topLeft() const;

    //! Adds a trace in the field from a start pixel
    void addTrace( QPoint startPixel );

    //! Adds a trace in the fiels from a map point
    void addTrace( QgsPointXY startPoint );

    //! Adds count random traces in the field from random start point
    void addRandomTraces( int count );
    //! Adds a trace in the field from one random start point
    void addRandomTrace();

    //! Adds traces in the field from gridded start points, pixelSpace is the space between points in pixel field
    void addGriddedTraces( int pixelSpace );

    //! Adds traces in the field from start points in the border of the field, pixelSpace is the space between points in pixel field
    void addTracesFromBorder( int pixelSpace );

    //! Sets the width of particle in device pixel, the particle width define the resolution of the field
    void setParticleWidth( int width );

    //!Return the width of particle
    int particleWidth() const;

  protected:
    QPointF fieldToDevice( const QPoint &pixel ) const
    {
      QPointF p( pixel );
      p = mFieldResolution * p + QPointF( mFieldResolution - 1, mFieldResolution - 1 ) / 2;

      return p;
    }

  private:


    QgsPointXY positionToMapCoordinnate( const QPoint &pixelPosition, const QgsPointXY &positionInPixel );



    virtual void setWay( const QPoint &pixel, float dt, float mag, int incX, int incY ) = 0;

    virtual bool way( const QPoint &pixel, float &dt, int &incX, int &incY ) const = 0;

    virtual bool isWayExist( const QPoint &pixel ) = 0;

    virtual void initField() = 0;

    virtual void startTrace( const QPoint &startPixel ) = 0;
    virtual void endTrace( const QPoint &endixel ) = 0;

//attribute
    QgsRectangle mLayerExtent;
    QgsRenderContext mRenderContext;
    QgsMapToPixel mMapToFieldPixel;
    QSize mFieldSize;
    QPoint mFieldTopLeftInDeviceCoordinates;
    bool mValid;

    QgsMeshVectorValueInterpolator *mVectorValueInterpolator;
    double mVmax;
    QImage mTraceImage;
    QPoint mLastPixel;

    //settings
    int mFieldResolution = 1;
};


/**
 * \ingroup core
 *
 * Class used to cache information about dynamic trace (with animation);
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshTraceFieldDynamic : public QgsMeshTraceField
{
  public:
    QgsMeshTraceFieldDynamic( const QgsRenderContext &renderContext,
                              QgsMeshVectorValueInterpolator *interpolator,
                              const QgsRectangle &layerExtent,
                              double Vmax ):
      QgsMeshTraceField( renderContext, interpolator, layerExtent, Vmax )
    {

    }

  private:

    QVector<float> mTimeField;
    QVector<char> mDirectionField;
    QVector<bool> mParticleField;


    /*the direction and the time spent in a pixel is defined with a char value
     *
     *     1  2  3
     *     4  5  6
     *     7  8  9
     *
     *     convenient to retrives the indexes of the next pixel :
     *     Xnext= (v-1)%3-1
     *     Ynext = (v-1)/3-1
     *
     *     and the direction is defined by :
     *     v=incX + 2 + (incY+1)*3
     *
     *      the value of the char minus 8 represent the time spent by the particule in the pixel
     *      1 -> near 0 value speed
     *      6554 --> max value speed
     *
     *      The byte is store in a QImage with format QImage::Format_Indexed8
     *
     */

    void initField() override;

    void setWay( const QPoint &pixel, float dt, float mag, int incX, int incY ) override;

    bool way( const QPoint &pixel, float &dt, int &incX, int &incY ) const override;

    bool isWayExist( const QPoint &pixel ) override;

    void startTrace( const QPoint &startPixel ) override {}
    void endTrace( const QPoint &endPixel ) override {}
};


/**
 * \ingroup core
 *
 * TODO
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
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
 * Abstract class used to return color value for specific pixel trace;
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshTraceColor
{
  public:
    //! Constructor
    QgsMeshTraceColor() = default;
    //! Destructor
    virtual ~QgsMeshTraceColor();
    //! Returns the color associated to the value
    virtual QColor color( float value ) const = 0;


};

/**
 * \ingroup core
 *
 * Class used to return unique color value for all pixel trace;
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshTraceUniqueColor: public QgsMeshTraceColor
{
  public:
    //! Constructor with the default color (black)
    QgsMeshTraceUniqueColor() = default;
    //! Constructor with the given color
    QgsMeshTraceUniqueColor( const QColor &color );

    QColor color( float value ) const override;

  private:
    QColor mColor = Qt::black;

};


/**
 * \ingroup core
 *
 * Class used to return color depending on a color ramp
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshTraceColorRamp: public QgsMeshTraceColor
{
  public:
    //! Constructor
    QgsMeshTraceColorRamp() = default;
    //! Constructor with a color ramp and takes the ownership
    QgsMeshTraceColorRamp( QgsColorRamp *colorRamp );
    ~QgsMeshTraceColorRamp() override;

    QColor color( float value ) const override
    {
      if ( mColorRamp )
        return mColorRamp->color( double( value ) );
      else
        return Qt::black;
    }

  private:
    QgsColorRamp *mColorRamp = nullptr;

};

/**
 * \ingroup core
 *
 * Class used to cache information about static trace (static streamlines)
 *
 * Can be improve to reduce aliasing of streamlines
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshTraceFieldStatic: public QgsMeshTraceField //draw static streamLine
{
  public:

    //! Constructor
    QgsMeshTraceFieldStatic( const QgsRenderContext &renderContext,
                             QgsMeshVectorValueInterpolator *interpolator,
                             const QgsRectangle &layerExtent,
                             double Vmax, QgsMeshTraceColor &traceColor );

    //! Destructor
    ~QgsMeshTraceFieldStatic() override;

    //! Exports the field in png file in the working directory, used for debugging
    void exportImage() const;


    //! Returns the size of the image that represents the trace field
    QSize imageSize() const;

    //! Returns the image that represents the trace field
    QImage image() const;

  private:
    //******************methods
    virtual void initField() override;

    void setWay( const QPoint &pixel, float dt, float mag, int incX, int incY ) override;

    bool way( const QPoint &pixel, float &dt, int &incX, int &incY ) const override {return true;}

    bool isWayExist( const QPoint &pixel ) override;

    void startTrace( const QPoint &startPixel ) override;
    void endTrace( const QPoint &endPixel ) override;

    //******************operating attributes
    QImage mTraceImage;
    QPainter *mPainter = nullptr;
    QPen mPen;
    QVector<float> mMagnitudeField;
    QPointF mLastPixel;
    bool mTraceInProgress = false;
    void( *color )( QPen &pen, const QPoint &pixel );
    QgsMeshTraceColor &mTraceColor;

    //******************settings attributes



};




class QgsMeshTraceRenderer
{
  public:
    enum Type {streamLines, particleAnimation};
    QgsMeshTraceRenderer( const QgsRectangle &layerExtent,
                          QgsTriangularMesh triangularMesh,
                          QgsMeshDataBlock vectorDatasetValues,
                          QgsMeshDataBlock scalarActiveFaceFlagValues,
                          double vectorDatasetMagMinimum,
                          double vectorDatasetMagMaximum,
                          QgsRenderContext rendererContext,
                          QSize outputSize,
                          Type type ):
      mContext( rendererContext )
    {
      mVectorInterpolator = new QgsMeshVectorValueInterpolatorFromVertex( triangularMesh, vectorDatasetValues, scalarActiveFaceFlagValues );
      traceColor = QgsMeshTraceUniqueColor( Qt::white );
      mTraceField = new QgsMeshTraceFieldStatic( rendererContext, mVectorInterpolator, layerExtent, vectorDatasetMagMaximum, traceColor );
    }

    ~QgsMeshTraceRenderer()
    {
      delete mVectorInterpolator;
      delete mTraceField;
    }

    void draw()
    {
      QPainter *painter = mContext.painter();
      painter->save();
      if ( mContext.flags() & QgsRenderContext::Antialiasing )
        painter->setRenderHint( QPainter::Antialiasing, true );

      mTraceField->updateSize();

      mTraceField->addRandomTraces( 5000 );
      //mTraceField->addGriddedTrace( 40 );
      //mTraceField->addTracesFromBorder( 40 );
      QSize imageSize = mTraceField->imageSize();

      QRectF targetRect( mTraceField->topLeft(), imageSize );
      QRectF sourceRect( QPoint( 0, 0 ), imageSize );
      painter->drawImage( targetRect, mTraceField->image(), sourceRect );

      painter->restore();

    }

  private:
    QgsMeshVectorValueInterpolatorFromVertex *mVectorInterpolator;
    QgsMeshTraceFieldStatic *mTraceField;
    QgsRenderContext mContext;
    QgsMeshTraceUniqueColor traceColor;
};

#endif // QGSMESHTRACERENDERER_H
