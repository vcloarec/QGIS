/***************************************************************************
  qgsmssqltransaction.h - QgsMssqlTransaction

 ---------------------
 begin                : 11.3.2021
 copyright            : (C) 2021 by Vincent Cloarec
 email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSMSSQLTRANSACTION_H
#define QGSMSSQLTRANSACTION_H

#include <QSqlDatabase>
#include <QEventLoop>
#include <QThread>
#include <QSqlQuery>
#include <QMutex>
#include <QWaitCondition>
#include <QPointer>

#include "qgstransaction.h"
#include "qgsdatasourceuri.h"
#include "qgis_sip.h"

///@cond PRIVATE
#define SIP_NO_FILE

class QgsMssqlTransaction : public QgsTransaction
{
    Q_OBJECT

  public:
    explicit QgsMssqlTransaction( const QString &connString );
    ~ QgsMssqlTransaction();

    /**
     * Executes the SQL query in database.
     *
     * \param sql The SQL query to execute
     * \param error The error or an empty string if none
     * \param isDirty True to add an undo/redo command in the edition buffer, false otherwise
     * \param name Name of the operation ( only used if `isDirty` is true)
     */
    bool executeSql( const QString &sql, QString &error, bool isDirty = false, const QString &name = QString() ) override;
    QString createSavepoint( const QString &savePointId, QString &error ) override;

    QSqlQuery createQuery();

  private:
    bool beginTransaction( QString &error, int ) override;
    bool commitTransaction( QString &error ) override;
    bool rollbackTransaction( QString &error ) override;

    QSqlDatabase mDataBase;
    bool transactionCommand( const QString command, QString &error );

};

class QgsMssqlConnectionWrapper : public QObject
{
    Q_OBJECT
  public:
    QgsMssqlConnectionWrapper( const QgsDataSourceUri &uri ) :
      QObject()
      , mUri( uri )
    {}

    QVariant queryResultValue( const QString &uid, int index ) const;
    bool isQuerySuccessful( const QString &uid ) const;
    bool isQueryActive( const QString &uid ) const;

  public slots:
    void createConnection();

    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    void createQuery( const QString &uid );
    void removeQuery( const QString &uid );
    void executeQuery( const QString &uid, const QString &query );

  private:

    QSqlDatabase mDatabaseConnection;
    QgsDataSourceUri mUri;
    QMap<QString, QSharedPointer<QSqlQuery>> mQueries;
    QMap<QString, bool> mQueriesSuccesful;
    mutable QMutex mMutex;
    bool mIsTransactionActive;
};


class QgsMssqlThreadSafeConnection : public QObject
{
  public:
    QgsMssqlThreadSafeConnection( const QgsDataSourceUri &uri );
    ~QgsMssqlThreadSafeConnection();

    class Query
    {
      public:
        Query( QgsMssqlThreadSafeConnection *connection );
        ~Query();

        bool exec( const QString query );
        bool isActive();

      private:
        QString mUid;
        QPointer<QgsMssqlThreadSafeConnection> mConnection = nullptr;

        friend class QgsMssqlThreadSafeConnection;
    };

    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    //! Creates a query associated to the connection
    Query createQuery();

  private:
    QgsMssqlConnectionWrapper *mConnection = nullptr;
    QThread *mTransactionThread;

    QString createQueryPrivate();
    void removeQueryPrivate( const QString &queryId );
    bool isQuerySucessfull( const QString &queryId );
    void executeQuery( const QString &query, const QString &uid );
    bool queryIsActive( const QString &queryId ) const;

    friend class Query;
};



///@endcond

#endif // QGSMSSQLTRANSACTION_H
