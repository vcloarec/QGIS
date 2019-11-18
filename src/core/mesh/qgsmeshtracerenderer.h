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
#include "qgsmeshvectorrenderer.h"


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
    QgsMeshVectorValueInterpolator( const QgsTriangularMesh &triangularMesh,
                                    const QgsMeshDataBlock &datasetVectorValues );

    //! Constructor with scalar active face flag values to not interpolate on inactive face
    QgsMeshVectorValueInterpolator( const QgsTriangularMesh &triangularMesh,
                                    const QgsMeshDataBlock &datasetVectorValues,
                                    const QgsMeshDataBlock &scalarActiveFaceFlagValues );
    //! Destructor
    virtual ~QgsMeshVectorValueInterpolator() = default;

    //! Sets scalar dataset values used to weight the vector
    void setWeightScalarDataset( const QVector<double> &weightDatasetValues, bool dataOnVertices )
    {
      mWeightDatasetValues = weightDatasetValues;
      mIsWeightedWithScalar = true;
      mIsWeightScalarOnVertices = dataOnVertices;
    }

    //! Returns the interpolated vector
    //virtual QgsVector vectorValue( const QgsPointXY &point ) const;

    //! Returns the interpolated vector and give the weight associated
    virtual QgsVector vectorValue( const QgsPointXY &point, double &weight ) const ;

  protected:
    void updateCacheFaceIndex( const QgsPointXY &point ) const;

    QgsTriangularMesh mTriangularMesh;
    QgsMeshDataBlock mDatasetValues;
    QVector<double> mWeightDatasetValues;
    QgsMeshDataBlock mActiveFaceFlagValues;
    mutable QgsMeshFace mFaceCache;
    mutable int mCacheFaceIndex = -1;
    mutable QMutex mMutex;
    bool mUseScalarActiveFaceFlagValues = false;
    bool mIsWeightedWithScalar = false;
    bool mIsWeightScalarOnVertices = false;


    bool isVectorValid( const QgsVector &v ) const
    {
      return !( std::isnan( v.x() ) || std::isnan( v.y() ) );

    }

    void activeFaceFilter( const QgsPointXY &point, QgsVector &vector, int faceIndex ) const
    {
      if ( mUseScalarActiveFaceFlagValues && ! mActiveFaceFlagValues.active( mTriangularMesh.trianglesToNativeFaces()[faceIndex] ) )
        vector = QgsVector( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() ) ;
    }


    double weightOnPoint( const QgsPointXY &point,  int faceIndex ) const
    {
      if ( mIsWeightedWithScalar )
      {
        QgsMeshFace face = mTriangularMesh.triangles().at( faceIndex );

        QgsPoint p1 = mTriangularMesh.vertices().at( face.at( 0 ) );
        QgsPoint p2 = mTriangularMesh.vertices().at( face.at( 1 ) );
        QgsPoint p3 = mTriangularMesh.vertices().at( face.at( 2 ) );

        double weight;

        if ( mIsWeightScalarOnVertices )
        {
          double v1 = mWeightDatasetValues.at( face.at( 0 ) );
          double v2 = mWeightDatasetValues.at( face.at( 1 ) );
          double v3 = mWeightDatasetValues.at( face.at( 2 ) );
          weight = QgsMeshLayerUtils::interpolateFromVerticesData( p1, p2, p3, v1, v2, v3, point );
        }
        else
        {
          double v = mWeightDatasetValues.at( mTriangularMesh.trianglesToNativeFaces().at( faceIndex ) );
          weight = QgsMeshLayerUtils::interpolateFromFacesData( p1, p2, p3, v, point );
        }
        return weight;
      }
      else
        return 1;
    }

    virtual QgsVector interpolatedValuePrivate( int faceIndex, const QgsPointXY point ) const = 0;
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
    QgsMeshVectorValueInterpolatorFromVertex( const QgsTriangularMesh &triangularMesh,
        const QgsMeshDataBlock &datasetVectorValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues )
    {

    }

    //! Constructor with scalar active face flag value to not interpolate on inactive face
    QgsMeshVectorValueInterpolatorFromVertex( const QgsTriangularMesh &triangularMesh,
        const QgsMeshDataBlock &datasetVectorValues,
        const QgsMeshDataBlock &scalarActiveFaceFlagValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues, scalarActiveFaceFlagValues )
    {

    }

    QgsVector interpolatedValuePrivate( int faceIndex, const QgsPointXY point ) const override;
};

/**
 * \ingroup core
 *
 * Class used to retrieve the value of the vector for a pixel from vertex
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class  CORE_EXPORT QgsMeshVectorValueInterpolatorFromFace: public QgsMeshVectorValueInterpolator //CORE_EXPORT needed to no have v-table error ????
{
  public:
    //! Constructor
    QgsMeshVectorValueInterpolatorFromFace( const QgsTriangularMesh &triangularMesh,
                                            const QgsMeshDataBlock &datasetVectorValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues )
    {

    }

    //! Constructor with scalar active face flag value to not interpolate on inactive face
    QgsMeshVectorValueInterpolatorFromFace( const QgsTriangularMesh &triangularMesh,
                                            const QgsMeshDataBlock &datasetVectorValues,
                                            const QgsMeshDataBlock &scalarActiveFaceFlagValues ):
      QgsMeshVectorValueInterpolator( triangularMesh, datasetVectorValues, scalarActiveFaceFlagValues )
    {

    }

  protected:
    QgsVector interpolatedValuePrivate( int faceIndex, const QgsPointXY point ) const override
    {
      QgsMeshFace face = mTriangularMesh.triangles().at( faceIndex );

      QgsPoint p1 = mTriangularMesh.vertices().at( face.at( 0 ) );
      QgsPoint p2 = mTriangularMesh.vertices().at( face.at( 1 ) );
      QgsPoint p3 = mTriangularMesh.vertices().at( face.at( 2 ) );

      QgsVector vect = QgsVector( mDatasetValues.value( mTriangularMesh.trianglesToNativeFaces().at( faceIndex ) ).x(),
                                  mDatasetValues.value( mTriangularMesh.trianglesToNativeFaces().at( faceIndex ) ).y() );

      return QgsMeshLayerUtils::interpolateVectorFromFacesData(
               p1,
               p2,
               p3,
               vect,
               point );
    }
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
    QgsMeshTraceField( const QgsTriangularMesh &triangularMesh,
                       const QgsMeshDataBlock &dataSetVectorValues,
                       const QgsMeshDataBlock &scalarActiveFaceFlagValues,
                       const QgsRectangle &layerExtent,
                       double magMax, bool dataIsOnVertices,
                       int resolution = 1 ):
      mFieldResolution( resolution ),
      mLayerExtent( layerExtent ),
      mMagMax( magMax )
    {
      if ( dataIsOnVertices )
      {
        if ( scalarActiveFaceFlagValues.isValid() )
          mVectorValueInterpolator.reset( new QgsMeshVectorValueInterpolatorFromVertex( triangularMesh,
                                          dataSetVectorValues,
                                          scalarActiveFaceFlagValues ) );
        else
          mVectorValueInterpolator.reset( new QgsMeshVectorValueInterpolatorFromVertex( triangularMesh,
                                          dataSetVectorValues ) );
      }
      else
      {
        if ( scalarActiveFaceFlagValues.isValid() )
          mVectorValueInterpolator.reset( new QgsMeshVectorValueInterpolatorFromFace( triangularMesh,
                                          dataSetVectorValues,
                                          scalarActiveFaceFlagValues ) );
        else
          mVectorValueInterpolator.reset( new QgsMeshVectorValueInterpolatorFromFace( triangularMesh,
                                          dataSetVectorValues ) );
      }
    }

    virtual ~QgsMeshTraceField() {}

    //! update the size of the fiels and the QgsMapToPixel instance to retrieve map point
    //! from pixel in the field depending on the resolution of the device
    //! If the extent of renderer context and the resolution are not changed, do nothing
    //! else, updates the size and cleans
    void updateSize( const QgsRenderContext &renderContext );

    //! update the size of the fiels and the QgsMapToPixel instance to retrieve map point
    //! from pixel in the field depending on the resolution of the device
    void updateSize( const QgsRenderContext &renderContext, int resolution );


    bool isValid() const;

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

    //! Adds count random traces in the field from random start point, the number of traces dependinf on the max filling density
    void addRandomTraces();

    //! Adds a trace in the field from one random start point
    void addRandomTrace();

    //! Adds traces in the field from gridded start points, pixelSpace is the space between points in pixel field
    void addGriddedTraces();

    //! Adds traces in the field from start points in the border of the field, pixelSpace is the space between points in pixel field
    void addTracesFromBorder();

    //! Sets the resolution of the field
    void setResolution( int width );

    //!Return the width of particle
    int resolution() const;

    //! Returns the size of the image that represents the trace field
    QSize imageSize() const;

    //! Returns the current render image of the field
    virtual QImage image();

    //! used to stop processing when working with multithreads
    void stopProcessing()
    {
      mProcessingStop = true;
    }

    //! Returns true if a way was already defined in the pixel
    bool isWayExist( const QPoint &pixel ) const;

    //! Gives information on the way defined in the pixel. If no way was defined, returns false.
    virtual bool way( const QPoint &pixel, float &dt, int &incX, int &incY ) const;

    //! dets the maximum pixel filling, eg, the rate of number pixel that can be filled with way.
    void setPixelFillingDensity( double maxFilling )
    {
      mPixelFillingDensity = maxFilling;
      mMaxPixelFillingCount = int( mPixelFillingDensity * mFieldSize.width() * mFieldSize.height() );
    }

  protected:
    QPointF fieldToDevice( const QPoint &pixel ) const;
    QImage mTraceImage;
    QPainter *mPainter = nullptr;
    bool mProcessingStop = false;
    QSize mFieldSize;
    int mFieldResolution = 1;
    mutable QMutex mMutex;
    int mPixelFillingCount = 0;
    int mMaxPixelFillingCount = 0;
    std::unique_ptr<QgsMeshVectorValueInterpolator> mVectorValueInterpolator;


  private:


    QgsPointXY positionToMapCoordinates( const QPoint &pixelPosition, const QgsPointXY &positionInPixel );


    //Set the way in the field, not thread safe
    virtual void setWayPrivate( const QPoint &pixel, float dt, float mag, int incX, int incY ) = 0;

    virtual bool wayPrivate( const QPoint &pixel, float &dt, int &incX, int &incY ) const = 0;

    virtual bool isWayExistPrivate( const QPoint &pixel ) const = 0;

    virtual void initField() = 0;

    virtual void startTrace( const QPoint &startPixel ) = 0;
    virtual void endTrace( const QPoint &endixel ) = 0;

//attribute
    QgsRectangle mLayerExtent;
    QgsRectangle mMapExtent;
    QgsMapToPixel mMapToFieldPixel;
    QPoint mFieldTopLeftInDeviceCoordinates;
    bool mValid = false;
    double mMagMax = 0;
    QPoint mLastPixel;
    double mPixelFillingDensity;





};


class QgsMeshTraceFieldDynamic;


/**
 * \ingroup core
 *
 * TODO
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class QgsMeshTraceParticle
{
  public:
    QgsMeshTraceParticle( const QPoint &startPoint ):
      mCurrentPositionInTheField( startPoint )
    {

    }

    void move( float totalTime, const QgsMeshTraceFieldDynamic &field );

    void  draw( QgsMeshTraceFieldDynamic &field, double width ) const;

    bool isLost() const;

  private:
    QPoint mCurrentPositionInTheField;
    QList<QPoint> mPath;
    float mTimeRemaingInTheCurrentPixel = 0;
    bool mLost = false;
    float mTimeLife = 1000;
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
    QgsMeshTraceFieldDynamic( const QgsTriangularMesh &triangularMesh,
                              const QgsMeshDataBlock &dataSetVectorValues,
                              const QgsMeshDataBlock &scalarActiveFaceFlagValues,
                              bool dataIsOnVertices,
                              const QgsRenderContext &renderContext,
                              const QgsRectangle &layerExtent,
                              double magMax ):
      QgsMeshTraceField( triangularMesh, dataSetVectorValues, scalarActiveFaceFlagValues, layerExtent, magMax, dataIsOnVertices )
    {
    }

    void addParticle( QPoint startPoint )
    {

      if ( !isWayExist( startPoint ) )
        addTrace( startPoint );

      if ( !isWayExist( startPoint ) )
        return;

      QMutexLocker locker( &mMutex );
      mParticles.append( QgsMeshTraceParticle( startPoint ) );
      mParticlesCount++;
    }

    void addRandomParticle()
    {
      if ( !isValid() )
        return;

      int xRandom =  1 + std::rand() / int( ( RAND_MAX + 1u ) / uint( size().width() ) )  ;
      int yRandom = 1 + std::rand() / int ( ( RAND_MAX + 1u ) / uint( size().height() ) ) ;

      addParticle( QPoint( xRandom, yRandom ) );
    }

    void addRandomParticles( int count )
    {
      while ( mParticlesCount < count && !mProcessingStop )
      {
        addRandomParticle();
      }
    }

    void moveParticles( float time );

    int particleCount() const
    {
      QMutexLocker locjker( &mMutex );
      return  mParticlesCount;
    }

    void drawTrace( const QRectF &part )
    {
      QMutexLocker locker( &mMutexDrawing );
      mPainter->drawEllipse( part );
    }



  private:

    QVector<float> mTimeField;
    QVector<char> mDirectionField;
    QVector<bool> mParticleField;
    QList<QgsMeshTraceParticle> mParticles;
    int mParticlesCount = 0;
    QImage mShaderImg;
    QMutex mMutexDrawing;


    /*the direction and the time spent in a pixel is defined with a char value
     *
     *     1  2  3
     *     4  5  6
     *     7  8  9
     *
     *     convenient to retrieve the indexes of the next pixel :
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

    void setWayPrivate( const QPoint &pixel, float dt, float mag, int incX, int incY ) override;
    bool isWayExistPrivate( const QPoint &pixel ) const override;
    bool wayPrivate( const QPoint &pixel, float &dt, int &incX, int &incY ) const override;

    void startTrace( const QPoint &startPixel ) override {}
    void endTrace( const QPoint &endPixel ) override {}

    friend class QgsMeshTraceParticle;

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
class CORE_EXPORT QgsMeshTraceFixedColor: public QgsMeshTraceColor
{
  public:
    //! Constructor with the default color (black)
    QgsMeshTraceFixedColor() = default;
    //! Constructor with the given color
    QgsMeshTraceFixedColor( const QColor &color );

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
    QgsMeshTraceColorRamp( const QgsColorRampShader &colorRamp );
    ~QgsMeshTraceColorRamp() override;

    //!Return the color
    QColor color( float value ) const override
    {
      if ( mColorRampShader.sourceColorRamp() )
        return mColorRampShader.sourceColorRamp()->color( double( value ) );
      else
        return Qt::black;
    }

  private:
    QgsColorRampShader mColorRampShader;

};

/**
 * \ingroup core
 *
 * Class used to draw static streamlines
 *
 * Can be improve to reduce aliasing of streamlines
 *
 * \note not available in Python bindings
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsMeshStreamLineField: public QgsMeshTraceField //draw static streamLine
{
  public:

    //! Constructor
    QgsMeshStreamLineField( const QgsTriangularMesh &triangularMesh,
                            const QgsMeshDataBlock &dataSetVectorValues,
                            const QgsMeshDataBlock &scalarActiveFaceFlagValues,
                            const QVector<double> &datasetVectorScalarWeightValues,
                            bool datasetVectorScalarWeightValuesOnVertices,
                            const QgsRenderContext &renderContext,
                            const QgsRectangle &layerExtent,
                            double Vmax,
                            bool dataIsOnVertices );

    //! Destructor
    ~QgsMeshStreamLineField() override;

    //! Exports the field in png file in the working directory, used for debugging
    void exportImage() const;

    void setLineWidth( double width )
    {
      mPen.setWidthF( width );
    }


    void setTraceColor( const QgsColorRampShader &shader );
    void setTraceColor( QColor fixedColor );

  private:
    //******************methods
    virtual void initField() override;

    void setWayPrivate( const QPoint &pixel, float dt, float mag, int incX, int incY ) override;

    bool wayPrivate( const QPoint &pixel, float &dt, int &incX, int &incY ) const override {return true;}

    bool isWayExistPrivate( const QPoint &pixel ) const override;

    void startTrace( const QPoint &startPixel ) override;
    void endTrace( const QPoint &endPixel ) override;

    //******************operating attributes

    QPen mPen;
    QVector<float> mMagnitudeField;
    QPointF mLastPixel;
    bool mTraceInProgress = false;
    std::unique_ptr<QgsMeshTraceColor> mTraceColor = std::unique_ptr<QgsMeshTraceColor>( new QgsMeshTraceFixedColor() );

    //******************settings attributes



};




class QgsMeshVectorStreamLineRenderer: public QgsMeshVectorRenderer
{
  public:
    //!Constructor
    QgsMeshVectorStreamLineRenderer( const QgsTriangularMesh &triangularMesh,
                                     const QgsMeshDataBlock &dataSetVectorValues,
                                     const QgsMeshDataBlock &scalarActiveFaceFlagValues,
                                     const QVector<double> &datasetVectorScalarWeightValues,
                                     bool datasetVectorScalarWeightValuesOnVertices,
                                     bool dataIsOnVertices,
                                     const QgsMeshRendererVectorStreamlineSettings &settings,
                                     QgsRenderContext &rendererContext,
                                     const QgsRectangle &layerExtent,
                                     double magMax ):
      mRendererContext( rendererContext )
    {
      mStreamLineField.reset( new QgsMeshStreamLineField( triangularMesh,
                              dataSetVectorValues,
                              scalarActiveFaceFlagValues,
                              datasetVectorScalarWeightValues,
                              datasetVectorScalarWeightValuesOnVertices,
                              rendererContext,
                              layerExtent,
                              magMax, dataIsOnVertices ) );

      mStreamLineField->updateSize( rendererContext );
      mStreamLineField->setPixelFillingDensity( settings.seedingDensity() );
      mStreamLineField->setLineWidth(
        rendererContext.convertToPainterUnits( settings.lineWidth(), QgsUnitTypes::RenderUnit::RenderMillimeters ) );


      switch ( settings.colorMethod() )
      {
        case QgsMeshRendererVectorStreamlineSettings::Fixe:
          mStreamLineField->setTraceColor( settings.fixedColor() );
          break;
        case QgsMeshRendererVectorStreamlineSettings::ColorRamp:
          mStreamLineField->setTraceColor( settings.colorRampShader() );
          break;
      }

      switch ( settings.seedingMethod() )
      {
        case QgsMeshRendererVectorStreamlineSettings::Gridded:
          mStreamLineField->addGriddedTraces();
          break;
        case QgsMeshRendererVectorStreamlineSettings::OnBorder:
          mStreamLineField->addTracesFromBorder();
          break;
        case QgsMeshRendererVectorStreamlineSettings::Random:
          mStreamLineField->addRandomTraces();
          break;
      }

      //mStreamLineField->addTrace( QgsPointXY( 59, -24 ) ); //for debugging
    }

    //!Constructor to used when the steamline already exists (cache)
    QgsMeshVectorStreamLineRenderer( QgsMeshStreamLineField *streamLineField, QgsRenderContext &rendererContext ):
      mStreamLineField( streamLineField ),
      mRendererContext( rendererContext )
    {

    }

    void draw()
    {
      if ( mRendererContext.renderingStopped() )
        return;
      mRendererContext.painter()->drawImage( mStreamLineField->topLeft(), mStreamLineField->image() );
    }

  private:
    std::unique_ptr<QgsMeshStreamLineField> mStreamLineField;
    QgsRenderContext &mRendererContext;
};

#endif // QGSMESHTRACERENDERER_H
