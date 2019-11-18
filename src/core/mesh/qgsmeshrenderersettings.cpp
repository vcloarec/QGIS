/***************************************************************************
                         qgsmeshrenderersettings.cpp
                         ---------------------------
    begin                : May 2018
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

#include "qgsmeshrenderersettings.h"
#include "qgssymbollayerutils.h"


bool QgsMeshRendererMeshSettings::isEnabled() const
{
  return mEnabled;
}

void QgsMeshRendererMeshSettings::setEnabled( bool on )
{
  mEnabled = on;
}

double QgsMeshRendererMeshSettings::lineWidth() const
{
  return mLineWidth;
}

void QgsMeshRendererMeshSettings::setLineWidth( double lineWidth )
{
  mLineWidth = lineWidth;
}

QColor QgsMeshRendererMeshSettings::color() const
{
  return mColor;
}

void QgsMeshRendererMeshSettings::setColor( const QColor &color )
{
  mColor = color;
}

QDomElement QgsMeshRendererMeshSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( QStringLiteral( "mesh-settings" ) );
  elem.setAttribute( QStringLiteral( "enabled" ), mEnabled ? QStringLiteral( "1" ) : QStringLiteral( "0" ) );
  elem.setAttribute( QStringLiteral( "line-width" ), mLineWidth );
  elem.setAttribute( QStringLiteral( "color" ), QgsSymbolLayerUtils::encodeColor( mColor ) );
  return elem;
}

void QgsMeshRendererMeshSettings::readXml( const QDomElement &elem )
{
  mEnabled = elem.attribute( QStringLiteral( "enabled" ) ).toInt();
  mLineWidth = elem.attribute( QStringLiteral( "line-width" ) ).toDouble();
  mColor = QgsSymbolLayerUtils::decodeColor( elem.attribute( QStringLiteral( "color" ) ) );
}

// ---------------------------------------------------------------------

QgsColorRampShader QgsMeshRendererScalarSettings::colorRampShader() const
{
  return mColorRampShader;
}

void QgsMeshRendererScalarSettings::setColorRampShader( const QgsColorRampShader &shader )
{
  mColorRampShader = shader;
}

double QgsMeshRendererScalarSettings::classificationMinimum() const { return mClassificationMinimum; }

double QgsMeshRendererScalarSettings::classificationMaximum() const { return mClassificationMaximum; }

void QgsMeshRendererScalarSettings::setClassificationMinimumMaximum( double minimum, double maximum )
{
  mClassificationMinimum = minimum;
  mClassificationMaximum = maximum;
}

double QgsMeshRendererScalarSettings::opacity() const { return mOpacity; }

void QgsMeshRendererScalarSettings::setOpacity( double opacity ) { mOpacity = opacity; }

QgsMeshRendererScalarSettings::DataInterpolationMethod QgsMeshRendererScalarSettings::dataInterpolationMethod() const
{
  return mDataInterpolationMethod;
}

void QgsMeshRendererScalarSettings::setDataInterpolationMethod( const QgsMeshRendererScalarSettings::DataInterpolationMethod &dataInterpolationMethod )
{
  mDataInterpolationMethod = dataInterpolationMethod;
}

QDomElement QgsMeshRendererScalarSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( QStringLiteral( "scalar-settings" ) );
  elem.setAttribute( QStringLiteral( "min-val" ), mClassificationMinimum );
  elem.setAttribute( QStringLiteral( "max-val" ), mClassificationMaximum );
  elem.setAttribute( QStringLiteral( "opacity" ), mOpacity );

  QString methodTxt;
  switch ( mDataInterpolationMethod )
  {
    case None:
      methodTxt = QStringLiteral( "none" );
      break;
    case NeighbourAverage:
      methodTxt = QStringLiteral( "neighbour-average" );
      break;
  }
  elem.setAttribute( QStringLiteral( "interpolation-method" ), methodTxt );
  QDomElement elemShader = mColorRampShader.writeXml( doc );
  elem.appendChild( elemShader );
  return elem;
}

void QgsMeshRendererScalarSettings::readXml( const QDomElement &elem )
{
  mClassificationMinimum = elem.attribute( QStringLiteral( "min-val" ) ).toDouble();
  mClassificationMaximum = elem.attribute( QStringLiteral( "max-val" ) ).toDouble();
  mOpacity = elem.attribute( QStringLiteral( "opacity" ) ).toDouble();
  QString methodTxt = elem.attribute( QStringLiteral( "interpolation-method" ) );
  if ( QStringLiteral( "neighbour-average" ) == methodTxt )
  {
    mDataInterpolationMethod = DataInterpolationMethod::NeighbourAverage;
  }
  else
  {
    mDataInterpolationMethod = DataInterpolationMethod::None;
  }
  QDomElement elemShader = elem.firstChildElement( QStringLiteral( "colorrampshader" ) );
  mColorRampShader.readXml( elemShader );
}

// ---------------------------------------------------------------------

double QgsMeshRendererVectorArrowSettings::lineWidth() const
{
  return mLineWidth;
}

void QgsMeshRendererVectorArrowSettings::setLineWidth( double lineWidth )
{
  mLineWidth = lineWidth;
}

QColor QgsMeshRendererVectorArrowSettings::color() const
{
  return mColor;
}

void QgsMeshRendererVectorArrowSettings::setColor( const QColor &vectorColor )
{
  mColor = vectorColor;
}

double QgsMeshRendererVectorArrowSettings::filterMin() const
{
  return mFilterMin;
}

void QgsMeshRendererVectorArrowSettings::setFilterMin( double vectorFilterMin )
{
  mFilterMin = vectorFilterMin;
}

double QgsMeshRendererVectorArrowSettings::filterMax() const
{
  return mFilterMax;
}

void QgsMeshRendererVectorArrowSettings::setFilterMax( double vectorFilterMax )
{
  mFilterMax = vectorFilterMax;
}

QgsMeshRendererVectorArrowSettings::ArrowScalingMethod QgsMeshRendererVectorArrowSettings::shaftLengthMethod() const
{
  return mShaftLengthMethod;
}

void QgsMeshRendererVectorArrowSettings::setShaftLengthMethod( QgsMeshRendererVectorArrowSettings::ArrowScalingMethod shaftLengthMethod )
{
  mShaftLengthMethod = shaftLengthMethod;
}

double QgsMeshRendererVectorArrowSettings::minShaftLength() const
{
  return mMinShaftLength;
}

void QgsMeshRendererVectorArrowSettings::setMinShaftLength( double minShaftLength )
{
  mMinShaftLength = minShaftLength;
}

double QgsMeshRendererVectorArrowSettings::maxShaftLength() const
{
  return mMaxShaftLength;
}

void QgsMeshRendererVectorArrowSettings::setMaxShaftLength( double maxShaftLength )
{
  mMaxShaftLength = maxShaftLength;
}

double QgsMeshRendererVectorArrowSettings::scaleFactor() const
{
  return mScaleFactor;
}

void QgsMeshRendererVectorArrowSettings::setScaleFactor( double scaleFactor )
{
  mScaleFactor = scaleFactor;
}

double QgsMeshRendererVectorArrowSettings::fixedShaftLength() const
{
  return mFixedShaftLength;
}

void QgsMeshRendererVectorArrowSettings::setFixedShaftLength( double fixedShaftLength )
{
  mFixedShaftLength = fixedShaftLength;
}

double QgsMeshRendererVectorArrowSettings::arrowHeadWidthRatio() const
{
  return mArrowHeadWidthRatio;
}

void QgsMeshRendererVectorArrowSettings::setArrowHeadWidthRatio( double vectorHeadWidthRatio )
{
  mArrowHeadWidthRatio = vectorHeadWidthRatio;
}

double QgsMeshRendererVectorArrowSettings::arrowHeadLengthRatio() const
{
  return mArrowHeadLengthRatio;
}

void QgsMeshRendererVectorArrowSettings::setArrowHeadLengthRatio( double vectorHeadLengthRatio )
{
  mArrowHeadLengthRatio = vectorHeadLengthRatio;
}

bool QgsMeshRendererVectorArrowSettings::isOnUserDefinedGrid() const
{
  return mOnUserDefinedGrid;
}

void QgsMeshRendererVectorArrowSettings::setOnUserDefinedGrid( bool enabled )
{
  mOnUserDefinedGrid = enabled;
}

int QgsMeshRendererVectorArrowSettings::userGridCellWidth() const
{
  return mUserGridCellWidth;
}

void QgsMeshRendererVectorArrowSettings::setUserGridCellWidth( int width )
{
  mUserGridCellWidth = width;
}

int QgsMeshRendererVectorArrowSettings::userGridCellHeight() const
{
  return mUserGridCellHeight;
}

void QgsMeshRendererVectorArrowSettings::setUserGridCellHeight( int height )
{
  mUserGridCellHeight = height;
}


QDomElement QgsMeshRendererVectorArrowSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( QStringLiteral( "vector-arrow-settings" ) );
  elem.setAttribute( QStringLiteral( "line-width" ), mLineWidth );
  elem.setAttribute( QStringLiteral( "color" ), QgsSymbolLayerUtils::encodeColor( mColor ) );
  elem.setAttribute( QStringLiteral( "filter-min" ), mFilterMin );
  elem.setAttribute( QStringLiteral( "filter-max" ), mFilterMax );
  elem.setAttribute( QStringLiteral( "arrow-head-width-ratio" ), mArrowHeadWidthRatio );
  elem.setAttribute( QStringLiteral( "arrow-head-length-ratio" ), mArrowHeadLengthRatio );
  elem.setAttribute( QStringLiteral( "user-grid-enabled" ), mOnUserDefinedGrid ? QStringLiteral( "1" ) : QStringLiteral( "0" ) );
  elem.setAttribute( QStringLiteral( "user-grid-width" ), mUserGridCellWidth );
  elem.setAttribute( QStringLiteral( "user-grid-height" ), mUserGridCellHeight );

  QDomElement elemShaft = doc.createElement( QStringLiteral( "shaft-length" ) );
  QString methodTxt;
  switch ( mShaftLengthMethod )
  {
    case MinMax:
      methodTxt = QStringLiteral( "minmax" );
      elemShaft.setAttribute( QStringLiteral( "min" ), mMinShaftLength );
      elemShaft.setAttribute( QStringLiteral( "max" ), mMaxShaftLength );
      break;
    case Scaled:
      methodTxt = QStringLiteral( "scaled" );
      elemShaft.setAttribute( QStringLiteral( "scale-factor" ), mScaleFactor );
      break;
    case Fixed:
      methodTxt = QStringLiteral( "fixed" ) ;
      elemShaft.setAttribute( QStringLiteral( "fixed-length" ), mFixedShaftLength );
      break;
  }
  elemShaft.setAttribute( QStringLiteral( "method" ), methodTxt );
  elem.appendChild( elemShaft );
  return elem;
}

void QgsMeshRendererVectorArrowSettings::readXml( const QDomElement &elem )
{
  mLineWidth = elem.attribute( QStringLiteral( "line-width" ) ).toDouble();
  mColor = QgsSymbolLayerUtils::decodeColor( elem.attribute( QStringLiteral( "color" ) ) );
  mFilterMin = elem.attribute( QStringLiteral( "filter-min" ) ).toDouble();
  mFilterMax = elem.attribute( QStringLiteral( "filter-max" ) ).toDouble();
  mArrowHeadWidthRatio = elem.attribute( QStringLiteral( "arrow-head-width-ratio" ) ).toDouble();
  mArrowHeadLengthRatio = elem.attribute( QStringLiteral( "arrow-head-length-ratio" ) ).toDouble();
  mOnUserDefinedGrid = elem.attribute( QStringLiteral( "user-grid-enabled" ) ).toInt(); //bool
  mUserGridCellWidth = elem.attribute( QStringLiteral( "user-grid-width" ) ).toInt();
  mUserGridCellHeight = elem.attribute( QStringLiteral( "user-grid-height" ) ).toInt();

  QDomElement elemShaft = elem.firstChildElement( QStringLiteral( "shaft-length" ) );
  QString methodTxt = elemShaft.attribute( QStringLiteral( "method" ) );
  if ( QStringLiteral( "minmax" ) == methodTxt )
  {
    mShaftLengthMethod = MinMax;
    mMinShaftLength = elemShaft.attribute( QStringLiteral( "min" ) ).toDouble();
    mMaxShaftLength = elemShaft.attribute( QStringLiteral( "max" ) ).toDouble();
  }
  else if ( QStringLiteral( "scaled" ) == methodTxt )
  {
    mShaftLengthMethod = Scaled;
    mScaleFactor = elemShaft.attribute( QStringLiteral( "scale-factor" ) ).toDouble();
  }
  else  // fixed
  {
    mShaftLengthMethod = Fixed;
    mFixedShaftLength = elemShaft.attribute( QStringLiteral( "fixed-length" ) ).toDouble();
  }
}

// ---------------------------------------------------------------------

QDomElement QgsMeshRendererSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( QStringLiteral( "mesh-renderer-settings" ) );

  QDomElement elemActiveDataset = doc.createElement( QStringLiteral( "active-dataset" ) );
  if ( mActiveScalarDataset.isValid() )
    elemActiveDataset.setAttribute( QStringLiteral( "scalar" ), QStringLiteral( "%1,%2" ).arg( mActiveScalarDataset.group() ).arg( mActiveScalarDataset.dataset() ) );
  if ( mActiveVectorDataset.isValid() )
    elemActiveDataset.setAttribute( QStringLiteral( "vector" ), QStringLiteral( "%1,%2" ).arg( mActiveVectorDataset.group() ).arg( mActiveVectorDataset.dataset() ) );
  elem.appendChild( elemActiveDataset );

  for ( int groupIndex : mRendererScalarSettings.keys() )
  {
    const QgsMeshRendererScalarSettings &scalarSettings = mRendererScalarSettings[groupIndex];
    QDomElement elemScalar = scalarSettings.writeXml( doc );
    elemScalar.setAttribute( QStringLiteral( "group" ), groupIndex );
    elem.appendChild( elemScalar );
  }

  for ( int groupIndex : mRendererVectorSettings.keys() )
  {
    const QgsMeshRendererVectorSettings &vectorSettings = mRendererVectorSettings[groupIndex];
    QDomElement elemVector = vectorSettings.writeXml( doc );
    elemVector.setAttribute( QStringLiteral( "group" ), groupIndex );
    elem.appendChild( elemVector );
  }

  QDomElement elemNativeMesh = mRendererNativeMeshSettings.writeXml( doc );
  elemNativeMesh.setTagName( QStringLiteral( "mesh-settings-native" ) );
  elem.appendChild( elemNativeMesh );

  QDomElement elemTriangularMesh = mRendererTriangularMeshSettings.writeXml( doc );
  elemTriangularMesh.setTagName( QStringLiteral( "mesh-settings-triangular" ) );
  elem.appendChild( elemTriangularMesh );

  return elem;
}

void QgsMeshRendererSettings::readXml( const QDomElement &elem )
{
  mRendererScalarSettings.clear();
  mRendererVectorSettings.clear();

  QDomElement elemActiveDataset = elem.firstChildElement( QStringLiteral( "active-dataset" ) );
  if ( elemActiveDataset.hasAttribute( QStringLiteral( "scalar" ) ) )
  {
    QStringList lst = elemActiveDataset.attribute( QStringLiteral( "scalar" ) ).split( QChar( ',' ) );
    if ( lst.count() == 2 )
      mActiveScalarDataset = QgsMeshDatasetIndex( lst[0].toInt(), lst[1].toInt() );
  }
  if ( elemActiveDataset.hasAttribute( QStringLiteral( "vector" ) ) )
  {
    QStringList lst = elemActiveDataset.attribute( QStringLiteral( "vector" ) ).split( QChar( ',' ) );
    if ( lst.count() == 2 )
      mActiveVectorDataset = QgsMeshDatasetIndex( lst[0].toInt(), lst[1].toInt() );
  }

  QDomElement elemScalar = elem.firstChildElement( QStringLiteral( "scalar-settings" ) );
  while ( !elemScalar.isNull() )
  {
    int groupIndex = elemScalar.attribute( QStringLiteral( "group" ) ).toInt();
    QgsMeshRendererScalarSettings scalarSettings;
    scalarSettings.readXml( elemScalar );
    mRendererScalarSettings.insert( groupIndex, scalarSettings );

    elemScalar = elemScalar.nextSiblingElement( QStringLiteral( "scalar-settings" ) );
  }

  QDomElement elemVector = elem.firstChildElement( QStringLiteral( "vector-settings" ) );
  while ( !elemVector.isNull() )
  {
    int groupIndex = elemVector.attribute( QStringLiteral( "group" ) ).toInt();
    QgsMeshRendererVectorSettings vectorSettings;
    vectorSettings.readXml( elemVector );
    mRendererVectorSettings.insert( groupIndex, vectorSettings );

    elemVector = elemVector.nextSiblingElement( QStringLiteral( "vector-settings" ) );
  }

  QDomElement elemNativeMesh = elem.firstChildElement( QStringLiteral( "mesh-settings-native" ) );
  mRendererNativeMeshSettings.readXml( elemNativeMesh );

  QDomElement elemTriangularMesh = elem.firstChildElement( QStringLiteral( "mesh-settings-triangular" ) );
  mRendererTriangularMeshSettings.readXml( elemTriangularMesh );
}

QgsMeshRendererVectorStreamlineSettings::SeedingStartPointsMethod QgsMeshRendererVectorStreamlineSettings::seedingMethod() const
{
  return mSeedingMethod;
}

void QgsMeshRendererVectorStreamlineSettings::setSeedingMethod( const SeedingStartPointsMethod &seedingMethod )
{
  mSeedingMethod = seedingMethod;
}

double QgsMeshRendererVectorStreamlineSettings::seedingDensity() const
{
  return mSeedingDensity;
}

void QgsMeshRendererVectorStreamlineSettings::setSeedingDensity( double seedingDensity )
{
  mSeedingDensity = seedingDensity;
}

QgsMeshRendererVectorStreamlineSettings::ColorMethod QgsMeshRendererVectorStreamlineSettings::colorMethod() const
{
  return mColorMethod;
}

void QgsMeshRendererVectorStreamlineSettings::setColorMethod( const QgsMeshRendererVectorStreamlineSettings::ColorMethod &colorMethod )
{
  mColorMethod = colorMethod;
}

double QgsMeshRendererVectorStreamlineSettings::lineWidth() const
{
  return mLineWidth;
}

void QgsMeshRendererVectorStreamlineSettings::setLineWidth( double lineWidth )
{
  mLineWidth = lineWidth;
}

QColor QgsMeshRendererVectorStreamlineSettings::fixedColor() const
{
  return mFixedColor;
}

void QgsMeshRendererVectorStreamlineSettings::setFixedColor( const QColor &fixedColor )
{
  mFixedColor = fixedColor;
}

QgsColorRampShader QgsMeshRendererVectorStreamlineSettings::colorRampShader() const
{
  return mColorRampShader;
}

void QgsMeshRendererVectorStreamlineSettings::setColorRampShader( const QgsColorRampShader &colorRampShader )
{
  mColorRampShader = colorRampShader;
}

QDomElement QgsMeshRendererVectorStreamlineSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( QStringLiteral( "vector-streamline-settings" ) );

  elem.setAttribute( QStringLiteral( "seeding-method" ), mSeedingMethod );
  elem.setAttribute( QStringLiteral( "seeding-density" ), mSeedingDensity );
  elem.setAttribute( QStringLiteral( "line-width" ), mLineWidth );

  elem.setAttribute( QStringLiteral( "is-weght-with-scalar" ), mIsWeightWithScalar );
  elem.setAttribute( QStringLiteral( "weight-dataset-group-scalar-index" ), mWeightDatasetGroupScalarIndex );
  elem.setAttribute( QStringLiteral( "minimum-magnitude-filter" ), mMinMagFilter );
  elem.setAttribute( QStringLiteral( "maximum-magnitude-filter" ), mMaxMagFilter );

  //color
  elem.setAttribute( QStringLiteral( "color-method" ), mColorMethod );
  elem.setAttribute( QStringLiteral( "fixed-color" ), QgsSymbolLayerUtils::encodeColor( mFixedColor ) );
  QDomElement elemShader = mColorRampShader.writeXml( doc );
  elem.appendChild( elemShader );
  elem.setAttribute( QStringLiteral( "opacity" ), mOpacity );

  return elem;
}

double QgsMeshRendererVectorStreamlineSettings::opacity() const
{
  return mOpacity;
}

void QgsMeshRendererVectorStreamlineSettings::setOpacity( double opacity )
{
  mOpacity = opacity;
}

void QgsMeshRendererVectorStreamlineSettings::readXml( const QDomElement &elem )
{
  mSeedingMethod =
    static_cast<QgsMeshRendererVectorStreamlineSettings::SeedingStartPointsMethod>(
      elem.attribute( QStringLiteral( "seeding-method" ) ).toInt() );
  mSeedingDensity = elem.attribute( QStringLiteral( "seeding-density" ) ).toDouble();
  mLineWidth = elem.attribute( QStringLiteral( "line-width" ) ).toDouble();

  mIsWeightWithScalar = elem.attribute( QStringLiteral( "is-weght-with-scalar" ) ).toInt(); //bool
  mWeightDatasetGroupScalarIndex = elem.attribute( QStringLiteral( "weight-dataset-group-scalar-index" ) ).toInt();
  mMinMagFilter = elem.attribute( QStringLiteral( "minimum-magnitude-filter" ) ).toDouble();
  mMaxMagFilter = elem.attribute( QStringLiteral( "maximum-magnitude-filter" ) ).toDouble();

  //color
  mColorMethod = static_cast<QgsMeshRendererVectorStreamlineSettings::ColorMethod>(
                   elem.attribute( QStringLiteral( "color-method" ) ).toInt() );
  mFixedColor = QgsSymbolLayerUtils::decodeColor( elem.attribute( QStringLiteral( "fixed-color" ) ) );
  QDomElement elemShader = elem.firstChildElement( QStringLiteral( "colorrampshader" ) );
  mColorRampShader.readXml( elemShader );
  mOpacity = elem.attribute( QStringLiteral( "opacity" ) ).toDouble();
}

bool QgsMeshRendererVectorStreamlineSettings::isWeightWithScalar() const
{
  return mIsWeightWithScalar;
}

void QgsMeshRendererVectorStreamlineSettings::setIsWeightWithScalar( bool isWeightWithScalar )
{
  mIsWeightWithScalar = isWeightWithScalar;
}

int QgsMeshRendererVectorStreamlineSettings::weightDatasetGroupScalarIndex() const
{
  return mWeightDatasetGroupScalarIndex;
}

void QgsMeshRendererVectorStreamlineSettings::setWeightDatasetGroupScalarIndex( int weightDatasetGroupScalarIndex )
{
  mWeightDatasetGroupScalarIndex = weightDatasetGroupScalarIndex;
}

double QgsMeshRendererVectorStreamlineSettings::minMagFilter() const
{
  return mMinMagFilter;
}

void QgsMeshRendererVectorStreamlineSettings::setMinMagFilter( double minMagFilter )
{
  mMinMagFilter = minMagFilter;
}

double QgsMeshRendererVectorStreamlineSettings::maxMagFilter() const
{
  return mMaxMagFilter;
}

void QgsMeshRendererVectorStreamlineSettings::setMaxMagFilter( double maxMagFilter )
{
  mMaxMagFilter = maxMagFilter;
}

QgsMeshRendererVectorStreamlineSettings QgsMeshRendererVectorSettings::streamLinesSettings() const
{
  return mStreamLinesSettings;
}

void QgsMeshRendererVectorSettings::setStreamLinesSettings( const QgsMeshRendererVectorStreamlineSettings &streamLinesSettings )
{
  mStreamLinesSettings = streamLinesSettings;
}

QgsMeshRendererVectorArrowSettings QgsMeshRendererVectorSettings::arrowSettings() const
{
  return mArrowsSettings;
}

void QgsMeshRendererVectorSettings::setArrowsSettings( const QgsMeshRendererVectorArrowSettings &arrowsSettings )
{
  mArrowsSettings = arrowsSettings;
}

QgsMeshRendererVectorTracesSettings QgsMeshRendererVectorSettings::tracesSettings() const
{
  return mTracesSettings;
}

void QgsMeshRendererVectorSettings::setTracesSettings( const QgsMeshRendererVectorTracesSettings &tracesSettings )
{
  mTracesSettings = tracesSettings;
}

QgsMeshRendererVectorSettings::DisplayingMethod QgsMeshRendererVectorSettings::displayingMethod() const
{
  return mDisplayingMethod;
}

void QgsMeshRendererVectorSettings::setDisplayingMethod( const DisplayingMethod &displayingMethod )
{
  mDisplayingMethod = displayingMethod;
}
