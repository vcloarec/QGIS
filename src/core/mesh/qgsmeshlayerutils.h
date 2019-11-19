/***************************************************************************
                         qgsmeshlayerutils.h
                         --------------------------
    begin                : August 2018
    copyright            : (C) 2018 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMESHLAYERUTILS_H
#define QGSMESHLAYERUTILS_H

#define SIP_NO_FILE

#include "qgis_core.h"
#include "qgsrectangle.h"
#include "qgsmaptopixel.h"
#include "qgsmeshdataprovider.h"
#include "qgsmeshrenderersettings.h"

#include <QVector>
#include <QSize>

///@cond PRIVATE

class QgsMeshTimeSettings;
class QgsTriangularMesh;
class QgsMeshDataBlock;

/**
 * \ingroup core
 * Misc utility functions used for mesh layer support
 *
 * \note not available in Python bindings
 * \since QGIS 3.4
 */
class CORE_EXPORT QgsMeshLayerUtils
{
  public:

    /**
     * Calculates magnitude values from the given QgsMeshDataBlock.
     *
     * \since QGIS 3.6
     */
    static QVector<double> calculateMagnitudes( const QgsMeshDataBlock &block );

    /**
     * Transformes the bounding box to rectangle in screen coordinates (in pixels)
     * \param mtp actual renderer map to pixel
     * \param outputSize actual renderer output size
     * \param bbox bounding box in map coordinates
     * \param leftLim minimum x coordinate in pixel
     * \param rightLim maximum x coordinate in pixel
     * \param topLim minimum y coordinate in pixel
     * \param bottomLim maximum y coordinate in pixel
     */
    static void boundingBoxToScreenRectangle(
      const QgsMapToPixel &mtp,
      const QSize &outputSize,
      const QgsRectangle &bbox,
      int &leftLim, int &rightLim, int &topLim, int &bottomLim );

    /**
    * Interpolates value based on known values on the vertices of a triangle
    * \param p1 first vertex of the triangle
    * \param p2 second vertex of the triangle
    * \param p3 third vertex of the triangle
    * \param val1 value on p1 of the triangle
    * \param val2 value on p2 of the triangle
    * \param val3 value on p3 of the triangle
    * \param pt point where to calculate value
    * \returns value on the point pt or NaN in case the point is outside the triangle
    */
    static double interpolateFromVerticesData(
      const QgsPointXY &p1, const QgsPointXY &p2, const QgsPointXY &p3,
      double val1, double val2, double val3, const QgsPointXY &pt
    );

    /**
    * Interpolates vector based on known vector on the vertices of a triangle
    * \param p1 first vertex of the triangle
    * \param p2 second vertex of the triangle
    * \param p3 third vertex of the triangle
    * \param vect1 value on p1 of the triangle
    * \param vect2 value on p2 of the triangle
    * \param vect3 value on p3 of the triangle
    * \param pt point where to calculate value
    * \returns vector on the point pt or NaN in case the point is outside the triangle
    */
    static QgsVector interpolateVectorFromVerticesData(
      const QgsPointXY &p1, const QgsPointXY &p2, const QgsPointXY &p3,
      QgsVector vect1, QgsVector vect2, QgsVector vect3, const QgsPointXY &pt
    );

    /**
    * Interpolate value based on known value on the face of a triangle
    * \param p1 first vertex of the triangle
    * \param p2 second vertex of the triangle
    * \param p3 third vertex of the triangle
    * \param val face value
    * \param pt point where to calculate value
    * \returns value on the point pt or NaN in case the point is outside the triangle
    */
    static double interpolateFromFacesData(
      const QgsPointXY &p1, const QgsPointXY &p2, const QgsPointXY &p3,
      double val, const QgsPointXY &pt );

    /**
    * Interpolate value based on known value on the face of a triangle
    * \param p1 first vertex of the triangle
    * \param p2 second vertex of the triangle
    * \param p3 third vertex of the triangle
    * \param vect face vector
    * \param pt point where to calculate value
    * \returns vector on the point pt or NaN in case the point is outside the triangle
    */
    static QgsVector interpolateVectorFromFacesData(
      const QgsPointXY &p1, const QgsPointXY &p2, const QgsPointXY &p3,
      QgsVector vect, const QgsPointXY &pt );

    /**
    * Interpolate values on vertices from values on faces
    *
    * \since QGIS 3.12
    */
    static QVector<double> interpolateFromFacesData(
      QVector<double> valuesOnFaces,
      QgsMesh *nativeMesh,
      QgsTriangularMesh *triangularMesh,
      QgsMeshDataBlock *active,
      QgsMeshRendererScalarSettings::DataInterpolationMethod method
    );

    /**
     * Calculates the bounding box of the triangle
     * \param p1 first vertex of the triangle
     * \param p2 second vertex of the triangle
     * \param p3 third vertex of the triangle
     * \returns bounding box of the triangle
     */
    static QgsRectangle triangleBoundingBox( const QgsPointXY &p1, const QgsPointXY &p2, const QgsPointXY &p3 );

    /**
     * Formats hours in human readable string based on settings
     */
    static QString formatTime( double hours, const QgsMeshTimeSettings &settings );
};

///@endcond

#endif // QGSMESHLAYERUTILS_H
