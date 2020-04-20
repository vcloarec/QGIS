/***************************************************************************
                         qgsmeshlayerrenderer.cpp
                         ------------------------
    begin                : April 2018
    copyright            : (C) 2018 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <memory>
#include <QSet>
#include <QPair>
#include <QLinearGradient>
#include <QBrush>
#include <algorithm>

#include "qgsmeshlayerrenderer.h"

#include "qgsfield.h"
#include "qgslogger.h"
#include "qgsmeshlayer.h"
#include "qgspointxy.h"
#include "qgsrenderer.h"
#include "qgssinglebandpseudocolorrenderer.h"
#include "qgsrastershader.h"
#include "qgsmeshlayerinterpolator.h"
#include "qgsmeshlayerutils.h"
#include "qgsmeshvectorrenderer.h"
#include "qgsmeshtracerenderer.h"
#include "qgsfillsymbollayer.h"
#include "qgssettings.h"
#include "qgsstyle.h"
#include "qgsmeshdataprovidertemporalcapabilities.h"

QgsMeshLayerRenderer::QgsMeshLayerRenderer(
  QgsMeshLayer *layer,
  QgsRenderContext &context )
  : QgsMapLayerRenderer( layer->id(), &context )
  , mFeedback( new QgsMeshLayerRendererFeedback )
  , mRendererSettings( layer->rendererSettings() )
{
  // make copies for mesh data
  Q_ASSERT( layer->nativeMesh() );
  Q_ASSERT( layer->triangularMesh() );
  Q_ASSERT( layer->rendererCache() );
  Q_ASSERT( layer->dataProvider() );

  // copy native mesh
  mNativeMesh = *( layer->nativeMesh() );
  mLayerExtent = layer->extent();

  // copy triangular mesh
  copyTriangularMeshes( layer, context );

  // copy datasets
  copyScalarDatasetValues( layer );
  copyVectorDatasetValues( layer );

  calculateOutputSize();
}

void QgsMeshLayerRenderer::copyTriangularMeshes( QgsMeshLayer *layer, QgsRenderContext &context )
{
  // handle level of details of mesh
  QgsMeshSimplificationSettings simplificationSettings = layer->meshSimplificationSettings();
  if ( simplificationSettings.isEnabled() )
  {
    double triangleSize = simplificationSettings.meshResolution() * context.mapToPixel().mapUnitsPerPixel();
    mTriangularMesh = *( layer->triangularMesh( triangleSize ) );
    mIsMeshSimplificationActive = true;
  }
  else
  {
    mTriangularMesh = *( layer->triangularMesh() );
  }
}

QgsFeedback *QgsMeshLayerRenderer::feedback() const
{
  return mFeedback.get();
}

void QgsMeshLayerRenderer::calculateOutputSize()
{
  // figure out image size
  QgsRenderContext &context = *renderContext();
  QgsRectangle extent = context.mapExtent();
  QgsMapToPixel mapToPixel = context.mapToPixel();
  QgsPointXY topleft = mapToPixel.transform( extent.xMinimum(), extent.yMaximum() );
  QgsPointXY bottomright = mapToPixel.transform( extent.xMaximum(), extent.yMinimum() );
  int width = int( bottomright.x() - topleft.x() );
  int height = int( bottomright.y() - topleft.y() );
  mOutputSize = QSize( width, height );
}

void QgsMeshLayerRenderer::copyScalarDatasetValues( QgsMeshLayer *layer )
{
  QgsMeshDatasetIndex datasetIndex;
  if ( renderContext()->isTemporal() )
    datasetIndex = layer->activeScalarDatasetAtTime( renderContext()->temporalRange() );
  else
    datasetIndex = layer->staticScalarDatasetIndex();

  // Find out if we can use cache up to date. If yes, use it and return
  const int datasetGroupCount = layer->dataProvider()->datasetGroupCount();
  const QgsMeshRendererScalarSettings::DataResamplingMethod method = mRendererSettings.scalarSettings( datasetIndex.group() ).dataResamplingMethod();
  QgsMeshLayerRendererCache *cache = layer->rendererCache();
  if ( ( cache->mDatasetGroupsCount == datasetGroupCount ) &&
       ( cache->mActiveScalarDatasetIndex == datasetIndex ) &&
       ( cache->mDataInterpolationMethod ==  method ) &&
       ( QgsMesh3dAveragingMethod::equals( cache->mScalarAveragingMethod.get(), mRendererSettings.averagingMethod() ) )
     )
  {
    mScalarDatasetValues = cache->mScalarDatasetValues;
    mScalarActiveFaceFlagValues = cache->mScalarActiveFaceFlagValues;
    mScalarDataType = cache->mScalarDataType;
    mScalarDatasetMinimum = cache->mScalarDatasetMinimum;
    mScalarDatasetMaximum = cache->mScalarDatasetMaximum;
    return;
  }

  // Cache is not up-to-date, gather data
  if ( datasetIndex.isValid() )
  {
    const QgsMeshDatasetGroupMetadata metadata = layer->dataProvider()->datasetGroupMetadata( datasetIndex.group() );
    mScalarDataType = QgsMeshLayerUtils::datasetValuesType( metadata.dataType() );

    // populate scalar values
    const int count = QgsMeshLayerUtils::datasetValuesCount( &mNativeMesh, mScalarDataType );
    QgsMeshDataBlock vals = QgsMeshLayerUtils::datasetValues(
                              layer,
                              datasetIndex,
                              0,
                              count );

    if ( vals.isValid() )
    {
      // vals could be scalar or vectors, for contour rendering we want always magnitude
      mScalarDatasetValues = QgsMeshLayerUtils::calculateMagnitudes( vals );
    }
    else
    {
      mScalarDatasetValues = QVector<double>( count, std::numeric_limits<double>::quiet_NaN() );
    }

    // populate face active flag, always defined on faces
    mScalarActiveFaceFlagValues = layer->dataProvider()->areFacesActive(
                                    datasetIndex,
                                    0,
                                    mNativeMesh.faces.count() );

    // for data on faces, there could be request to interpolate the data to vertices
    if ( method != QgsMeshRendererScalarSettings::None )
    {
      if ( mScalarDataType == QgsMeshDatasetGroupMetadata::DataType::DataOnFaces )
      {
        mScalarDataType = QgsMeshDatasetGroupMetadata::DataType::DataOnVertices;
        mScalarDatasetValues = QgsMeshLayerUtils::interpolateFromFacesData(
                                 mScalarDatasetValues,
                                 &mNativeMesh,
                                 &mTriangularMesh,
                                 &mScalarActiveFaceFlagValues,
                                 method
                               );
      }
      else if ( mScalarDataType == QgsMeshDatasetGroupMetadata::DataType::DataOnVertices )
      {
        mScalarDataType = QgsMeshDatasetGroupMetadata::DataType::DataOnFaces;
        mScalarDatasetValues = QgsMeshLayerUtils::resampleFromVerticesToFaces(
                                 mScalarDatasetValues,
                                 &mNativeMesh,
                                 &mTriangularMesh,
                                 &mScalarActiveFaceFlagValues,
                                 method
                               );
      }

    }

    const QgsMeshDatasetMetadata datasetMetadata = layer->dataProvider()->datasetMetadata( datasetIndex );
    mScalarDatasetMinimum = datasetMetadata.minimum();
    mScalarDatasetMaximum = datasetMetadata.maximum();
  }

  // update cache
  cache->mDatasetGroupsCount = datasetGroupCount;
  cache->mActiveScalarDatasetIndex = datasetIndex;
  cache->mDataInterpolationMethod = method;
  cache->mScalarDatasetValues = mScalarDatasetValues;
  cache->mScalarActiveFaceFlagValues = mScalarActiveFaceFlagValues;
  cache->mScalarDataType = mScalarDataType;
  cache->mScalarDatasetMinimum = mScalarDatasetMinimum;
  cache->mScalarDatasetMaximum = mScalarDatasetMaximum;
  cache->mScalarAveragingMethod.reset( mRendererSettings.averagingMethod() ? mRendererSettings.averagingMethod()->clone() : nullptr );
}


void QgsMeshLayerRenderer::copyVectorDatasetValues( QgsMeshLayer *layer )
{
  QgsMeshDatasetIndex datasetIndex;
  if ( renderContext()->isTemporal() )
    datasetIndex = layer->activeVectorDatasetAtTime( renderContext()->temporalRange() );
  else
    datasetIndex = layer->staticVectorDatasetIndex();

  // Find out if we can use cache up to date. If yes, use it and return
  const int datasetGroupCount = layer->dataProvider()->datasetGroupCount();
  QgsMeshLayerRendererCache *cache = layer->rendererCache();
  if ( ( cache->mDatasetGroupsCount == datasetGroupCount ) &&
       ( cache->mActiveVectorDatasetIndex == datasetIndex ) &&
       ( QgsMesh3dAveragingMethod::equals( cache->mVectorAveragingMethod.get(), mRendererSettings.averagingMethod() ) )
     )
  {
    mVectorDatasetValues = cache->mVectorDatasetValues;
    mVectorDatasetValuesMag = cache->mVectorDatasetValuesMag;
    mVectorDatasetMagMinimum = cache->mVectorDatasetMagMinimum;
    mVectorDatasetMagMaximum = cache->mVectorDatasetMagMaximum;
    mVectorDatasetGroupMagMinimum = cache->mVectorDatasetMagMinimum;
    mVectorDatasetGroupMagMaximum = cache->mVectorDatasetMagMaximum;
    mVectorDataType = cache->mVectorDataType;
    return;
  }

  // Cache is not up-to-date, gather data
  if ( datasetIndex.isValid() )
  {
    const QgsMeshDatasetGroupMetadata metadata = layer->dataProvider()->datasetGroupMetadata( datasetIndex );

    bool isScalar = metadata.isScalar();
    if ( isScalar )
    {
      QgsDebugMsg( QStringLiteral( "Dataset has no vector values" ) );
    }
    else
    {
      mVectorDataType = QgsMeshLayerUtils::datasetValuesType( metadata.dataType() );

      mVectorDatasetGroupMagMinimum = metadata.minimum();
      mVectorDatasetGroupMagMaximum = metadata.maximum();

      int count = QgsMeshLayerUtils::datasetValuesCount( &mNativeMesh, mVectorDataType );
      mVectorDatasetValues = QgsMeshLayerUtils::datasetValues(
                               layer,
                               datasetIndex,
                               0,
                               count );

      if ( mVectorDatasetValues.isValid() )
        mVectorDatasetValuesMag = QgsMeshLayerUtils::calculateMagnitudes( mVectorDatasetValues );
      else
        mVectorDatasetValuesMag = QVector<double>( count, std::numeric_limits<double>::quiet_NaN() );

      const QgsMeshDatasetMetadata datasetMetadata = layer->dataProvider()->datasetMetadata( datasetIndex );
      mVectorDatasetMagMinimum = datasetMetadata.minimum();
      mVectorDatasetMagMaximum = datasetMetadata.maximum();
    }
  }

  // update cache
  cache->mDatasetGroupsCount = datasetGroupCount;
  cache->mActiveVectorDatasetIndex = datasetIndex;
  cache->mVectorDatasetValues = mVectorDatasetValues;
  cache->mVectorDatasetValuesMag = mVectorDatasetValuesMag;
  cache->mVectorDatasetMagMinimum = mVectorDatasetMagMinimum;
  cache->mVectorDatasetMagMaximum = mVectorDatasetMagMaximum;
  cache->mVectorDatasetGroupMagMinimum = mVectorDatasetMagMinimum;
  cache->mVectorDatasetGroupMagMaximum = mVectorDatasetMagMaximum;
  cache->mVectorDataType = mVectorDataType;
  cache->mVectorAveragingMethod.reset( mRendererSettings.averagingMethod() ? mRendererSettings.averagingMethod()->clone() : nullptr );
}

bool QgsMeshLayerRenderer::render()
{
  renderScalarDataset();
  renderMesh();
  renderVectorDataset();
  return true;
}

void QgsMeshLayerRenderer::renderMesh()
{
  if ( !mRendererSettings.nativeMeshSettings().isEnabled() &&
       !mRendererSettings.edgeMeshSettings().isEnabled() &&
       !mRendererSettings.triangularMeshSettings().isEnabled() )
    return;

  // triangular mesh
  const QList<int> trianglesInExtent = mTriangularMesh.faceIndexesForRectangle( renderContext()->mapExtent() );
  if ( mRendererSettings.triangularMeshSettings().isEnabled() )
  {
    renderFaceMesh(
      mRendererSettings.triangularMeshSettings(),
      mTriangularMesh.triangles(),
      trianglesInExtent );
  }

  // native mesh
  if ( mRendererSettings.nativeMeshSettings().isEnabled() && mTriangularMesh.levelOfDetail() == 0 )
  {
    const QSet<int> nativeFacesInExtent = QgsMeshUtils::nativeFacesFromTriangles( trianglesInExtent,
                                          mTriangularMesh.trianglesToNativeFaces() );

    renderFaceMesh(
      mRendererSettings.nativeMeshSettings(),
      mNativeMesh.faces,
      nativeFacesInExtent.values() );
  }

  // edge mesh
  if ( mRendererSettings.edgeMeshSettings().isEnabled() )
  {
    const QList<int> edgesInExtent = mTriangularMesh.edgeIndexesForRectangle( renderContext()->mapExtent() );
    renderEdgeMesh( mRendererSettings.edgeMeshSettings(), edgesInExtent );
  }
}

static QPainter *_painterForMeshFrame( QgsRenderContext &context, const QgsMeshRendererMeshSettings &settings )
{
  // Set up the render configuration options
  QPainter *painter = context.painter();
  painter->save();
  if ( context.flags() & QgsRenderContext::Antialiasing )
    painter->setRenderHint( QPainter::Antialiasing, true );

  QPen pen = painter->pen();
  pen.setCapStyle( Qt::FlatCap );
  pen.setJoinStyle( Qt::MiterJoin );

  double penWidth = context.convertToPainterUnits( settings.lineWidth(),
                    QgsUnitTypes::RenderUnit::RenderMillimeters );
  pen.setWidthF( penWidth );
  pen.setColor( settings.color() );
  painter->setPen( pen );
  return painter;
}

void QgsMeshLayerRenderer::renderEdgeMesh( const QgsMeshRendererMeshSettings &settings, const QList<int> &edgesInExtent )
{
  Q_ASSERT( settings.isEnabled() );

  if ( !mTriangularMesh.contains( QgsMesh::ElementType::Edge ) )
    return;

  QgsRenderContext &context = *renderContext();
  QPainter *painter = _painterForMeshFrame( context, settings );

  const QVector<QgsMeshEdge> edges = mTriangularMesh.edges();
  const QVector<QgsMeshVertex> vertices = mTriangularMesh.vertices();

  for ( const int i : edgesInExtent )
  {
    if ( context.renderingStopped() )
      break;

    if ( i >= edges.size() )
      continue;

    const QgsMeshEdge &edge = edges[i];
    const int startVertexIndex = edge.first;
    const int endVertexIndex = edge.second;

    if ( ( startVertexIndex >= vertices.size() ) || endVertexIndex >= vertices.size() )
      continue;

    const QgsMeshVertex &startVertex = vertices[startVertexIndex];
    const QgsMeshVertex &endVertex = vertices[endVertexIndex];
    const QgsPointXY lineStart = context.mapToPixel().transform( startVertex.x(), startVertex.y() );
    const QgsPointXY lineEnd = context.mapToPixel().transform( endVertex.x(), endVertex.y() );
    painter->drawLine( lineStart.toQPointF(), lineEnd.toQPointF() );
  }
  painter->restore();
};

void QgsMeshLayerRenderer::renderFaceMesh(
  const QgsMeshRendererMeshSettings &settings,
  const QVector<QgsMeshFace> &faces,
  const QList<int> &facesInExtent )
{
  Q_ASSERT( settings.isEnabled() );

  if ( !mTriangularMesh.contains( QgsMesh::ElementType::Face ) )
    return;

  QgsRenderContext &context = *renderContext();
  QPainter *painter = _painterForMeshFrame( context, settings );

  const QVector<QgsMeshVertex> &vertices = mTriangularMesh.vertices(); //Triangular mesh vertices contains also native mesh vertices
  QSet<QPair<int, int>> drawnEdges;

  for ( const int i : facesInExtent )
  {
    if ( context.renderingStopped() )
      break;

    const QgsMeshFace &face = faces[i];
    if ( face.size() < 2 )
      continue;

    for ( int j = 0; j < face.size(); ++j )
    {
      const int startVertexId = face[j];
      const int endVertexId = face[( j + 1 ) % face.size()];
      const QPair<int, int> thisEdge( startVertexId, endVertexId );
      const QPair<int, int> thisEdgeReversed( endVertexId, startVertexId );
      if ( drawnEdges.contains( thisEdge ) || drawnEdges.contains( thisEdgeReversed ) )
        continue;
      drawnEdges.insert( thisEdge );
      drawnEdges.insert( thisEdgeReversed );

      const QgsMeshVertex &startVertex = vertices[startVertexId];
      const QgsMeshVertex &endVertex = vertices[endVertexId];
      const QgsPointXY lineStart = context.mapToPixel().transform( startVertex.x(), startVertex.y() );
      const QgsPointXY lineEnd = context.mapToPixel().transform( endVertex.x(), endVertex.y() );
      painter->drawLine( lineStart.toQPointF(), lineEnd.toQPointF() );
    }
  }

  painter->restore();
}

void QgsMeshLayerRenderer::renderScalarDataset()
{
  if ( mScalarDatasetValues.isEmpty() )
    return; // activeScalarDataset == NO_ACTIVE_MESH_DATASET

  if ( std::isnan( mScalarDatasetMinimum ) || std::isnan( mScalarDatasetMaximum ) )
    return; // only NODATA values

  int groupIndex = mRendererSettings.activeScalarDatasetGroup();
  if ( groupIndex < 0 )
    return; // no shader

  const QgsMeshRendererScalarSettings scalarSettings = mRendererSettings.scalarSettings( groupIndex );

  if ( ( mTriangularMesh.contains( QgsMesh::ElementType::Face ) ) &&
       ( mScalarDataType != QgsMeshDatasetGroupMetadata::DataType::DataOnEdges ) )
  {
    renderScalarDatasetOnFaces( scalarSettings );
  }

  if ( ( mTriangularMesh.contains( QgsMesh::ElementType::Edge ) ) &&
       ( mScalarDataType != QgsMeshDatasetGroupMetadata::DataType::DataOnFaces ) )
  {
    renderScalarDatasetOnEdges( scalarSettings );
  }
}

void QgsMeshLayerRenderer::renderScalarDatasetOnEdges( const QgsMeshRendererScalarSettings &scalarSettings )
{
  QgsRenderContext &context = *renderContext();
  QPainter *painter = context.painter();
  painter->save();
  if ( context.flags() & QgsRenderContext::Antialiasing )
    painter->setRenderHint( QPainter::Antialiasing, true );

  QPen pen = painter->pen();
  pen.setCapStyle( Qt::PenCapStyle::RoundCap );
  pen.setJoinStyle( Qt::MiterJoin );

  double lineWidth = scalarSettings.edgeWidth();
  double penWidth = context.convertToPainterUnits( lineWidth,
                    scalarSettings.edgeWidthUnit() );
  pen.setWidthF( penWidth );
  painter->setPen( pen );

  const QVector<QgsMeshEdge> edges = mTriangularMesh.edges();
  const QVector<QgsMeshVertex> vertices = mTriangularMesh.vertices();
  const QList<int> egdesInExtent = mTriangularMesh.edgeIndexesForRectangle( context.mapExtent() );
  const QSet<int> nativeEdgesInExtent = QgsMeshUtils::nativeEdgesFromEdges( egdesInExtent,
                                        mTriangularMesh.edgesToNativeEdges() );
  std::unique_ptr<QgsColorRampShader> shader( new QgsColorRampShader( scalarSettings.colorRampShader() ) );
  QList<QgsColorRampShader::ColorRampItem> colorRampItemList = shader->colorRampItemList();
  const QgsColorRampShader::Type classificationType = shader->colorRampType();

  for ( const int i : egdesInExtent )
  {
    if ( context.renderingStopped() )
      break;

    if ( i >= edges.size() )
      continue;

    const QgsMeshEdge &edge = edges[i];
    const int startVertexIndex = edge.first;
    const int endVertexIndex = edge.second;

    if ( ( startVertexIndex >= vertices.size() ) || endVertexIndex >= vertices.size() )
      continue;

    const QgsMeshVertex &startVertex = vertices[startVertexIndex];
    const QgsMeshVertex &endVertex = vertices[endVertexIndex];
    const QgsPointXY lineStart = context.mapToPixel().transform( startVertex.x(), startVertex.y() );
    const QgsPointXY lineEnd = context.mapToPixel().transform( endVertex.x(), endVertex.y() );

    if ( mScalarDataType == QgsMeshDatasetGroupMetadata::DataType::DataOnEdges )
    {
      QColor edgeColor = colorAt( shader.get(),  mScalarDatasetValues[i] );
      pen.setColor( edgeColor );

      if ( scalarSettings.isEdgeVaryingWidth() )
      {
        penWidth = scaleEdgeWidth( mScalarDatasetValues[i], scalarSettings );
      }
      pen.setWidthF( penWidth );
      painter->setPen( pen );
      painter->drawLine( lineStart.toQPointF(), lineEnd.toQPointF() );
    }
    else
    {
      double valVertexStart = mScalarDatasetValues[startVertexIndex];
      double valVertexEnd = mScalarDatasetValues[endVertexIndex];
      if ( std::isnan( valVertexStart ) || std::isnan( valVertexEnd ) )
        continue;
      double valDiff = ( valVertexEnd - valVertexStart );

      if ( scalarSettings.isEdgeVaryingWidth() )
      {
        penWidth = scaleEdgeWidth( ( valVertexStart + valVertexEnd ) / 2, scalarSettings );
      }
      pen.setWidthF( penWidth );

      if ( qgsDoubleNear( valDiff, 0.0 ) )
      {
        QColor edgeColor = colorAt( shader.get(),  valVertexStart );
        pen.setColor( edgeColor );
        painter->setPen( pen );
        painter->drawLine( lineStart.toQPointF(), lineEnd.toQPointF() );
      }
      else
      {
        if ( classificationType == QgsColorRampShader::Type::Exact )
        {
          Q_ASSERT( ! qgsDoubleNear( valDiff, 0.0 ) );
          for ( int i = 0; i < colorRampItemList.size(); ++i )
          {
            const QgsColorRampShader::ColorRampItem &item = colorRampItemList.at( i );
            if ( !std::isnan( item.value ) )
            {
              double fraction = ( item.value - valVertexStart ) / valDiff;
              if ( ( fraction > 0.0 ) && ( fraction < 1.0 ) )
              {
                QgsPointXY point = fractionPoint( lineStart, lineEnd, fraction );
                pen.setColor( item.color );
                painter->setPen( pen );
                painter->drawPoint( point.toQPointF() );
              }
            }
          }
        }
        else if ( classificationType == QgsColorRampShader::Type::Discrete )
        {
          QgsPointXY startPoint = lineStart;
          QColor color = colorAt( shader.get(), valVertexStart );
          Q_ASSERT( ! qgsDoubleNear( valDiff, 0.0 ) );
          for ( int i = 0; i < colorRampItemList.size() - 1; ++i )
          {
            const QgsColorRampShader::ColorRampItem &item = colorRampItemList.at( i );
            if ( !std::isnan( item.value ) )
            {
              double fraction = ( item.value - valVertexStart ) / valDiff;
              if ( ( fraction > 0.0 ) && ( fraction < 1.0 ) )
              {
                QgsPointXY endPoint = fractionPoint( lineStart, lineEnd, fraction );
                pen.setColor( color );
                painter->setPen( pen );
                painter->drawLine( startPoint.toQPointF(), endPoint.toQPointF() );
                color = item.color;
                startPoint = endPoint;
              }
            }
          }
          pen.setColor( colorAt( shader.get(), valVertexEnd ) );
          painter->setPen( pen );
          painter->drawLine( startPoint.toQPointF(), lineEnd.toQPointF() );
        }
        else if ( classificationType == QgsColorRampShader::Type::Interpolated )
        {
          QLinearGradient gradient( lineStart.toQPointF(), lineEnd.toQPointF() );
          gradient.setColorAt( 0.0, colorAt( shader.get(), valVertexStart ) );
          gradient.setColorAt( 1.0, colorAt( shader.get(), valVertexEnd ) );
          gradient.setSpread( gradient.ReflectSpread );

          Q_ASSERT( ! qgsDoubleNear( valDiff, 0.0 ) );

          for ( int i = 0; i < colorRampItemList.size(); ++i )
          {
            const QgsColorRampShader::ColorRampItem &item = colorRampItemList.at( i );
            if ( !std::isnan( item.value ) )
            {
              double fraction = ( item.value - valVertexStart ) / valDiff;
              if ( ( fraction > 0.0 ) && ( fraction < 1.0 ) )
              {
                gradient.setColorAt( fraction, colorAt( shader.get(), item.value ) );
              }
            }
            QBrush brush( gradient );
            pen.setBrush( brush );
            painter->setPen( pen );
            painter->drawLine( lineStart.toQPointF(), lineEnd.toQPointF() );
          }
        }
        else
        {
          // no other option possible
          Q_ASSERT( false );
        }
      }
    }
  }
  painter->restore();
}

QColor QgsMeshLayerRenderer::colorAt( QgsColorRampShader *shader, double val ) const
{
  int r, g, b, a;
  if ( shader->shade( val, &r, &g, &b, &a ) )
  {
    return QColor( r, g, b, a );
  }
  return QColor();
}

double QgsMeshLayerRenderer::scaleEdgeWidth( double scalarValue, const QgsMeshRendererScalarSettings &scalarSettings )
{
  double width = scalarSettings.edgeMinimumWidth() + ( scalarSettings.edgeWidth() - scalarSettings.edgeMinimumWidth() ) *
                 ( scalarValue - scalarSettings.classificationMinimum() ) /
                 ( scalarSettings.classificationMaximum() - scalarSettings.classificationMinimum() );
  if ( width < scalarSettings.edgeMinimumWidth() )
    width = scalarSettings.edgeMinimumWidth();
  if ( width > scalarSettings.edgeWidth() )
    width = scalarSettings.edgeWidth();

  return renderContext()->convertToPainterUnits( width, scalarSettings.edgeWidthUnit() );
}

QgsPointXY QgsMeshLayerRenderer::fractionPoint( const QgsPointXY &p1, const QgsPointXY &p2, double fraction ) const
{
  const QgsPointXY pt( p1.x() + fraction * ( p2.x() - p1.x() ),
                       p1.y() + fraction * ( p2.y() - p1.y() ) );
  return pt;
}

void QgsMeshLayerRenderer::renderScalarDatasetOnFaces( const QgsMeshRendererScalarSettings &scalarSettings )
{
  QgsRenderContext &context = *renderContext();

  QgsColorRampShader *fcn = new QgsColorRampShader( scalarSettings.colorRampShader() );
  QgsRasterShader *sh = new QgsRasterShader();
  sh->setRasterShaderFunction( fcn );  // takes ownership of fcn
  QgsMeshLayerInterpolator interpolator( mTriangularMesh,
                                         mScalarDatasetValues,
                                         mScalarActiveFaceFlagValues,
                                         mScalarDataType,
                                         context,
                                         mOutputSize );
  interpolator.setSpatialIndexActive( mIsMeshSimplificationActive );
  QgsSingleBandPseudoColorRenderer renderer( &interpolator, 0, sh );  // takes ownership of sh
  renderer.setClassificationMin( scalarSettings.classificationMinimum() );
  renderer.setClassificationMax( scalarSettings.classificationMaximum() );
  renderer.setOpacity( scalarSettings.opacity() );

  std::unique_ptr<QgsRasterBlock> bl( renderer.block( 0, context.mapExtent(), mOutputSize.width(), mOutputSize.height(), mFeedback.get() ) );
  QImage img = bl->image();

  context.painter()->drawImage( 0, 0, img );
}

void QgsMeshLayerRenderer::renderVectorDataset()
{
  int groupIndex = mRendererSettings.activeVectorDatasetGroup();
  if ( groupIndex < 0 )
    return;

  if ( !mVectorDatasetValues.isValid() )
    return; // no data at all

  if ( std::isnan( mVectorDatasetMagMinimum ) || std::isnan( mVectorDatasetMagMaximum ) )
    return; // only NODATA values

  std::unique_ptr<QgsMeshVectorRenderer> renderer( QgsMeshVectorRenderer::makeVectorRenderer(
        mTriangularMesh,
        mVectorDatasetValues,
        mScalarActiveFaceFlagValues,
        mVectorDatasetValuesMag,
        mVectorDatasetMagMaximum,
        mVectorDatasetMagMinimum,
        mVectorDataType,
        mRendererSettings.vectorSettings( groupIndex ),
        *renderContext(),
        mLayerExtent,
        mOutputSize ) );

  if ( renderer )
    renderer->draw();
}

double QgsMeshStrokeWidthVarying::minimumValue() const
{
  return mMinimumValue;
}

void QgsMeshStrokeWidthVarying::setMinimumValue( double minimumValue )
{
  mMinimumValue = minimumValue;
}

double QgsMeshStrokeWidthVarying::maximumValue() const
{
  return mMaximumValue;
}

void QgsMeshStrokeWidthVarying::setMaximumValue( double maximumValue )
{
  mMaximumValue = maximumValue;
}

double QgsMeshStrokeWidthVarying::minimumWidth() const
{
  return mMinimumWidth;
}

void QgsMeshStrokeWidthVarying::setMinimumWidth( double minimumWidth )
{
  mMinimumWidth = minimumWidth;
}

double QgsMeshStrokeWidthVarying::maximumWidth() const
{
  return mMaximumWidth;
}

void QgsMeshStrokeWidthVarying::setMaximumWidth( double maximumWidth )
{
  mMaximumWidth = maximumWidth;
}

bool QgsMeshStrokeWidthVarying::ignoreOutOfRange() const
{
  return mIgnoreOutOfRange;
}

void QgsMeshStrokeWidthVarying::setIgnoreOutOfRange( bool ignoreOutOfRange )
{
  mIgnoreOutOfRange = ignoreOutOfRange;
}
