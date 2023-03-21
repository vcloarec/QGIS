/***************************************************************************
  qgsrunnableprovidercreator.h - QgsRunnableProviderCreator

 ---------------------
 begin                : 20.3.2023
 copyright            : (C) 2023 by Vincent Cloarec
 email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSRUNNABLEPROVIDERCREATOR_H
#define QGSRUNNABLEPROVIDERCREATOR_H

#include <QRunnable>

#include <qgsdataprovider.h>

class QgsRunnableProviderCreator : public QRunnable
{
  public:
    QgsRunnableProviderCreator( const QString &layerId,
                                QString const &providerKey,
                                QString const &dataSource,
                                const QgsDataProvider::ProviderOptions &options,
                                QgsDataProvider::ReadFlags flags );

    void run() override;

    /**
     * Returns the created data provider.
     * Caller takes ownership and as the dataprovider object was moved to the thread
     * where the QgsRunnableProviderCreator was created, the caller has to be in this thread.
     */
    QgsDataProvider *dataProvider();

    //! Returns the layer id \a layerId corresponding to the layer associated to the created provider.
    QString layerId() const;

  private:
    std::unique_ptr<QgsDataProvider> mDataProvider;
    QString mLayerId;
    QString mProviderKey;
    QString mDataSource;
    QgsDataProvider::ProviderOptions mOptions;
    QgsDataProvider::ReadFlags mFlags;
    QThread *mOriginThread = nullptr;
};

#endif // QGSRUNNABLEPROVIDERCREATOR_H
