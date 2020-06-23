#ifndef QGSMESHONTHEFLYDATASETGROUP_H
#define QGSMESHONTHEFLYDATASETGROUP_H

#include "qgsmeshdataset.h"
#include "qgsmeshcalcnode.h"

class QgsMeshOnTheFlyDatasetGroup: public QgsMeshDatasetGroup
{
  public:
    QgsMeshOnTheFlyDatasetGroup() = default;
    QgsMeshOnTheFlyDatasetGroup( const QString &name,
                                 const QString &formulaString,
                                 QgsMeshLayer *layer,
                                 qint64 relativeStartTime,
                                 qint64 relativeEndTime ):
      QgsMeshDatasetGroup( name )
      , mLayer( layer )
      , mStartTime( relativeStartTime )
      , mEndTime( relativeEndTime )
    {
      QString errMessage;
      mCalcNode.reset( QgsMeshCalcNode::parseMeshCalcString( formulaString, errMessage ) );

      if ( !mCalcNode || !mLayer )
        return;

      mDatasetGroupNameUsed = mCalcNode->usedDatasetGroupNames();
      setDataType( QgsMeshCalcUtils::determineResultDataType( mLayer, mDatasetGroupNameUsed ) );

      //populate used indexes
      const QList<int> &indexes = mLayer->datasetGroupsIndexes();
      for ( int i : indexes )
      {
        QString usedName = mLayer->datasetGroupMetadata( i ).name();
        if ( mDatasetGroupNameUsed.contains( usedName ) )
          usedDatasetGroupindexes[usedName] = i;
      }

      //populate dataset index with time;
      const QList<int> &usedIndexes = usedDatasetGroupindexes.values();
      QMap<qint64, double> times;
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
          if ( !times.contains( time ) && time >= mStartTime && time <= mEndTime )
            times[time] = time / 3600 / 1000;
        }
      }

      if ( times.isEmpty() )
        times[0] = 0;

      mDatasetGroupMetadata.resize( times.size() );
      const QList<double> &timeList = times.values();
      for ( int i = 0; i < timeList.count(); ++i )
        setTime( i, timeList.at( i ) );

    }

    int datasetCount() const override
    {
      return mDatasetGroupMetadata.count();
    }

    QgsMeshDatasetValue value( QgsMeshDatasetIndex ) const
    {
      //QgsMeshCalcUtils util( mLayer, mDatasetGroupNameUsed, mTime )
    }

  private:
    std::unique_ptr<QgsMeshCalcNode> mCalcNode;
    QgsMeshLayer *mLayer = nullptr;
    qint64 mStartTime = 0.0;
    qint64 mEndTime = 0.0;
    QStringList mDatasetGroupNameUsed;
    QMap<QString, int> usedDatasetGroupindexes;
    QVector<QgsMeshDatasetMetadata> mDatasetGroupMetadata;

    void setTime( int datasetIndex, double value )
    {
      if ( datasetIndex >= mDatasetGroupMetadata.size() )
        return;
      const QgsMeshDatasetMetadata &old = mDatasetGroupMetadata[datasetIndex];
      mDatasetGroupMetadata[datasetIndex] = QgsMeshDatasetMetadata( value, old.isValid(), old.minimum(), old.maximum(), old.maximumVerticalLevelsCount() );
    }

    // QgsMeshDatasetGroup interface
  public:
    QgsMeshDatasetMetadata datasetMetadata( int datasetIndex ) override
    {
      return QgsMeshDatasetMetadata();
    }
    const QgsMeshDataset *dataset( int index ) const
    {
      return nullptr;
    }
};

#endif // QGSMESHONTHEFLYDATASETGROUP_H
