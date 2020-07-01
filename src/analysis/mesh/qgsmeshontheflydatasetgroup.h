/***************************************************************************
                         qgsmeshontheflydatasetgroup.h
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

#ifndef QGSMESHONTHEFLYDATASETGROUP_H
#define QGSMESHONTHEFLYDATASETGROUP_H

#include "qgsmeshdataset.h"
#include "qgsmeshcalcnode.h"
#include "qgsmeshdatagenerator.h"

#include "qgsmeshlayertemporalproperties.h"

#define SIP_NO_FILE

class ANALYSIS_EXPORT QgsMeshOnTheFlyDatasetGroup: public QgsMeshDatasetGroup
{
  public:
    /**
     * Constructor
     * \param name name of the dataset group
     * \param formulaString formula use to define the dataset group
     * \param layer mesh layer that contains dataset group
     * \param relativeStartTime relative time start, in mimliseconds, from the mesh layer provider reference time
     * \param relativeEndTime relative time end, in mimliseconds, from the mesh layer provider reference time
     */
    QgsMeshOnTheFlyDatasetGroup( const QString &name,
                                 const QString &formulaString,
                                 QgsMeshLayer *layer,
                                 qint64 relativeStartTime,
                                 qint64 relativeEndTime );

    void initialize() override;
    int datasetCount() const override;
    QgsMeshDataset *dataset( int index ) const override;
    QgsMeshDatasetMetadata datasetMetadata( int datasetIndex ) const override;
    QgsMeshDatasetGroup::Type type() const override {return QgsMeshDatasetGroup::Expression;}
    QStringList datasetGroupNamesDependentOn() const override;
    QDomElement writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const override;
    QString information() const override;

  private:
    QString mFormula;
    std::unique_ptr<QgsMeshCalcNode> mCalcNode;
    QgsMeshLayer *mLayer = nullptr;
    qint64 mStartTime = 0.0;
    qint64 mEndTime = 0.0;
    QStringList mDatasetGroupNameUsed;
    QList<qint64> mDatasetTimes;

    mutable std::shared_ptr<QgsMeshMemoryDataset> mCacheDataset;
    mutable QVector<QgsMeshDatasetMetadata> mDatasetMetaData;
    mutable int mCurrentDatasetIndex = -1;

    void calculateDataset() const;
};


class ANALYSIS_EXPORT QgsMeshOnTheFlyDatasetGroupGenerator: public QgsMeshDataGeneratorInterface
{
  public:

    QgsMeshDatasetGroup *createDatasetGroupFromExpression( const QString &name,
        const QString &formulaString,
        QgsMeshLayer *layer,
        qint64 relativeStartTime,
        qint64 relativeEndTime ) override;

    QString key() const override {return QStringLiteral( "on-the-fly" );}
};



#endif // QGSMESHONTHEFLYDATASETGROUP_H
