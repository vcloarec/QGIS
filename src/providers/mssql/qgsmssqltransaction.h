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
#include "qgsmssqldatabase.h"

///@cond PRIVATE
#define SIP_NO_FILE

/**
 * /////////////  Locking mechanism of MSSQL
 * MMSQL provide an option that define how transactions are isolated from each other, READ_COMMITTED_SNAPSHOT = {ON,OFF}
 * If this option is OFF, value by default, if a transaction had modified somes rows, these rows are locked unitl the transaction is finished.
 * Whith this locking, access to this rows by another connection are "freezed" until the end of the connection.
 *
 * If READ_COMMITTED_SNAPSHOT = ON, acces to the row is possible during transaction, but the rows are read in the state just before the last commit,
 * so access to the modified row by another connection is not freezed.
 *
 * The option READ_COMMITTED_SNAPSHOT is a database setting, so QGIS could not change this option.
 *
 * In the worth case, if the main thread use a connection to access to a database with a transaction opened, QGIS freezes until this transaction finished.
 * If this transaction is also handle in the main trhead, this freezing never finishes
 *
 * In the better case, the other connection is not in the main thread and will not freez QGIS. But his job is blocked until the transaction is finished
 * (never if the transaction is handle in the same thread).
 *
 * The risk of freezing the main thread is not acceptable, and freezing, even temporarly, other thread will provide a bad user experience.
 *
 * The solution to avoid this freezing is, when a transaction is open for a connection,
 * All request (from every where in QGIS) for connection with the same connection uri HAVE TO return the connection where the transaction has begun.
 *
 *
 * /////////////  QSqlDatabase can not be shared between threads
 *
 * https://doc.qt.io/qt-5/threads-modules.html#threads-and-the-sql-module
 *
 * "A connection can only be used from within the thread that created it.
 * Moving connections between threads or creating queries from a different thread is not supported."
 *
 * So, it not possible to share a transaction between threads. For example, if the connection is created in the main thread, it is not possible to use it in the renderer
 *
 * --> We CAN'T USE DIRECTLY QsqlDataBase to share connection with transaction between thread.
 *
 * To allow transaction with MSSQL, it is necessary to implement a connection object that will be created in a independant thread,
 * and all other thread will used it with thread safe function and without using directly QSqlQuery or QsqlDatabase outside the independant thread.
 *
 * As this new object can not be handle by the native QSqlDataBase pool, we need to implement another mechanism to store the connection with transaction in a singleton.
 *
 */

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
    bool rollbackToSavepoint( const QString &name, QString &error SIP_OUT ) override;

  private:
    bool beginTransaction( QString &error, int ) override;
    bool commitTransaction( QString &error ) override;
    bool rollbackTransaction( QString &error ) override;

    QgsMssqlDatabase mDataBase;
    bool transactionCommand( const QString command, QString &error );
};





///@endcond

#endif // QGSMSSQLTRANSACTION_H
