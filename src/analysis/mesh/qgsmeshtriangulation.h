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

#ifndef SIP_RUN

/**
 * \ingroup analysis
 * \class QgsMeshZValueDataset
 *
 * Convenient class that can be used to obtain a dataset on vertices that represents the Z value of the mesh vertices
 *
 * \since QGIS 3.16
 */
class QgsMeshZValueDataset: public QgsMeshDataset
{
  public:
    //! Constructor with the mesh
    QgsMeshZValueDataset( const QgsMesh &mesh );

    QgsMeshDatasetValue datasetValue( int valueIndex ) const override;
    QgsMeshDataBlock datasetValues( bool isScalar, int valueIndex, int count ) const override;
    QgsMeshDataBlock areFacesActive( int faceIndex, int count ) const override;
    bool isActive( int faceIndex ) const override;
    QgsMeshDatasetMetadata metadata() const override;
    int valuesCount() const override;

  private:
    QgsMesh mMesh;
    double mZMinimum = std::numeric_limits<double>::max();
    double mZMaximum = -std::numeric_limits<double>::max();
};

#endif //SIP_RUN

/**
 * \ingroup analysis
 * \class QgsMeshZValueDataset
 *
 * Convenient class that can be used to obtain a datasetgroup on vertices that represents the Z value of the mesh vertices
 *
 * \since QGIS 3.16
 */
class ANALYSIS_EXPORT QgsMeshZValueDatasetGroup: public QgsMeshDatasetGroup
{
  public:
    //! Constructor with the mesh
    QgsMeshZValueDatasetGroup( const QgsMesh &mesh );

    void initialize() override;
    QgsMeshDatasetMetadata datasetMetadata( int datasetIndex ) const override;
    int datasetCount() const override;
    QgsMeshDataset *dataset( int index ) const override;
    QgsMeshDatasetGroup::Type type() const override {return QgsMeshDatasetGroup::Virtual;}
    QDomElement writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const override;

  private:
#ifdef SIP_RUN
    QgsMeshZValueDatasetGroup( const QgsMeshZValueDatasetGroup &rhs );
#endif
    std::unique_ptr<QgsMeshZValueDataset> mDataset;

};

#endif // QGSMESHTRIANGULATION_H
