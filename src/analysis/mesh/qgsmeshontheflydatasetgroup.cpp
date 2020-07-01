/***************************************************************************
                         qgsmeshontheflydatasetgroup.cpp
                         ---------------------
    begin                : June 2020
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

#include "qgsmeshontheflydatasetgroup.h"
#include "qgsmeshlayertemporalproperties.h"

QgsMeshOnTheFlyDatasetGroup::QgsMeshOnTheFlyDatasetGroup( const QString &name, const QString &formulaString, QgsMeshLayer *layer, qint64 relativeStartTime, qint64 relativeEndTime ):
  QgsMeshDatasetGroup( name )
  , mFormula( formulaString )
  , mLayer( layer )
  , mStartTime( relativeStartTime )
  , mEndTime( relativeEndTime )
{
}

void QgsMeshOnTheFlyDatasetGroup::initialize()
{
  QString errMessage;
  mCalcNode.reset( QgsMeshCalcNode::parseMeshCalcString( mFormula, errMessage ) );

  if ( !mCalcNode || !mLayer )
    return;

  mDatasetGroupNameUsed = mCalcNode->usedDatasetGroupNames();
  setDataType( QgsMeshCalcUtils::determineResultDataType( mLayer, mDatasetGroupNameUsed ) );

  //populate used indexes
  QMap<QString, int> usedDatasetGroupindexes;
  const QList<int> &indexes = mLayer->datasetGroupsIndexes();
  for ( int i : indexes )
  {
    QString usedName = mLayer->datasetGroupMetadata( i ).name();
    if ( mDatasetGroupNameUsed.contains( usedName ) )
      usedDatasetGroupindexes[usedName] = i;
  }

  //populate dataset index with time;
  const QList<int> &usedIndexes = usedDatasetGroupindexes.values();
  QSet<qint64> times;
  for ( int groupIndex : usedIndexes )
  {
    int dsCount = mLayer->datasetCount( groupIndex );
    if ( dsCount == 0 )
      return;

    if ( dsCount == 1 ) //non temporal dataset group
      continue;
    for ( int i = 0; i < dsCount; i++ )
    {
      qint64 time = mLayer->datasetRelativeTimeInMilliseconds( QgsMeshDatasetIndex( groupIndex, i ) );
      if ( time != INVALID_MESHLAYER_TIME )
        times.insert( time );
    }
  }

  if ( times.isEmpty() )
    times.insert( 0 );

  mDatasetTimes = times.toList();
  std::sort( mDatasetTimes.begin(), mDatasetTimes.end() );

  mDatasetMetaData = QVector<QgsMeshDatasetMetadata>( mDatasetTimes.count() );

  //to fill metadata, calculate all the datasets one time
  for ( int i = 0; i < mDatasetTimes.count(); ++i )
  {
    mCurrentDatasetIndex = i;
    calculateDataset();
  }

  calculateStatistic();
}

int QgsMeshOnTheFlyDatasetGroup::datasetCount() const
{
  return mDatasetTimes.count();
}

QgsMeshDataset *QgsMeshOnTheFlyDatasetGroup::dataset( int index ) const
{
  if ( index < 0 || index >= mDatasetTimes.count() )
    return nullptr;

  if ( index != mCurrentDatasetIndex )
  {
    mCurrentDatasetIndex = index;
    calculateDataset();
  }

  return mCacheDataset.get();
}

QgsMeshDatasetMetadata QgsMeshOnTheFlyDatasetGroup::datasetMetadata( int datasetIndex ) const
{
  if ( datasetIndex < 0 && datasetIndex >= mDatasetMetaData.count() )
    return QgsMeshDatasetMetadata();

  return mDatasetMetaData.at( datasetIndex );
}

QStringList QgsMeshOnTheFlyDatasetGroup::datasetGroupNamesDependentOn() const
{
  return mDatasetGroupNameUsed;
}

QDomElement QgsMeshOnTheFlyDatasetGroup::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context )
  QDomElement elemDataset = doc.createElement( QStringLiteral( "mesh-dataset" ) );
  elemDataset.setAttribute( QStringLiteral( "source-type" ), QStringLiteral( "on-the-fly" ) );
  elemDataset.setAttribute( QStringLiteral( "name" ), name() );
  elemDataset.setAttribute( QStringLiteral( "formula" ), mFormula );
  elemDataset.setAttribute( QStringLiteral( "startTime" ), mStartTime );
  elemDataset.setAttribute( QStringLiteral( "endTime" ), mEndTime );

  return elemDataset;
}

QString QgsMeshOnTheFlyDatasetGroup::information() const
{
  return QObject::tr( "Formula: %1" ).arg( mFormula );
}

void QgsMeshOnTheFlyDatasetGroup::calculateDataset() const
{
  if ( !mLayer )
    return;

  QgsMeshCalcUtils dsu( mLayer, mDatasetGroupNameUsed, QgsInterval( mDatasetTimes[mCurrentDatasetIndex] / 1000.0 ) );

  //open output dataset
  std::unique_ptr<QgsMeshMemoryDatasetGroup> outputGroup = qgis::make_unique<QgsMeshMemoryDatasetGroup> ( QString(), dsu.outputType() );
  mCalcNode->calculate( dsu, *outputGroup );

  mCacheDataset = outputGroup->memoryDatasets[0];
  if ( !mDatasetMetaData[mCurrentDatasetIndex].isValid() )
  {
    mCacheDataset->calculateMinMax();
    mCacheDataset->time = mDatasetTimes[mCurrentDatasetIndex] / 3600.0 / 1000.0;
    mDatasetMetaData[mCurrentDatasetIndex] = mCacheDataset->metadata();
  }
}

QgsMeshDatasetGroup *QgsMeshOnTheFlyDatasetGroupGenerator::createDatasetGroupFromExpression( const QString &name, const QString &formulaString, QgsMeshLayer *layer, qint64 relativeStartTime, qint64 relativeEndTime )
{
  return new QgsMeshOnTheFlyDatasetGroup( name, formulaString, layer, relativeStartTime, relativeEndTime );
}
