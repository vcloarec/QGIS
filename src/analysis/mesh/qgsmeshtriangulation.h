/***************************************************************************
                          qgsmeshtriangulation.h
                          -----------------
    begin                : August 9th, 2020
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
#ifndef QGSMESHTRIANGULATION_H
#define QGSMESHTRIANGULATION_H

#include "qgscoordinatereferencesystem.h"
#include "qgsmeshdataprovider.h"

#include "qgis_analysis.h"

class QgsVectorLayer;
class QgsCoordinateTransformContext;
class QgsFeature;
class QgsTriangulation;
class QgsFeedback;


/**
 * \ingroup analysis
 * \class QgsMeshTriangulation
 *
 * Class that handles mesh creation with Delaunay constrained triangulation
 *
 * \since QGIS 3.16
 */
class ANALYSIS_EXPORT QgsMeshTriangulation : public QObject
{
    Q_OBJECT
  public:

    //! Contructor
    QgsMeshTriangulation();

    //! Destructor
    ~QgsMeshTriangulation();

    /**
     * Adds vertices from a vector layer, return true if success
     *
     * \param vectorLayer the vector layer with vertices to insert
     * \param valueAttribute the index of the attribute that represents the value of vertices, if -1 uses Z coordinate of vertices
     * \param transformContext the transform context used to transform coordinates
     */
    bool addVertices( QgsVectorLayer *vectorLayer, int valueAttribute, const QgsCoordinateTransformContext &transformContext, QgsFeedback *feedback = nullptr );

    /**
     * Adds break lines from a vector layer, return true if success
     * \param vectorLayer the vector layer with vertices to insert
     * \param valueAttribute the index of the attribute that represents the value of vertices, if -1 uses Z coordinate of vertices
     * \param transformContext the transform context used to transform coordinates
     *
     * \note if the vector layer contain point, only vertices will be added without breaklines
     */
    bool addBreakLines( QgsVectorLayer *linesSource, int valueAttribute, const QgsCoordinateTransformContext &transformContext, QgsFeedback *feedback = nullptr );

    //! Returns the triangulated mesh
    QgsMesh triangulatedMesh() const;

    //! Sets the coordinate reference system for the triangulation
    void setCrs( const QgsCoordinateReferenceSystem &crs );

  private:
#ifdef SIP_RUN
    QgsMeshTriangulation( const QgsMeshTriangulation &rhs );
#endif

    QgsCoordinateReferenceSystem mCrs;
    std::unique_ptr<QgsTriangulation> mTriangulation;

    void addBreakLinesFromFeature( const QgsFeature &feature, int valueAttribute, const QgsCoordinateTransform &transform, QgsFeedback *feedback = nullptr );
};

#endif // QGSMESHTRIANGULATION_H
