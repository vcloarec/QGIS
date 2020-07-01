/***************************************************************************
                          qgsmeshcalculator.cpp
                          ---------------------
    begin                : December 18th, 2018
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

#include <QFileInfo>
#include <limits>
#include <memory>

#include "qgsmeshcalcnode.h"
#include "qgsmeshcalculator.h"
#include "qgsmeshcalcutils.h"
#include "qgsmeshmemorydataprovider.h"
#include "qgsmeshontheflydatasetgroup.h"
#include "qgis.h"

QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputFile,
                                      const QgsRectangle &outputExtent,
                                      double startTime,
                                      double endTime,
                                      QgsMeshLayer *layer )
  : mFormulaString( formulaString )
  , mOutputDriver( QStringLiteral( "DAT" ) )
  , mOutputFile( outputFile )
  , mOutputExtent( outputExtent )
  , mUseMask( false )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
  if ( !mOutputFile.isEmpty() )
    mOutputGroupName = QFileInfo( mOutputFile ).baseName();
}

QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputFile,
                                      const QgsGeometry &outputMask,
                                      double startTime,
                                      double endTime,
                                      QgsMeshLayer *layer )
  : mFormulaString( formulaString )
  , mOutputDriver( QStringLiteral( "DAT" ) )
  , mOutputFile( outputFile )
  , mOutputMask( outputMask )
  , mUseMask( true )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
  if ( !mOutputFile.isEmpty() )
    mOutputGroupName = QFileInfo( mOutputFile ).baseName();
}


QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputDriver,
                                      const QString &outputGroupName,
                                      const QString &outputFile,
                                      const QgsRectangle &outputExtent,
                                      double startTime,
                                      double endTime,
                                      QgsMeshLayer *layer )
  : mFormulaString( formulaString )
  , mOutputDriver( outputDriver )
  , mOutputGroupName( outputGroupName )
  , mOutputFile( outputFile )
  , mOutputExtent( outputExtent )
  , mUseMask( false )
  , mDestination( QgsMeshCalculator::OnFile )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
}

QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputDriver,
                                      const QString &outputGroupName,
                                      const QString &outputFile,
                                      const QgsGeometry &outputMask,
                                      double startTime,
                                      double endTime,
                                      QgsMeshLayer *layer )
  : mFormulaString( formulaString )
  , mOutputDriver( outputDriver )
  , mOutputGroupName( outputGroupName )
  , mOutputFile( outputFile )
  , mOutputMask( outputMask )
  , mUseMask( true )
  , mDestination( QgsMeshCalculator::OnFile )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
}

QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputGroupName,
                                      const QgsRectangle &outputExtent,
                                      const QgsMeshCalculator::Destination &destination,
                                      QgsMeshLayer *layer,
                                      double startTime,
                                      double endTime )
  : mFormulaString( formulaString )
  , mOutputGroupName( outputGroupName )
  , mOutputExtent( outputExtent )
  , mUseMask( false )
  , mDestination( destination )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
}

QgsMeshCalculator::QgsMeshCalculator( const QString &formulaString,
                                      const QString &outputGroupName,
                                      const QgsGeometry &outputMask,
                                      const Destination &destination,
                                      QgsMeshLayer *layer,
                                      double startTime,
                                      double endTime )
  : mFormulaString( formulaString )
  , mOutputGroupName( outputGroupName )
  , mOutputMask( outputMask )
  , mUseMask( true )
  , mDestination( destination )
  , mStartTime( startTime )
  , mEndTime( endTime )
  , mMeshLayer( layer )
{
}

QgsMeshCalculator::Result QgsMeshCalculator::expression_valid(
  const QString &formulaString,
  QgsMeshLayer *layer )
{
  QgsMeshDriverMetadata::MeshDriverCapability cap;
  return QgsMeshCalculator::expressionIsValid( formulaString, layer, cap );
}

QgsMeshCalculator::Result QgsMeshCalculator::expressionIsValid(
  const QString &formulaString,
  QgsMeshLayer *layer,
  QgsMeshDriverMetadata::MeshDriverCapability &requiredCapability )
{
  QString errorString;
  std::unique_ptr< QgsMeshCalcNode > calcNode( QgsMeshCalcNode::parseMeshCalcString( formulaString, errorString ) );
  if ( !calcNode )
    return ParserError;

  if ( !layer || !layer->dataProvider() )
    return InputLayerError;

  QgsMeshDatasetGroupMetadata::DataType dataType = QgsMeshCalcUtils::determineResultDataType( layer, calcNode->usedDatasetGroupNames() );

  requiredCapability = dataType == QgsMeshDatasetGroupMetadata::DataOnFaces ? QgsMeshDriverMetadata::MeshDriverCapability::CanWriteFaceDatasets :
                       QgsMeshDriverMetadata::MeshDriverCapability::CanWriteVertexDatasets;

  return Success;
}

QgsMeshCalculator::Result QgsMeshCalculator::processCalculation( QgsFeedback *feedback )
{
  // check input
  if ( mOutputFile.isEmpty() && !( mDestination != OnFile ) )
  {
    return CreateOutputError;
  }

  if ( !mMeshLayer ||
       !mMeshLayer->dataProvider() ||
       mMeshLayer->providerType() != QStringLiteral( "mdal" )
     )
  {
    return CreateOutputError;
  }

  //prepare search string / tree
  QString errorString;
  std::unique_ptr< QgsMeshCalcNode > calcNode( QgsMeshCalcNode::parseMeshCalcString( mFormulaString, errorString ) );
  if ( !calcNode )
  {
    return ParserError;
  }

  // proceed eventually on the fly
  bool err;
  if ( mDestination == QgsMeshCalculator::OnTheFly )
  {
    std::unique_ptr<QgsMeshDatasetGroup> datasetGroupOnTheFly = qgis::make_unique<QgsMeshOnTheFlyDatasetGroup> ( mOutputGroupName, mFormulaString, mMeshLayer, mStartTime * 3600 * 1000, mEndTime * 3600 * 1000 );
    datasetGroupOnTheFly->initialize();

    err = !mMeshLayer->addDatasets( datasetGroupOnTheFly.release() );
    if ( err )
    {
      return CreateOutputError;
    }

    if ( feedback )
    {
      feedback->setProgress( 100.0 );
    }
    return Success;
  }

  //open output dataset
  QgsMeshCalcUtils dsu( mMeshLayer, calcNode->usedDatasetGroupNames(), mStartTime, mEndTime );
  if ( !dsu.isValid() )
  {
    return InvalidDatasets;
  }

  std::unique_ptr<QgsMeshMemoryDatasetGroup> outputGroup = qgis::make_unique<QgsMeshMemoryDatasetGroup> ( mOutputGroupName, dsu.outputType() );

  // calculate
  bool ok = calcNode->calculate( dsu, *outputGroup );
  if ( !ok )
  {
    return EvaluateError;
  }

  if ( feedback && feedback->isCanceled() )
  {
    return Canceled;
  }
  if ( feedback )
  {
    feedback->setProgress( 60.0 );
  }

  // Finalize dataset
  if ( mUseMask )
  {
    dsu.filter( *outputGroup, mOutputMask );
  }
  else
  {
    dsu.filter( *outputGroup, mOutputExtent );
  }
  outputGroup->setIsScalar( true );

  // before storing the file, find out if the process is not already canceled
  if ( feedback && feedback->isCanceled() )
  {
    return Canceled;
  }
  if ( feedback )
  {
    feedback->setProgress( 80.0 );
  }

  // store to file or in memory
  QVector<QgsMeshDataBlock> datasetValues;
  QVector<QgsMeshDataBlock> datasetActive;
  QVector<double> times;

  const auto datasize = outputGroup->datasetCount();
  datasetValues.reserve( datasize );
  times.reserve( datasize );

  for ( int i = 0; i < datasize; ++i )
  {
    const std::shared_ptr<QgsMeshMemoryDataset> dataset = outputGroup->memoryDatasets.at( i );

    times.push_back( dataset->time );
    datasetValues.push_back(
      dataset->datasetValues( outputGroup->isScalar(),
                              0,
                              dataset->values.size() )
    );
    if ( !dataset->active.isEmpty() )
    {
      datasetActive.push_back(
        dataset->areFacesActive(
          0,
          dataset->active.size() )
      );
    }
  }

  // calculate statistics
  outputGroup->initialize();

  const QgsMeshDatasetGroupMetadata meta = outputGroup->groupMetadata();

  if ( mDestination == OnMemory )
  {
    err = !mMeshLayer->addDatasets( outputGroup.release() );
  }
  else
  {
    err = mMeshLayer->dataProvider()->persistDatasetGroup(
            mOutputFile,
            mOutputDriver,
            meta,
            datasetValues,
            datasetActive,
            times
          );
  }


  if ( err )
  {
    return CreateOutputError;
  }

  if ( feedback )
  {
    feedback->setProgress( 100.0 );
  }
  return Success;
}
