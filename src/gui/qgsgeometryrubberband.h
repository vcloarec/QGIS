/***************************************************************************
                         qgsgeometryrubberband.h
                         -----------------------
    begin                : December 2014
    copyright            : (C) 2014 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSGEOMETRYRUBBERBAND_H
#define QGSGEOMETRYRUBBERBAND_H

#include "qgsmapcanvasitem.h"
#include "qgswkbtypes.h"
#include <QBrush>
#include <QPen>
#include "qgis_gui.h"

#include "qgscompoundcurve.h"
#include "qgscurvepolygon.h"
#include "qgscircularstring.h"
#include "qgslinestring.h"
#include "qgspoint.h"

#ifdef SIP_RUN
% ModuleHeaderCode
// For ConvertToSubClassCode.
#include <qgsgeometryrubberband.h>
% End
#endif

class QgsAbstractGeometry;
class QgsPoint;
struct QgsVertexId;

/**
 * \ingroup gui
 * A rubberband class for QgsAbstractGeometry (considering curved geometries)*/
class GUI_EXPORT QgsGeometryRubberBand: public QgsMapCanvasItem
{

#ifdef SIP_RUN
    SIP_CONVERT_TO_SUBCLASS_CODE
    if ( dynamic_cast<QgsGeometryRubberBand *>( sipCpp ) )
      sipType = sipType_QgsGeometryRubberBand;
    else
      sipType = nullptr;
    SIP_END
#endif

  public:
    enum IconType
    {

      /**
      * No icon is used
      */
      ICON_NONE,

      /**
       * A cross is used to highlight points (+)
       */
      ICON_CROSS,

      /**
       * A cross is used to highlight points (x)
       */
      ICON_X,

      /**
       * A box is used to highlight points (□)
       */
      ICON_BOX,

      /**
       * A circle is used to highlight points (○)
       */
      ICON_CIRCLE,

      /**
       * A full box is used to highlight points (■)
       */
      ICON_FULL_BOX
    };

    QgsGeometryRubberBand( QgsMapCanvas *mapCanvas, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::LineGeometry );
    virtual ~QgsGeometryRubberBand() override;

    virtual void reset( QgsWkbTypes::GeometryType geomType = QgsWkbTypes::LineGeometry )
    {
      mGeometry.reset();
      mGeometryType = geomType;
    }

    //! Sets geometry (takes ownership). Geometry is expected to be in map coordinates
    virtual void setGeometry( QgsAbstractGeometry *geom SIP_TRANSFER );
    //! Returns a pointer to the geometry
    const QgsAbstractGeometry *geometry() { return mGeometry.get(); }
    //! Moves vertex to new position (in map coordinates)
    void moveVertex( QgsVertexId id, const QgsPoint &newPos );
    //! Sets fill color for vertex markers
    void setFillColor( const QColor &c );
    //! Sets stroke color for vertex markers
    void setStrokeColor( const QColor &c );
    //! Sets stroke width
    void setStrokeWidth( int width );
    //! Sets pen style
    void setLineStyle( Qt::PenStyle penStyle );
    //! Sets brush style
    void setBrushStyle( Qt::BrushStyle brushStyle );
    //! Sets vertex marker icon type
    void setIconType( IconType iconType ) { mIconType = iconType; }
    //! Sets whether the vertices are drawn
    void setIsVerticesDrawn( bool isVerticesDrawn );

  protected:
    void paint( QPainter *painter ) override;
    QgsWkbTypes::GeometryType geometryType() const;

  private:
    std::unique_ptr<QgsAbstractGeometry> mGeometry = nullptr;
    QBrush mBrush;
    QPen mPen;
    int mIconSize;
    IconType mIconType;
    QgsWkbTypes::GeometryType mGeometryType;
    bool mDrawVertices = true;

    void drawVertex( QPainter *p, double x, double y );
    QgsRectangle rubberBandRectangle() const;
};

/**
 * Class that reprensents a rubber can that can be linear or circular.
 */
class QgsCurveRubberBand: public QgsGeometryRubberBand
{
  public:
    //! Constructor
    QgsCurveRubberBand( QgsMapCanvas *mapCanvas, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::LineGeometry );

    //! Returns the curve defined by the rubber band, the caller has to take the ownership, nullptr if no curve is defined.
    QgsCurve *curve();

    /**
     * Returns if the curve defined by the rubber band is complete :
     * has more than 2 points for circular string and more than 1 point for linear string
     */
    bool curveIsComplete() const;

    /**
     * Resets the rubber band with the specified geometry type
     * that must be line geometry or polygon geometry.
     */
    void reset( QgsWkbTypes::GeometryType geomType = QgsWkbTypes::LineGeometry ) override;

    //! Adds point to the rubber band
    void addPoint( const QgsPointXY &point, bool doUpdate = true );

    //! Moves the last point to the \a point position
    void movePoint( const QgsPointXY &point );

    //! Moves the point with \a index to the \a point position
    void movePoint( int index, const QgsPointXY &point );

    //! Returns the points count in the rubber band (except the first point if polygon)
    int pointsCount();

    /**
     * Sets the first point that serves to render polygon rubber band
     */
    void setFirstPolygonPoint( const QgsPointXY &point );

    //! Returns the type of the curve (linear string or circular string)
    QgsWkbTypes::Type stringType() const;

    //! Sets the type of the curve (linear string or circular string)
    void setStringType( const QgsWkbTypes::Type &type );

    QgsPoint lastPoint() const;

    void removeLastPoint();

  private:
    QgsWkbTypes::Type mStringType = QgsWkbTypes::LineString;

    void setGeometry( QgsAbstractGeometry *geom ) override;
    void updateCurve();

    QgsCurve *createLinearString();
    QgsCurve *createCircularString();

    QgsPointSequence mPoints;
    QgsPoint mFirstPolygonPoint;
};

#endif // QGSGEOMETRYRUBBERBAND_H
