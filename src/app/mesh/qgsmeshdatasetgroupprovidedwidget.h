#ifndef QGSMESHDATASETGROUPPROVIDEDWIDGET_H
#define QGSMESHDATASETGROUPPROVIDEDWIDGET_H

#include "ui_qgsmeshdatasetgroupprovidedwidgetbase.h"

class QgsMeshLayer;

class APP_EXPORT QgsMeshDatasetGroupProvidedWidget: public QWidget, private Ui::QgsMeshDatasetGroupProvidedWidgetBase
{
    Q_OBJECT
  public:
    QgsMeshDatasetGroupProvidedWidget( QWidget *parent = nullptr );

    //! Synchronize widgets state with associated mesh layer
    void syncToLayer( QgsMeshLayer *meshLayer );

    QMap<int, QgsMeshDatasetGroupState> datasetGroupStates() const;

  signals:
    void datasetGroupAdded();

  private slots:
    void addDataset();

  private:
    QgsMeshLayer *mMeshLayer;
};

#endif // QGSMESHDATASETGROUPPROVIDEDWIDGET_H
