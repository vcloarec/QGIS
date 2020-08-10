/***************************************************************************
                          qgsmeshtriangulation.cpp
                          -----------------
    begin                : August 9th, 2020
    copyright            : (C) 2020 by Vincent Cloarec
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

#include "qgsmeshtriangulation.h"
#include "qgsdualedgetriangulation.h"
#include "qgsvectorlayer.h"
#include "qgscoordinatetransformcontext.h"
#include "qgscurve.h"
#include "qgscurvepolygon.h"
#include "qgsmultisurface.h"
#include "qgsmulticurve.h"

QgsMeshTriangulation::QgsMeshTriangulation()
{
  mTriangulation.reset( new QgsDualEdgeTriangulation() );
}

bool QgsMeshTriangulation::addVertices( QgsVectorLayer *vectorLayer, int valueAttribute, const QgsCoordinateTransformContext &transformContext )
{
  if ( !vectorLayer )
    return false;

  if ( !vectorLayer->isValid() )
    return false;

  QgsCoordinateTransform transform( vectorLayer->crs(), mCrs, transformContext );

  QgsFeatureIterator fIt = vectorLayer->getFeatures();
  QgsFeature feat;
  bool isZvalueGeom = valueAttribute < 0;
  while ( fIt.nextFeature( feat ) )
  {
    QgsGeometry geom = feat.geometry();
    geom.transform( transform, QgsCoordinateTransform::ForwardTransform, true );
    QgsAbstractGeometry::vertex_iterator vit = geom.vertices_begin();

    double value = 0;
    if ( !isZvalueGeom )
      value = feat.attribute( valueAttribute ).toDouble();

    while ( vit != geom.vertices_end() )
    {
      if ( isZvalueGeom )
        mTriangulation->addPoint( *vit );
      else
      {
        QgsPoint point = *vit;
        point.setZ( value );
        mTriangulation->addPoint( point );
      }
      ++vit;
    }
  }

  return true;
}

bool QgsMeshTriangulation::addBreakLines( QgsVectorLayer *vectorLayer, int valueAttribute, const QgsCoordinateTransformContext &transformContext )
{
  if ( !vectorLayer )
    return false;

  if ( !vectorLayer->isValid() )
    return false;

  QgsCoordinateTransform transform( vectorLayer->crs(), mCrs, transformContext );

  QgsWkbTypes::GeometryType geomType = vectorLayer->geometryType();

  switch ( geomType )
  {
    case QgsWkbTypes::PointGeometry:
      return addVertices( vectorLayer, valueAttribute, transformContext );
      break;
    case QgsWkbTypes::LineGeometry:
    case QgsWkbTypes::PolygonGeometry:
    {
      QgsFeatureIterator fIt = vectorLayer->getFeatures();
      QgsFeature feat;
      while ( fIt.nextFeature( feat ) )
      {
        addBreakLinesFromFeature( feat, valueAttribute, transform );
      }

      return true;
    }
    break;
    default:
      return false;
  }
}

QgsMesh QgsMeshTriangulation::triangulatedMesh() const
{
  return mTriangulation->triangulationToMesh();
}

void QgsMeshTriangulation::setCrs( const QgsCoordinateReferenceSystem &crs )
{
  mCrs = crs;
}

void QgsMeshTriangulation::addBreakLinesFromFeature( const QgsFeature &feature, int valueAttribute, const QgsCoordinateTransform &transform )
{
  double valueOnVertex = 0;
  if ( valueAttribute >= 0 )
    valueOnVertex = feature.attribute( valueAttribute ).toDouble();

  //from QgsTinInterpolator::insertData()
  std::vector<const QgsCurve *> curves;
  QgsGeometry geom = feature.geometry();
  geom.transform( transform, QgsCoordinateTransform::ForwardTransform, true );
  if ( QgsWkbTypes::geometryType( geom.wkbType() ) == QgsWkbTypes::PolygonGeometry )
  {
    std::vector< const QgsCurvePolygon * > polygons;
    if ( geom.isMultipart() )
    {
      const QgsMultiSurface *ms = qgsgeometry_cast< const QgsMultiSurface * >( geom.constGet() );
      for ( int i = 0; i < ms->numGeometries(); ++i )
      {
        polygons.emplace_back( qgsgeometry_cast< const QgsCurvePolygon * >( ms->geometryN( i ) ) );
      }
    }
    else
    {
      polygons.emplace_back( qgsgeometry_cast< const QgsCurvePolygon * >( geom.constGet() ) );
    }

    for ( const QgsCurvePolygon *polygon : polygons )
    {
      if ( !polygon )
        continue;

      if ( polygon->exteriorRing() )
        curves.emplace_back( polygon->exteriorRing() );

      for ( int i = 0; i < polygon->numInteriorRings(); ++i )
      {
        curves.emplace_back( polygon->interiorRing( i ) );
      }
    }
  }
  else
  {
    if ( geom.isMultipart() )
    {
      const QgsMultiCurve *mc = qgsgeometry_cast< const QgsMultiCurve * >( geom.constGet() );
      for ( int i = 0; i < mc->numGeometries(); ++i )
      {
        curves.emplace_back( qgsgeometry_cast< const QgsCurve * >( mc->geometryN( i ) ) );
      }
    }
    else
    {
      curves.emplace_back( qgsgeometry_cast< const QgsCurve * >( geom.constGet() ) );
    }
  }

  for ( const QgsCurve *curve : curves )
  {
    if ( !curve )
      continue;

    QgsPointSequence linePoints;
    curve->points( linePoints );
    if ( valueAttribute >= 0 )
      for ( QgsPoint &point : linePoints )
        point.setZ( valueOnVertex );

    mTriangulation->addLine( linePoints, QgsInterpolator::SourceBreakLines );
  }
}
