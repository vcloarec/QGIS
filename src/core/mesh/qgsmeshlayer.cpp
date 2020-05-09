/***************************************************************************
                         qgsmeshlayer.cpp
                         ----------------
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

#include <cstddef>
#include <limits>

#include <QUuid>

#include "qgscolorramp.h"
#include "qgslogger.h"
#include "qgsmaplayerlegend.h"
#include "qgsmeshdataprovider.h"
#include "qgsmeshlayer.h"
#include "qgsmeshlayerrenderer.h"
#include "qgsmeshlayertemporalproperties.h"
#include "qgsmeshlayerutils.h"
#include "qgsmeshtimesettings.h"
#include "qgspainting.h"
#include "qgsproviderregistry.h"
#include "qgsreadwritecontext.h"
#include "qgsstyle.h"
#include "qgstriangularmesh.h"
#include "qgsmesh3daveraging.h"

QgsMeshLayer::QgsMeshLayer( const QString &meshLayerPath,
                            const QString &baseName,
                            const QString &providerKey,
                            const QgsMeshLayer::LayerOptions &options )
  : QgsMapLayer( QgsMapLayerType::MeshLayer, baseName, meshLayerPath ),
    mTemporalProperties( new QgsMeshLayerTemporalProperties( this ) )
{
  mShouldValidateCrs = !options.skipCrsValidation;

  setProviderType( providerKey );
  // if we’re given a provider type, try to create and bind one to this layer
  bool ok = false;
  if ( !meshLayerPath.isEmpty() && !providerKey.isEmpty() )
  {
    QgsDataProvider::ProviderOptions providerOptions { options.transformContext };
    ok = setDataProvider( providerKey, providerOptions );
  }

  if ( ok )
  {
    setLegend( QgsMapLayerLegend::defaultMeshLegend( this ) );
    setDefaultRendererSettings();

    if ( mDataProvider )
      mTemporalProperties->setDefaultsFromDataProviderTemporalCapabilities( mDataProvider->temporalCapabilities() );
  }
}


void QgsMeshLayer::setDefaultRendererSettings()
{
  QgsMeshRendererMeshSettings meshSettings;
  if ( mDataProvider && mDataProvider->datasetGroupCount() > 0 )
  {
    // Show data from the first dataset group
    mRendererSettings.setActiveScalarDatasetGroup( 0 );
    // If the first dataset group has nan min/max, display the mesh to avoid nothing displayed
    QgsMeshDatasetGroupMetadata meta = mDataProvider->datasetGroupMetadata( 0 );
    if ( meta.maximum() == std::numeric_limits<double>::quiet_NaN() &&
         meta.minimum() == std::numeric_limits<double>::quiet_NaN() )
      meshSettings.setEnabled( true );

    // If the mesh is non temporal, set the static scalar dataset
    if ( !mDataProvider->temporalCapabilities()->hasTemporalCapabilities() )
      setStaticScalarDatasetIndex( QgsMeshDatasetIndex( 0, 0 ) );
  }
  else
  {
    // show at least the mesh by default
    meshSettings.setEnabled( true );
  }
  mRendererSettings.setNativeMeshSettings( meshSettings );

  // Sets default resample method for scalar dataset
  if ( !mDataProvider )
    return;
  for ( int i = 0; i < mDataProvider->datasetGroupCount(); ++i )
  {
    QgsMeshDatasetGroupMetadata meta = mDataProvider->datasetGroupMetadata( i );
    QgsMeshRendererScalarSettings scalarSettings = mRendererSettings.scalarSettings( i );
    switch ( meta.dataType() )
    {
      case QgsMeshDatasetGroupMetadata::DataOnFaces:
      case QgsMeshDatasetGroupMetadata::DataOnVolumes: // data on volumes are averaged to 2D data on faces
        scalarSettings.setDataResamplingMethod( QgsMeshRendererScalarSettings::NeighbourAverage );
        break;
      case QgsMeshDatasetGroupMetadata::DataOnVertices:
        scalarSettings.setDataResamplingMethod( QgsMeshRendererScalarSettings::None );
        break;
      case QgsMeshDatasetGroupMetadata::DataOnEdges:
        break;
    }
    mRendererSettings.setScalarSettings( i, scalarSettings );
  }

}

void QgsMeshLayer::createSimplifiedMeshes()
{
  if ( mSimplificationSettings.isEnabled() && !hasSimplifiedMeshes() )
  {
    double reductionFactor = mSimplificationSettings.reductionFactor();

    QVector<QgsTriangularMesh *> simplifyMeshes =
      mTriangularMeshes[0]->simplifyMesh( reductionFactor );

    for ( int i = 0; i < simplifyMeshes.count() ; ++i )
    {
      mTriangularMeshes.emplace_back( simplifyMeshes[i] );
    }
  }
}

bool QgsMeshLayer::hasSimplifiedMeshes() const
{
  //First mesh is the base mesh, so if size>1, there is no simplified meshes
  return ( mTriangularMeshes.size() > 1 );
}

QgsMeshLayer::~QgsMeshLayer()
{
  delete mDataProvider;
}

QgsMeshDataProvider *QgsMeshLayer::dataProvider()
{
  return mDataProvider;
}

const QgsMeshDataProvider *QgsMeshLayer::dataProvider() const
{
  return mDataProvider;
}

QgsMeshLayer *QgsMeshLayer::clone() const
{
  QgsMeshLayer::LayerOptions options;
  if ( mDataProvider )
  {
    options.transformContext = mDataProvider->transformContext();
  }
  QgsMeshLayer *layer = new QgsMeshLayer( source(), name(), mProviderKey,  options );
  QgsMapLayer::clone( layer );
  return layer;
}

QgsRectangle QgsMeshLayer::extent() const
{
  if ( mDataProvider )
    return mDataProvider->extent();
  else
  {
    QgsRectangle rec;
    rec.setMinimal();
    return rec;
  }
}

QString QgsMeshLayer::providerType() const
{
  return mProviderKey;
}

QgsMesh *QgsMeshLayer::nativeMesh()
{
  return mNativeMesh.get();
}

const QgsMesh *QgsMeshLayer::nativeMesh() const
{
  return mNativeMesh.get();
}

QgsTriangularMesh *QgsMeshLayer::triangularMesh( double minimumTriangleSize ) const
{
  for ( const std::unique_ptr<QgsTriangularMesh> &lod : mTriangularMeshes )
  {
    if ( lod && lod->averageTriangleSize() > minimumTriangleSize )
      return lod.get();
  }

  if ( !mTriangularMeshes.empty() )
    return mTriangularMeshes.back().get();
  else
    return nullptr;
}

void  QgsMeshLayer::updateTriangularMesh( const QgsCoordinateTransform &transform )
{
  // Native mesh
  if ( !mNativeMesh )
  {
    // lazy loading of mesh data
    fillNativeMesh();
  }

  // Triangular mesh
  if ( mTriangularMeshes.empty() )
  {
    QgsTriangularMesh *baseMesh = new QgsTriangularMesh;
    mTriangularMeshes.emplace_back( baseMesh );
  }

  if ( mTriangularMeshes[0].get()->update( mNativeMesh.get(), transform ) )
    mTriangularMeshes.resize( 1 ); //if the base triangular mesh is effectivly updated, remove simplified meshes
}

QgsMeshLayerRendererCache *QgsMeshLayer::rendererCache()
{
  return mRendererCache.get();
}

QgsMeshRendererSettings QgsMeshLayer::rendererSettings() const
{
  return mRendererSettings;
}

void QgsMeshLayer::setRendererSettings( const QgsMeshRendererSettings &settings )
{
  mRendererSettings = settings;
  emit rendererChanged();
  triggerRepaint();
}

QgsMeshTimeSettings QgsMeshLayer::timeSettings() const
{
  return mTimeSettings;
}

void QgsMeshLayer::setTimeSettings( const QgsMeshTimeSettings &settings )
{
  mTimeSettings = settings;
  emit timeSettingsChanged();
}

QString QgsMeshLayer::formatTime( double hours )
{
  if ( dataProvider() && dataProvider()->temporalCapabilities()->hasReferenceTime() )
    return QgsMeshLayerUtils::formatTime( hours, temporalProperties()->referenceTime(), mTimeSettings );
  else
    return QgsMeshLayerUtils::formatTime( hours, QDateTime(), mTimeSettings );
}

QgsMeshDatasetValue QgsMeshLayer::datasetValue( const QgsMeshDatasetIndex &index, const QgsPointXY &point, double searchRadius ) const
{
  QgsMeshDatasetValue value;
  const QgsTriangularMesh *mesh = triangularMesh();

  if ( mesh && dataProvider() && dataProvider()->isValid() && index.isValid() )
  {
    if ( dataProvider()->contains( QgsMesh::ElementType::Edge ) )
    {
      QgsRectangle searchRectangle( point.x() - searchRadius, point.y() - searchRadius, point.x() + searchRadius, point.y() + searchRadius );
      return dataset1dValue( index, point, searchRadius );
    }
    int faceIndex = mesh->faceIndexForPoint( point ) ;
    if ( faceIndex >= 0 )
    {
      int nativeFaceIndex = mesh->trianglesToNativeFaces().at( faceIndex );
      const QgsMeshDatasetGroupMetadata::DataType dataType = dataProvider()->datasetGroupMetadata( index ).dataType();
      if ( dataProvider()->isFaceActive( index, nativeFaceIndex ) )
      {
        switch ( dataType )
        {
          case QgsMeshDatasetGroupMetadata::DataOnFaces:
          {
            value = dataProvider()->datasetValue( index, nativeFaceIndex );
          }
          break;

          case QgsMeshDatasetGroupMetadata::DataOnVertices:
          {
            const QgsMeshFace &face = mesh->triangles()[faceIndex];
            const int v1 = face[0], v2 = face[1], v3 = face[2];
            const QgsPoint p1 = mesh->vertices()[v1], p2 = mesh->vertices()[v2], p3 = mesh->vertices()[v3];
            const QgsMeshDatasetValue val1 = dataProvider()->datasetValue( index, v1 );
            const QgsMeshDatasetValue val2 = dataProvider()->datasetValue( index, v2 );
            const QgsMeshDatasetValue val3 = dataProvider()->datasetValue( index, v3 );
            const double x = QgsMeshLayerUtils::interpolateFromVerticesData( p1, p2, p3, val1.x(), val2.x(), val3.x(), point );
            double y = std::numeric_limits<double>::quiet_NaN();
            bool isVector = dataProvider()->datasetGroupMetadata( index ).isVector();
            if ( isVector )
              y = QgsMeshLayerUtils::interpolateFromVerticesData( p1, p2, p3, val1.y(), val2.y(), val3.y(), point );

            value = QgsMeshDatasetValue( x, y );
          }
          break;

          case QgsMeshDatasetGroupMetadata::DataOnVolumes:
          {
            const QgsMesh3dAveragingMethod *avgMethod = mRendererSettings.averagingMethod();
            if ( avgMethod )
            {
              const QgsMesh3dDataBlock block3d = dataProvider()->dataset3dValues( index, nativeFaceIndex, 1 );
              const QgsMeshDataBlock block2d = avgMethod->calculate( block3d );
              if ( block2d.isValid() )
              {
                value = block2d.value( 0 );
              }
            }
          }
          break;

          default:
            break;
        }
      }
    }
  }

  return value;
}

QgsMesh3dDataBlock QgsMeshLayer::dataset3dValue( const QgsMeshDatasetIndex &index, const QgsPointXY &point ) const
{
  QgsMesh3dDataBlock block3d;

  const QgsTriangularMesh *baseTriangularMesh = triangularMesh();

  if ( baseTriangularMesh && dataProvider() && dataProvider()->isValid() && index.isValid() )
  {
    const QgsMeshDatasetGroupMetadata::DataType dataType = dataProvider()->datasetGroupMetadata( index ).dataType();
    if ( dataType == QgsMeshDatasetGroupMetadata::DataOnVolumes )
    {
      int faceIndex = baseTriangularMesh->faceIndexForPoint( point ) ;
      if ( faceIndex >= 0 )
      {
        int nativeFaceIndex = baseTriangularMesh->trianglesToNativeFaces().at( faceIndex );
        block3d = dataProvider()->dataset3dValues( index, nativeFaceIndex, 1 );
      }
    }
  }
  return block3d;
}

QgsMeshDatasetValue QgsMeshLayer::dataset1dValue( const QgsMeshDatasetIndex &index, const QgsPointXY &point, double searchRadius ) const
{
  QgsMeshDatasetValue value;
  QgsPointXY projectedPoint;
  int selectedIndex = closestEdge( point, searchRadius, projectedPoint );
  const QgsTriangularMesh *mesh = triangularMesh();
  if ( selectedIndex >= 0 )
  {
    const QgsMeshDatasetGroupMetadata::DataType dataType = dataProvider()->datasetGroupMetadata( index ).dataType();
    switch ( dataType )
    {
      case QgsMeshDatasetGroupMetadata::DataOnEdges:
      {
        value = dataProvider()->datasetValue( index, selectedIndex );
      }
      break;

      case QgsMeshDatasetGroupMetadata::DataOnVertices:
      {
        const QgsMeshEdge &edge = mesh->edges()[selectedIndex];
        const int v1 = edge.first, v2 = edge.second;
        const QgsPoint p1 = mesh->vertices()[v1], p2 = mesh->vertices()[v2];
        const QgsMeshDatasetValue val1 = dataProvider()->datasetValue( index, v1 );
        const QgsMeshDatasetValue val2 = dataProvider()->datasetValue( index, v2 );
        double edgeLength = p1.distance( p2 );
        double dist1 = p1.distance( projectedPoint.x(), projectedPoint.y() );
        value = QgsMeshLayerUtils::interpolateFromVerticesData( dist1 / edgeLength, val1, val2 );
      }
      break;
      default:
        break;
    }
  }


  return value;
}

void QgsMeshLayer::setTransformContext( const QgsCoordinateTransformContext &transformContext )
{
  if ( mDataProvider )
    mDataProvider->setTransformContext( transformContext );
}

QgsMeshDatasetIndex QgsMeshLayer::datasetIndexAtTime( const QgsDateTimeRange &timeRange, int datasetGroupIndex ) const
{
  const QDateTime layerReferenceTime = mTemporalProperties->referenceTime();
  qint64 startTime = layerReferenceTime.msecsTo( timeRange.begin() );
  qint64 endTime = layerReferenceTime.msecsTo( timeRange.end() );

  if ( dataProvider() )
    return dataProvider()->temporalCapabilities()->datasetIndexFromRelativeTimeRange( datasetGroupIndex, startTime, endTime );
  else
    return QgsMeshDatasetIndex();
}

QgsMeshDatasetIndex QgsMeshLayer::activeScalarDatasetAtTime( const QgsDateTimeRange &timeRange ) const
{
  if ( mTemporalProperties->isActive() )
    return datasetIndexAtTime( timeRange, mRendererSettings.activeScalarDatasetGroup() );
  else
    return mStaticScalarDatasetIndex;
}

QgsMeshDatasetIndex QgsMeshLayer::activeVectorDatasetAtTime( const QgsDateTimeRange &timeRange ) const
{
  if ( mTemporalProperties->isActive() )
    return datasetIndexAtTime( timeRange, mRendererSettings.activeVectorDatasetGroup() );
  else
    return mStaticVectorDatasetIndex;
}

void QgsMeshLayer::fillNativeMesh()
{
  Q_ASSERT( !mNativeMesh );

  mNativeMesh.reset( new QgsMesh() );

  if ( !( dataProvider() && dataProvider()->isValid() ) )
    return;

  dataProvider()->populateMesh( mNativeMesh.get() );
}

void QgsMeshLayer::onDatasetGroupsAdded( int count )
{
  updateDatasetGroupsState();
  // assign default style to new dataset groups
  int newDatasetGroupCount = mDataProvider->datasetGroupCount();
  for ( int i = newDatasetGroupCount - count; i < newDatasetGroupCount; ++i )
    assignDefaultStyleToDatasetGroup( i );

  if ( mDataProvider )
    temporalProperties()->setIsActive( mDataProvider->temporalCapabilities()->hasTemporalCapabilities() );
}

QMap<int, QgsMeshDatasetGroupState> QgsMeshLayer::datasetGroupsState() const
{
  return mDatasetGroupsState;
}

int QgsMeshLayer::closestEdge( const QgsPointXY &point, double searchRadius, QgsPointXY &projectedPoint ) const
{
  QgsRectangle searchRectangle( point.x() - searchRadius, point.y() - searchRadius, point.x() + searchRadius, point.y() + searchRadius );
  const QgsTriangularMesh *mesh = triangularMesh();
  // search for the closest edge in search area from point
  const QList<int> edgeIndexes = mesh->edgeIndexesForRectangle( searchRectangle );
  int selectedIndex = -1;
  if ( mesh &&
       mesh->contains( QgsMesh::Edge ) &&
       mDataProvider->isValid() )
  {
    double sqrMaxDistFromPoint = pow( searchRadius, 2 );
    for ( const int edgeIndex : edgeIndexes )
    {
      const QgsMeshEdge &edge = mesh->edges().at( edgeIndex );
      const QgsMeshVertex &vertex1 = mesh->vertices()[edge.first];
      const QgsMeshVertex &vertex2 = mesh->vertices()[edge.second];
      QgsPointXY projPoint;
      double sqrDist = point.sqrDistToSegment( vertex1.x(), vertex1.y(), vertex2.x(), vertex2.y(), projPoint );
      if ( sqrDist < sqrMaxDistFromPoint )
      {
        selectedIndex = edgeIndex;
        projectedPoint = projPoint;
        sqrMaxDistFromPoint = sqrDist;
      }
    }
  }

  return selectedIndex;
}

QgsMeshDatasetIndex QgsMeshLayer::staticVectorDatasetIndex() const
{
  return mStaticVectorDatasetIndex;
}

void QgsMeshLayer::setReferenceTime( const QDateTime &referenceTime )
{
  if ( dataProvider() )
    temporalProperties()->setReferenceTime( referenceTime, dataProvider()->temporalCapabilities() );
  else
    temporalProperties()->setReferenceTime( referenceTime, nullptr );
}

QgsPointXY QgsMeshLayer::snapOnVertex( const QgsPointXY &point, double searchRadius )
{
  const QgsTriangularMesh *mesh = triangularMesh();
  QgsPointXY exactPosition;
  if ( !mesh )
    return exactPosition;
  QgsRectangle rectangle( point.x() - searchRadius, point.y() - searchRadius, point.x() + searchRadius, point.y() + searchRadius );
  double maxDistance = searchRadius;
  //attempt to snap on edges's vertices
  QList<int> edgeIndexes = mesh->edgeIndexesForRectangle( rectangle );
  for ( const int edgeIndex : edgeIndexes )
  {
    const QgsMeshEdge &edge = mesh->edges().at( edgeIndex );
    const QgsMeshVertex &vertex1 = mesh->vertices()[edge.first];
    const QgsMeshVertex &vertex2 = mesh->vertices()[edge.second];
    double dist1 = point.distance( vertex1 );
    double dist2 = point.distance( vertex2 );
    if ( dist1 < maxDistance )
    {
      maxDistance = dist1;
      exactPosition = vertex1;
    }
    if ( dist2 < maxDistance )
    {
      maxDistance = dist2;
      exactPosition = vertex2;
    }
  }

  //attempt to snap on face's vertices
  QList<int> faceIndexes = mesh->faceIndexesForRectangle( rectangle );
  for ( const int faceIndex : faceIndexes )
  {
    const QgsMeshFace &face = mesh->triangles().at( faceIndex );
    for ( int i = 0; i < 3; ++i )
    {
      const QgsMeshVertex &vertex = mesh->vertices()[face.at( i )];
      double dist = point.distance( vertex );
      if ( dist < maxDistance )
      {
        maxDistance = dist;
        exactPosition = vertex;
      }
    }
  }

  return exactPosition;
}

QgsPointXY QgsMeshLayer::snapOnEdge( const QgsPointXY &point, double searchRadius )
{
  QgsPointXY projectedPoint;
  closestEdge( point, searchRadius, projectedPoint );

  return projectedPoint;
}

QgsPointXY QgsMeshLayer::snapOnFace( const QgsPointXY &point, double searchRadius )
{
  const QgsTriangularMesh *mesh = triangularMesh();
  QgsPointXY centroidPosition;
  if ( !mesh )
    return centroidPosition;
  QgsRectangle rectangle( point.x() - searchRadius, point.y() - searchRadius, point.x() + searchRadius, point.y() + searchRadius );
  double maxDistance = std::numeric_limits<double>::max();

  QList<int> faceIndexes = mesh->faceIndexesForRectangle( rectangle );
  for ( const int faceIndex : faceIndexes )
  {
    int nativefaceIndex = mesh->trianglesToNativeFaces().at( faceIndex );
    if ( nativefaceIndex < 0 && nativefaceIndex >= mesh->faceCentroids().count() )
      continue;
    const QgsPointXY centroid = mesh->faceCentroids()[nativefaceIndex];
    double dist = point.distance( centroid );
    if ( dist < maxDistance )
    {
      maxDistance = dist;
      centroidPosition = centroid;
    }
  }

  return centroidPosition;
}

void QgsMeshLayer::updateDatasetGroupsState()
{
  if ( !mDataProvider )
    return;

  int groupCount = mDataProvider->datasetGroupCount();

  for ( int i = 0; i < groupCount; ++i )
  {
    QgsMeshDatasetGroupMetadata meta = mDataProvider->datasetGroupMetadata( i );
    if ( !mDatasetGroupsState.contains( i ) )
    {
      // If not present, add a new state
      QgsMeshDatasetGroupState state;
      state.used = true;
      state.originalName = meta.name();

      mDatasetGroupsState[i] = state;
    }
    else
    {
      // If present, check if the original name is the same, if not, replace by default value of the group
      QgsMeshDatasetGroupState &state = mDatasetGroupsState[i];
      if ( !( state.originalName == meta.name() ) )
      {
        state.used = true;
        state.originalName = meta.name();
        state.renaming = QString();
      }
    }
  }
}

QgsPointXY QgsMeshLayer::snapOnElement( QgsMesh::ElementType elementType, const QgsPointXY &point, double searchRadius )
{
  switch ( elementType )
  {
    case QgsMesh::Vertex:
      return snapOnVertex( point, searchRadius );
    case QgsMesh::Edge:
      return snapOnEdge( point, searchRadius );
    case QgsMesh::Face:
      return snapOnFace( point, searchRadius );
  }
  return QgsPointXY(); // avoid warnings
}

QgsMeshDatasetIndex QgsMeshLayer::staticScalarDatasetIndex() const
{
  return mStaticScalarDatasetIndex;
}

void QgsMeshLayer::setStaticVectorDatasetIndex( const QgsMeshDatasetIndex &staticVectorDatasetIndex )
{
  mStaticVectorDatasetIndex = staticVectorDatasetIndex;
  if ( !temporalProperties()->isActive() )
    mRendererSettings.setActiveVectorDatasetGroup( staticVectorDatasetIndex.group() );
}

void QgsMeshLayer::setStaticScalarDatasetIndex( const QgsMeshDatasetIndex &staticScalarDatasetIndex )
{
  mStaticScalarDatasetIndex = staticScalarDatasetIndex;
  if ( !temporalProperties()->isActive() )
    mRendererSettings.setActiveScalarDatasetGroup( staticScalarDatasetIndex.group() );
}

QgsMeshSimplificationSettings QgsMeshLayer::meshSimplificationSettings() const
{
  return mSimplificationSettings;
}

void QgsMeshLayer::setMeshSimplificationSettings( const QgsMeshSimplificationSettings &simplifySettings )
{
  mSimplificationSettings = simplifySettings;
}

static QgsColorRamp *_createDefaultColorRamp()
{
  QgsColorRamp *ramp = QgsStyle::defaultStyle()->colorRamp( QStringLiteral( "Plasma" ) );
  if ( ramp )
    return ramp;

  // definition of "Plasma" color ramp (in case it is not available in the style for some reason)
  QgsStringMap props;
  props["color1"] = "13,8,135,255";
  props["color2"] = "240,249,33,255";
  props["stops"] =
    "0.0196078;27,6,141,255:0.0392157;38,5,145,255:0.0588235;47,5,150,255:0.0784314;56,4,154,255:0.0980392;65,4,157,255:"
    "0.117647;73,3,160,255:0.137255;81,2,163,255:0.156863;89,1,165,255:0.176471;97,0,167,255:0.196078;105,0,168,255:"
    "0.215686;113,0,168,255:0.235294;120,1,168,255:0.254902;128,4,168,255:0.27451;135,7,166,255:0.294118;142,12,164,255:"
    "0.313725;149,17,161,255:0.333333;156,23,158,255:0.352941;162,29,154,255:0.372549;168,34,150,255:0.392157;174,40,146,255:"
    "0.411765;180,46,141,255:0.431373;186,51,136,255:0.45098;191,57,132,255:0.470588;196,62,127,255:0.490196;201,68,122,255:"
    "0.509804;205,74,118,255:0.529412;210,79,113,255:0.54902;214,85,109,255:0.568627;218,91,105,255:0.588235;222,97,100,255:"
    "0.607843;226,102,96,255:0.627451;230,108,92,255:0.647059;233,114,87,255:0.666667;237,121,83,255:0.686275;240,127,79,255:"
    "0.705882;243,133,75,255:0.72549;245,140,70,255:0.745098;247,147,66,255:0.764706;249,154,62,255:0.784314;251,161,57,255:"
    "0.803922;252,168,53,255:0.823529;253,175,49,255:0.843137;254,183,45,255:0.862745;254,190,42,255:0.882353;253,198,39,255:"
    "0.901961;252,206,37,255:0.921569;251,215,36,255:0.941176;248,223,37,255:0.960784;246,232,38,255:0.980392;243,240,39,255";
  return QgsGradientColorRamp::create( props );
}

void QgsMeshLayer::assignDefaultStyleToDatasetGroup( int groupIndex )
{
  const QgsMeshDatasetGroupMetadata metadata = mDataProvider->datasetGroupMetadata( groupIndex );
  double groupMin = metadata.minimum();
  double groupMax = metadata.maximum();

  QgsColorRampShader fcn( groupMin, groupMax, _createDefaultColorRamp() );
  fcn.classifyColorRamp( 5, -1, QgsRectangle(), nullptr );

  QgsMeshRendererScalarSettings scalarSettings;
  scalarSettings.setClassificationMinimumMaximum( groupMin, groupMax );
  scalarSettings.setColorRampShader( fcn );
  QgsInterpolatedLineWidth edgeStrokeWidth;
  edgeStrokeWidth.setMinimumValue( groupMin );
  edgeStrokeWidth.setMaximumValue( groupMax );
  QgsInterpolatedLineColor edgeStrokeColor( fcn );
  QgsInterpolatedLineRenderer edgeStrokePen;
  scalarSettings.setEdgeStrokeWidth( edgeStrokeWidth );
  mRendererSettings.setScalarSettings( groupIndex, scalarSettings );

  if ( metadata.isVector() )
  {
    QgsMeshRendererVectorSettings vectorSettings;
    vectorSettings.setColorRampShader( fcn );
    mRendererSettings.setVectorSettings( groupIndex, vectorSettings );
  }
}

QgsMapLayerRenderer *QgsMeshLayer::createMapRenderer( QgsRenderContext &rendererContext )
{
  // Triangular mesh
  updateTriangularMesh( rendererContext.coordinateTransform() );

  // Build overview triangular meshes if needed
  createSimplifiedMeshes();

  // Cache
  if ( !mRendererCache )
    mRendererCache.reset( new QgsMeshLayerRendererCache() );

  return new QgsMeshLayerRenderer( this, rendererContext );
}

bool QgsMeshLayer::readSymbology( const QDomNode &node, QString &errorMessage,
                                  QgsReadWriteContext &context, QgsMapLayer::StyleCategories categories )
{
  Q_UNUSED( errorMessage )
  // TODO: implement categories for raster layer

  QDomElement elem = node.toElement();

  readCommonStyle( elem, context, categories );

  QDomElement elemRendererSettings = elem.firstChildElement( "mesh-renderer-settings" );
  if ( !elemRendererSettings.isNull() )
    mRendererSettings.readXml( elemRendererSettings );

  QDomElement elemSimplifySettings = elem.firstChildElement( "mesh-simplify-settings" );
  if ( !elemSimplifySettings.isNull() )
    mSimplificationSettings.readXml( elemSimplifySettings, context );

  // get and set the blend mode if it exists
  QDomNode blendModeNode = node.namedItem( QStringLiteral( "blendMode" ) );
  if ( !blendModeNode.isNull() )
  {
    QDomElement e = blendModeNode.toElement();
    setBlendMode( QgsPainting::getCompositionMode( static_cast< QgsPainting::BlendMode >( e.text().toInt() ) ) );
  }

  return true;
}

bool QgsMeshLayer::writeSymbology( QDomNode &node, QDomDocument &doc, QString &errorMessage,
                                   const QgsReadWriteContext &context, QgsMapLayer::StyleCategories categories ) const
{
  Q_UNUSED( errorMessage )
  // TODO: implement categories for raster layer

  QDomElement elem = node.toElement();

  writeCommonStyle( elem, doc, context, categories );

  QDomElement elemRendererSettings = mRendererSettings.writeXml( doc );
  elem.appendChild( elemRendererSettings );

  QDomElement elemSimplifySettings = mSimplificationSettings.writeXml( doc, context );
  elem.appendChild( elemSimplifySettings );

  // add blend mode node
  QDomElement blendModeElement  = doc.createElement( QStringLiteral( "blendMode" ) );
  QDomText blendModeText = doc.createTextNode( QString::number( QgsPainting::getBlendModeEnum( blendMode() ) ) );
  blendModeElement.appendChild( blendModeText );
  node.appendChild( blendModeElement );

  return true;
}

bool QgsMeshLayer::writeStyle( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsReadWriteContext &context, QgsMapLayer::StyleCategories categories ) const
{
  return writeSymbology( node, doc, errorMessage, context, categories );
}

bool QgsMeshLayer::readStyle( const QDomNode &node, QString &errorMessage, QgsReadWriteContext &context, QgsMapLayer::StyleCategories categories )
{
  return readSymbology( node, errorMessage, context, categories );
}

QString QgsMeshLayer::decodedSource( const QString &source, const QString &provider, const QgsReadWriteContext &context ) const
{
  QString src( source );
  if ( provider == QLatin1String( "mdal" ) )
  {
    src = context.pathResolver().readPath( src );
  }
  return src;
}

QString QgsMeshLayer::encodedSource( const QString &source, const QgsReadWriteContext &context ) const
{
  QString src( source );
  if ( providerType() == QLatin1String( "mdal" ) )
  {
    src = context.pathResolver().writePath( src );
  }
  return src;
}

bool QgsMeshLayer::readXml( const QDomNode &layer_node, QgsReadWriteContext &context )
{
  QgsDebugMsgLevel( QStringLiteral( "Datasource in QgsMeshLayer::readXml: %1" ).arg( mDataSource.toLocal8Bit().data() ), 3 );

  //process provider key
  QDomNode pkeyNode = layer_node.namedItem( QStringLiteral( "provider" ) );

  if ( pkeyNode.isNull() )
  {
    mProviderKey.clear();
  }
  else
  {
    QDomElement pkeyElt = pkeyNode.toElement();
    mProviderKey = pkeyElt.text();
  }

  if ( mReadFlags & QgsMapLayer::FlagDontResolveLayers )
  {
    return false;
  }

  QgsDataProvider::ProviderOptions providerOptions;
  if ( !setDataProvider( mProviderKey, providerOptions ) )
  {
    return false;
  }

  QDomElement elemExtraDatasets = layer_node.firstChildElement( QStringLiteral( "extra-datasets" ) );
  if ( !elemExtraDatasets.isNull() )
  {
    QDomElement elemUri = elemExtraDatasets.firstChildElement( QStringLiteral( "uri" ) );
    while ( !elemUri.isNull() )
    {
      QString uri = context.pathResolver().readPath( elemUri.text() );

      bool res = mDataProvider->addDataset( uri );
#ifdef QGISDEBUG
      QgsDebugMsg( QStringLiteral( "extra dataset (res %1): %2" ).arg( res ).arg( uri ) );
#else
      ( void )res; // avoid unused warning in release builds
#endif

      elemUri = elemUri.nextSiblingElement( QStringLiteral( "uri" ) );
    }
  }

  if ( mDataProvider && pkeyNode.toElement().hasAttribute( QStringLiteral( "time-unit" ) ) )
    mDataProvider->setTemporalUnit(
      static_cast<QgsUnitTypes::TemporalUnit>( pkeyNode.toElement().attribute( QStringLiteral( "time-unit" ) ).toInt() ) );

  QString errorMsg;
  readSymbology( layer_node, errorMsg, context );

  // read temporal
  temporalProperties()->readXml( layer_node.toElement(), context );
  if ( !temporalProperties()->timeExtent().begin().isValid() )
    temporalProperties()->setDefaultsFromDataProviderTemporalCapabilities( dataProvider()->temporalCapabilities() );

  //read group states
  QDomElement elemStates = layer_node.firstChildElement( QStringLiteral( "group-states" ) );
  QDomElement elemState = elemStates.firstChildElement( QStringLiteral( "group-state" ) );
  while ( !elemState.isNull() )
  {
    int groupIndex = elemState.attribute( QStringLiteral( "group" ) ).toInt();
    QgsMeshDatasetGroupState state;
    state.used = elemState.attribute( QStringLiteral( "is-used" ) ).toInt();
    state.originalName = elemState.attribute( QStringLiteral( "original-name" ) );
    state.renaming = elemState.attribute( QStringLiteral( "renaming" ) );
    mDatasetGroupsState[groupIndex] = state;
    elemState = elemState.nextSiblingElement( QStringLiteral( "group-state" ) );
  }

  // read static dataset
  QDomElement elemStaticDataset = layer_node.firstChildElement( QStringLiteral( "static-active-dataset" ) );
  if ( elemStaticDataset.hasAttribute( QStringLiteral( "scalar" ) ) )
  {
    QStringList lst = elemStaticDataset.attribute( QStringLiteral( "scalar" ) ).split( QChar( ',' ) );
    if ( lst.count() == 2 )
      mStaticScalarDatasetIndex = QgsMeshDatasetIndex( lst[0].toInt(), lst[1].toInt() );
  }
  if ( elemStaticDataset.hasAttribute( QStringLiteral( "vector" ) ) )
  {
    QStringList lst = elemStaticDataset.attribute( QStringLiteral( "vector" ) ).split( QChar( ',' ) );
    if ( lst.count() == 2 )
      mStaticVectorDatasetIndex = QgsMeshDatasetIndex( lst[0].toInt(), lst[1].toInt() );
  }

  return mValid; // should be true if read successfully
}

bool QgsMeshLayer::writeXml( QDomNode &layer_node, QDomDocument &document, const QgsReadWriteContext &context ) const
{
  // first get the layer element so that we can append the type attribute
  QDomElement mapLayerNode = layer_node.toElement();

  if ( mapLayerNode.isNull() || ( QLatin1String( "maplayer" ) != mapLayerNode.nodeName() ) )
  {
    QgsDebugMsgLevel( QStringLiteral( "can't find <maplayer>" ), 2 );
    return false;
  }

  mapLayerNode.setAttribute( QStringLiteral( "type" ), QStringLiteral( "mesh" ) );

  // add provider node
  if ( mDataProvider )
  {
    QDomElement provider  = document.createElement( QStringLiteral( "provider" ) );
    QDomText providerText = document.createTextNode( providerType() );
    provider.appendChild( providerText );
    layer_node.appendChild( provider );
    provider.setAttribute( QStringLiteral( "time-unit" ), mDataProvider->temporalCapabilities()->temporalUnit() );

    const QStringList extraDatasetUris = mDataProvider->extraDatasets();
    QDomElement elemExtraDatasets = document.createElement( QStringLiteral( "extra-datasets" ) );
    for ( const QString &uri : extraDatasetUris )
    {
      QString path = context.pathResolver().writePath( uri );
      QDomElement elemUri = document.createElement( QStringLiteral( "uri" ) );
      elemUri.appendChild( document.createTextNode( path ) );
      elemExtraDatasets.appendChild( elemUri );
    }
    layer_node.appendChild( elemExtraDatasets );
  }
  // write temporal
  mTemporalProperties->writeXml( mapLayerNode, document, context );

  QDomElement elemStaticDataset = document.createElement( QStringLiteral( "static-active-dataset" ) );
  if ( mStaticScalarDatasetIndex.isValid() )
    elemStaticDataset.setAttribute( QStringLiteral( "scalar" ), QStringLiteral( "%1,%2" ).arg( mStaticScalarDatasetIndex.group() ).arg( mStaticScalarDatasetIndex.dataset() ) );
  if ( mStaticVectorDatasetIndex.isValid() )
    elemStaticDataset.setAttribute( QStringLiteral( "vector" ), QStringLiteral( "%1,%2" ).arg( mStaticVectorDatasetIndex.group() ).arg( mStaticVectorDatasetIndex.dataset() ) );
  layer_node.appendChild( elemStaticDataset );

  //write group states
  QDomElement elemStates = document.createElement( QStringLiteral( "group-states" ) );
  for ( int i : mDatasetGroupsState.keys() )
  {
    QDomElement elemState = document.createElement( QStringLiteral( "group-state" ) );
    elemState.setAttribute( QStringLiteral( "group" ), i );
    elemState.setAttribute( QStringLiteral( "is-used" ), mDatasetGroupsState[i].used ? 1 : 0 );
    elemState.setAttribute( QStringLiteral( "original-name" ), mDatasetGroupsState[i].originalName );
    elemState.setAttribute( QStringLiteral( "renaming" ), mDatasetGroupsState[i].renaming );
    elemStates.appendChild( elemState );
  }
  mapLayerNode.appendChild( elemStates );

  // renderer specific settings
  QString errorMsg;
  return writeSymbology( layer_node, document, errorMsg, context );
}

void QgsMeshLayer::reload()
{
  if ( mDataProvider && mDataProvider->isValid() )
  {
    mDataProvider->reloadData();

    //reload the mesh structure
    if ( !mNativeMesh )
      mNativeMesh.reset( new QgsMesh );

    dataProvider()->populateMesh( mNativeMesh.get() );

    //clear the TriangularMeshes
    mTriangularMeshes.clear();

    //clear the rendererCache
    mRendererCache.reset( new QgsMeshLayerRendererCache() );
  }
}

QStringList QgsMeshLayer::subLayers() const
{
  if ( mDataProvider )
    return mDataProvider->subLayers();
  else
    return QStringList();
}

bool QgsMeshLayer::setDataProvider( QString const &provider, const QgsDataProvider::ProviderOptions &options )
{
  delete mDataProvider;

  mProviderKey = provider;
  QString dataSource = mDataSource;

  mDataProvider = qobject_cast<QgsMeshDataProvider *>( QgsProviderRegistry::instance()->createProvider( provider, dataSource, options ) );
  if ( !mDataProvider )
  {
    QgsDebugMsgLevel( QStringLiteral( "Unable to get mesh data provider" ), 2 );
    return false;
  }

  mDataProvider->setParent( this );
  QgsDebugMsgLevel( QStringLiteral( "Instantiated the mesh data provider plugin" ), 2 );

  mValid = mDataProvider->isValid();
  if ( !mValid )
  {
    QgsDebugMsgLevel( QStringLiteral( "Invalid mesh provider plugin %1" ).arg( QString( mDataSource.toUtf8() ) ), 2 );
    return false;
  }

  setCrs( mDataProvider->crs() );

  if ( provider == QStringLiteral( "mesh_memory" ) )
  {
    // required so that source differs between memory layers
    mDataSource = mDataSource + QStringLiteral( "&uid=%1" ).arg( QUuid::createUuid().toString() );
  }

  for ( int i = 0; i < mDataProvider->datasetGroupCount(); ++i )
    assignDefaultStyleToDatasetGroup( i );

  connect( mDataProvider, &QgsMeshDataProvider::dataChanged, this, &QgsMeshLayer::dataChanged );
  connect( mDataProvider, &QgsMeshDataProvider::datasetGroupsAdded, this, &QgsMeshLayer::onDatasetGroupsAdded );

  return true;
}

QgsMeshLayerTemporalProperties *QgsMeshLayer::temporalProperties()
{
  return  mTemporalProperties;
}
