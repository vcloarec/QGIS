/***************************************************************************
                         qgsmeshsimplifysettings.h
                         ---------------------
    begin                : February 2020
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

#ifndef QGSMESHSIMPLIFYSETTINGS_H
#define QGSMESHSIMPLIFYSETTINGS_H

#include <QDomElement>

#include "qgis_core.h"
#include "qgis.h"

/**
 * \ingroup core
 *
 * Represents a overview renderer settings
 *
 * \note The API is considered EXPERIMENTAL and can be changed without a notice
 *
 * \since QGIS 3.14
 */

class CORE_EXPORT QgsMeshSimplifySettings
{
  public:
    //! Returns if the overview is active
    bool isEnabled() const;
    //! Sets if the overview is active
    void setEnabled( bool isEnabled );

    /**
     * Returns the reduction factor used to build simplified mesh.
     */
    double reductionFactor() const;

    /**
     * Sets the reduction factor used to build simplified mesh.
     * The triangles count of the simplified mesh equals apromativly the triangles count of base mesh divised by this factor.
     * This reduction factor is used for simplification of each successive simplified mesh. For  example, if the base mesh has 5M faces,
     * and the reduction factor is 10, the first simplified mesh will have approximativly 500 000 faces, the second 50 000 faces,
     * the third 5000, ...
     * If highter reduction factor leads to simpler meshes, it produces also fewer levels of detail.
     * The reduction factor has to be strictly greater than 1. If not, the simplification processus will render nothing.
     */
    void setReductionFactor( double value );

    //! Writes configuration to a new DOM element
    QDomElement writeXml( QDomDocument &doc ) const;
    //! Reads configuration from the given DOM element
    void readXml( const QDomElement &elem );

    //! Returns the mesh resolution e.i., the minimum size (average) of triangles in pixels
    int meshResolution() const;

    /**
     * Sets the mesh resolution e.i., the minimum size (average) of triangles in pixels
     * This value is used during map rendering to choose the most appropriate mesh from he list of simplified mesh.
     * The first mesh which has its average triangle size greater this value will be chosen.
     */
    void setMeshResolution( int meshResolution );

  private:
    bool mEnabled = false;
    double mReductionFactor = 10;
    int mMeshResolution = 5;
};
#endif // QGSMESHSIMPLIFYSETTINGS_H
