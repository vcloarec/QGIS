/***************************************************************************
                         qgsgeometryrubberband.cpp
                         -------------------------
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

#include "qgsgeometryrubberband.h"
#include "qgsabstractgeometry.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include <QPainter>

QgsGeometryRubberBand::QgsGeometryRubberBand( QgsMapCanvas *mapCanvas, QgsWkbTypes::GeometryType geomType ): QgsMapCanvasItem( mapCanvas ),
  mIconSize( 5 ), mIconType( ICON_BOX ), mGeometryType( geomType )
{
  mPen = QPen( QColor( 255, 0, 0 ) );
  mBrush = QBrush( QColor( 255, 0, 0 ) );
}

QgsGeometryRubberBand::~QgsGeometryRubberBand()
{
}

void QgsGeometryRubberBand::paint( QPainter *painter )
{
  if ( !mGeometry || !painter )
  {
    return;
  }

  QgsScopedQPainterState painterState( painter );
  painter->translate( -pos() );

  if ( mGeometryType == QgsWkbTypes::PolygonGeometry )
  {
    painter->setBrush( mBrush );
  }
  else
  {
    painter->setBrush( Qt::NoBrush );
  }
  painter->setPen( mPen );


  std::unique_ptr< QgsAbstractGeometry > paintGeom( mGeometry->clone() );

  paintGeom->transform( mMapCanvas->getCoordinateTransform()->transform() );
  paintGeom->draw( *painter );

  if ( !mDrawVertices )
    return;

  //draw vertices
  QgsVertexId vertexId;
  QgsPoint vertex;
  while ( paintGeom->nextVertex( vertexId, vertex ) )
  {
    drawVertex( painter, vertex.x(), vertex.y() );
  }
}

QgsWkbTypes::GeometryType QgsGeometryRubberBand::geometryType() const
{
  return mGeometryType;
}

void QgsGeometryRubberBand::drawVertex( QPainter *p, double x, double y )
{
  qreal s = ( mIconSize - 1 ) / 2.0;

  switch ( mIconType )
  {
    case ICON_NONE:
      break;

    case ICON_CROSS:
      p->drawLine( QLineF( x - s, y, x + s, y ) );
      p->drawLine( QLineF( x, y - s, x, y + s ) );
      break;

    case ICON_X:
      p->drawLine( QLineF( x - s, y - s, x + s, y + s ) );
      p->drawLine( QLineF( x - s, y + s, x + s, y - s ) );
      break;

    case ICON_BOX:
      p->drawLine( QLineF( x - s, y - s, x + s, y - s ) );
      p->drawLine( QLineF( x + s, y - s, x + s, y + s ) );
      p->drawLine( QLineF( x + s, y + s, x - s, y + s ) );
      p->drawLine( QLineF( x - s, y + s, x - s, y - s ) );
      break;

    case ICON_FULL_BOX:
      p->drawRect( x - s, y - s, mIconSize, mIconSize );
      break;

    case ICON_CIRCLE:
      p->drawEllipse( x - s, y - s, mIconSize, mIconSize );
      break;
  }
}

void QgsGeometryRubberBand::setGeometry( QgsAbstractGeometry *geom )
{
  mGeometry.reset( geom );

  if ( mGeometry )
  {
    setRect( rubberBandRectangle() );
  }
}

void QgsGeometryRubberBand::moveVertex( QgsVertexId id, const QgsPoint &newPos )
{
  if ( mGeometry )
  {
    mGeometry->moveVertex( id, newPos );
    setRect( rubberBandRectangle() );
  }
}

void QgsGeometryRubberBand::setFillColor( const QColor &c )
{
  mBrush.setColor( c );
}

void QgsGeometryRubberBand::setStrokeColor( const QColor &c )
{
  mPen.setColor( c );
}

void QgsGeometryRubberBand::setStrokeWidth( int width )
{
  mPen.setWidth( width );
}

void QgsGeometryRubberBand::setLineStyle( Qt::PenStyle penStyle )
{
  mPen.setStyle( penStyle );
}

void QgsGeometryRubberBand::setBrushStyle( Qt::BrushStyle brushStyle )
{
  mBrush.setStyle( brushStyle );
}

void QgsGeometryRubberBand::setIsVerticesDrawn( bool isVerticesDrawn )
{
  mDrawVertices = isVerticesDrawn;
}

QgsRectangle QgsGeometryRubberBand::rubberBandRectangle() const
{
  qreal scale = mMapCanvas->mapUnitsPerPixel();
  qreal s = ( mIconSize - 1 ) / 2.0 * scale;
  qreal p = mPen.width() * scale;
  return mGeometry->boundingBox().buffered( s + p );
}

QgsCurveRubberBand::QgsCurveRubberBand( QgsMapCanvas *mapCanvas, QgsWkbTypes::GeometryType geomType ):
  QgsGeometryRubberBand( mapCanvas, geomType )
{}

QgsCurve *QgsCurveRubberBand::curve()
{
  if ( mPoints.empty() )
    return nullptr;

  switch ( mStringType )
  {
    case QgsWkbTypes::LineString:
      return new QgsLineString( mPoints ) ;
      break;
    case QgsWkbTypes::CircularString:
      if ( mPoints.count() != 3 )
        return nullptr;
      return new QgsCircularString(
               mPoints[0],
               mPoints[1],
               mPoints[2] ) ;
      break;
    default:
      return nullptr;
  }
}

bool QgsCurveRubberBand::curveIsComplete() const
{
  return ( mStringType == QgsWkbTypes::LineString && mPoints.count() > 1 ) ||
         ( mStringType == QgsWkbTypes::CircularString && mPoints.count() > 2 );
}

void QgsCurveRubberBand::reset( QgsWkbTypes::GeometryType geomType )
{
  if ( !( geomType == QgsWkbTypes::LineGeometry || geomType == QgsWkbTypes::PolygonGeometry ) )
    return;

  mPoints.clear();
  updateCurve();
}

void QgsCurveRubberBand::addPoint( const QgsPointXY &point, bool doUpdate )
{
  if ( mPoints.count() == 0 )
    mPoints.append( QgsPoint( point ) );

  mPoints.append( QgsPoint( point ) );

  if ( doUpdate )
    updateCurve();
}

void QgsCurveRubberBand::movePoint( const QgsPointXY &point )
{
  if ( mPoints.count() > 0 )
    mPoints.last() = QgsPoint( point );

  updateCurve();
}

void QgsCurveRubberBand::movePoint( int index, const QgsPointXY &point )
{
  if ( mPoints.count() > 0 && mPoints.size() > index )
    mPoints[index] = QgsPoint( point );

  updateCurve();
}

int QgsCurveRubberBand::pointsCount()
{
  return mPoints.size();
}

void QgsCurveRubberBand::setFirstPolygonPoint( const QgsPointXY &point )
{
  mFirstPolygonPoint = QgsPoint( point );
}

QgsWkbTypes::Type QgsCurveRubberBand::stringType() const
{
  return mStringType;
}

void QgsCurveRubberBand::setStringType( const QgsWkbTypes::Type &type )
{
  if ( ( type != QgsWkbTypes::CircularString && type != QgsWkbTypes::LineString ) || type == mStringType )
    return;

  mStringType = type;
  if ( type == QgsWkbTypes::LineString && mPoints.count() == 3 )
  {
    mPoints.removeAt( 1 );
  }

  setIsVerticesDrawn( type == QgsWkbTypes::CircularString );

  updateCurve();
}

void QgsCurveRubberBand::setGeometry( QgsAbstractGeometry *geom )
{
  QgsGeometryRubberBand::setGeometry( geom );
}

void QgsCurveRubberBand::updateCurve()
{
  std::unique_ptr<QgsCurve> curve;
  switch ( mStringType )
  {
    case  QgsWkbTypes::LineString:
      curve.reset( createLinearString() );
      break;
    case  QgsWkbTypes::CircularString:
      curve.reset( createCircularString() );
      break;
    default:
      return;
      break;
  }

  if ( geometryType() == QgsWkbTypes::PolygonGeometry )
  {
    std::unique_ptr<QgsCurvePolygon> geom( new QgsCurvePolygon );
    geom->setExteriorRing( curve.release() );
    setGeometry( geom.release() );
    return;
  }

  if ( geometryType() == QgsWkbTypes::LineGeometry )
  {
    setGeometry( curve.release() );
  }
}

QgsCurve *QgsCurveRubberBand::createLinearString()
{
  std::unique_ptr<QgsLineString> curve( new QgsLineString );
  if ( geometryType() == QgsWkbTypes::PolygonGeometry )
  {
    QgsPointSequence points = mPoints;
    points.prepend( mFirstPolygonPoint );
    curve->setPoints( points );
  }
  else
    curve->setPoints( mPoints );

  return curve.release();
}

QgsCurve *QgsCurveRubberBand::createCircularString()
{
  std::unique_ptr<QgsCircularString> curve( new QgsCircularString );
  curve->setPoints( mPoints );
  if ( geometryType() == QgsWkbTypes::PolygonGeometry )
  {
    // add a linear string to close the polygon
    std::unique_ptr<QgsCompoundCurve> polygonCurve( new QgsCompoundCurve );
    polygonCurve->addVertex( mFirstPolygonPoint );
    if ( !mPoints.empty() )
      polygonCurve->addVertex( mPoints.first() );
    polygonCurve->addCurve( curve.release() );
    return polygonCurve.release();
  }
  else
    return curve.release();
}
