/***************************************************************************
                         qgsmeshdatagenerator.h
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

#ifndef QGSMESHGENERATOR_H
#define QGSMESHGENERATOR_H

#include <QString>
#include <QMap>

#include "qgis_core.h"

#define SIP_NO_FILE

class QgsMeshDatasetGroup;
class QgsMeshLayer;

/**
 * \ingroup core
 *
 * Abstract class that is an interface for mesh generator
 *
 * Mesh generator can be used to create mesh data
 *
 * \since QGIS 3.16
 */
class CORE_EXPORT QgsMeshDataGeneratorInterface
{
  public:
    QgsMeshDataGeneratorInterface();
    virtual ~QgsMeshDataGeneratorInterface();

    //! Creates a new dataset group from a expresion. The caller takes the ownership of this instance
    virtual QgsMeshDatasetGroup *createDatasetGroupFromExpression( const QString &name,
        const QString &formulaString,
        QgsMeshLayer *layer,
        qint64 relativeStartTime,
        qint64 relativeEndTime ) = 0;

    //! Returns the key of the mesh calculator
    virtual QString key() const = 0;
};

/**
 * \ingroup core
 *
 * Class that store mesh generator
 *
 * \since QGIS 3.16
 */
class CORE_EXPORT QgsMeshDataGeneratorRegistry
{
  public:
    //! Constructor
    QgsMeshDataGeneratorRegistry() = default;

    //!Destructor
    ~QgsMeshDataGeneratorRegistry();

    //! Add a mesh calculator factory in the registry
    void addMeshDataGenerator( QgsMeshDataGeneratorInterface *generator );

    //! Returns the mesh calculator corresonding to the \a key
    QgsMeshDataGeneratorInterface *meshDataGenerator( const QString &key ) const;
  private:
    QMap<QString, QgsMeshDataGeneratorInterface *> mMeshDataGenerators;

};
#endif // QGSMESHGENERATOR_H
