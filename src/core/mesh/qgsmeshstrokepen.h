/***************************************************************************
                         qgsmeshstrokepen.h
                         ---------------------
    begin                : April 2020
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

#ifndef QGSMESHSTROKEPEN_H
#define QGSMESHSTROKEPEN_H

#include <QDebug>

#include "qgis.h"
#include "qgscolorrampshader.h"
#include "qgsreadwritecontext.h"
#include "qgsrendercontext.h"
#include "qgsunittypes.h"

/**
 * \ingroup core
 *
 * Class for coloring vector datasets
 *
 * \note not available in Python bindings
 * \since QGIS 3.14
 */

class CORE_EXPORT QgsMeshStrokeColor
{
  public:
    /**
     * Defines how the color of vector is defined
     * \since QGIS 3.14
     */
    enum ColoringMethod
    {
      //! Render the vector with a single color
      SingleColor = 0,
      //! Render the vector with a color ramp
      ColorRamp
    };

    //! Default constructor
    QgsMeshStrokeColor() = default;
    //! Constructor of a stroke with varying color depending on magnitude
    QgsMeshStrokeColor( const QgsColorRampShader &colorRampShader );
    //! Constructor of a stroke with fixed color
    QgsMeshStrokeColor( const QColor &color );

    //! Sets the color ramp to define the coloring
    void setColor( const QgsColorRampShader &colorRampShader );

    //! Sets the single color to define the coloring
    void setColor( const QColor &color );

    //! Returns the color corresponding to the magnitude
    QColor color( double magnitude ) const;

    //! Returns the coloring method used
    QgsMeshStrokeColor::ColoringMethod coloringMethod() const;

    //! Returns the color ramp shader
    QgsColorRampShader colorRampShader() const;

    //! Writes configuration to a new DOM element
    QDomElement writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const;
    //! Reads configuration from the given DOM element
    void readXml( const QDomElement &elem, const QgsReadWriteContext &context );

    /**
     *  Returns the break values, graduated colors and the associated gradient between two values
     *  - If the color is fixed, returns only one interval (value1, value2) for  \a breakVlaues, \a breakColors (singlecolor,singleColor)
     *  and a gradient with single color (uniform with the single color)
     *  - If the color ramp is classified with 'exact', returns void \a gradrients
     *  - If the color ramp is classified with 'discrete', return \a gradients with uniform colors
     */
    void graduatedColors( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients );

  private:
    QgsColorRampShader mColorRampShader;
    QColor mSingleColor = Qt::black;

    QgsMeshStrokeColor::ColoringMethod mColoringMethod = SingleColor;

    QLinearGradient makeSimpleLinearGradient( const QColor &color1, const QColor &color2, bool invert ) const;

    void graduatedColorsExact( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const;
    void graduatedColorsInterpolated(double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert) const;
    void graduatedColorsDiscret(double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert) const;
};

class CORE_EXPORT QgsMeshStrokeWidth
{
  public:
    //! Returns the minimum value used to defined the varying stroke width
    double minimumValue() const;
    //! Sets the minimum value used to defined the varying stroke width
    void setMinimumValue( double minimumValue );

    //! Returns the maximum value used to defined the varying stroke width
    double maximumValue() const;
    //! Sets the maximum value used to defined the varying stroke width
    void setMaximumValue( double maximumValue );

    //! Returns the minimum width used to defined the varying stroke width
    double minimumWidth() const;
    //! Sets the minimum width used to defined the varying stroke width
    void setMinimumWidth( double minimumWidth );

    //! Returns the maximum width used to defined the varying stroke width
    double maximumWidth() const;
    //! Sets the maximum width used to defined the varying stroke width
    void setMaximumWidth( double maximumWidth );

    //! Returns whether the varying stroke width ignores out of range value
    bool ignoreOutOfRange() const;
    //! Sets whether the varying stroke width ignores out of range value
    void setIgnoreOutOfRange( bool ignoreOutOfRange );

    //! Returns whether the stroke width is varying
    bool isWidthVarying() const;
    //! Returns whether the stroke width is varying
    void setIsWidthVarying( bool isWidthVarying );

    //! Returns the fixed stroke width
    double fixedStrokeWidth() const;
    //! Sets the fixed stroke width
    void setFixedStrokeWidth( double fixedWidth );

    //! Returns the varying stroke width depending on value, if not varying returns the fixed stroke width
    double strokeWidth( double value ) const;

    //! Writes configuration to a new DOM element
    QDomElement writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const;
    //! Reads configuration from the given DOM element
    void readXml( const QDomElement &elem, const QgsReadWriteContext &context );

  private:
    bool mIsWidthVarying = false;

    double mFixedWidth = DEFAULT_LINE_WIDTH;

    double mMinimumValue = 0;
    double mMaximumValue = 10;
    double mMinimumWidth = DEFAULT_LINE_WIDTH;
    double mMaximumWidth = 10;
    bool mIgnoreOutOfRange = false;

    mutable double mLinearCoef = 1;
    mutable bool mNeedUpdateFormula = true;
    void updateLinearFormula() const;
};

class CORE_EXPORT QgsMeshStrokePen
{
  public:
    QgsMeshStrokeWidth strokeWidth() const;
    void setStrokeWidth( const QgsMeshStrokeWidth &strokeWidth );

    QgsMeshStrokeColor strokeColoring() const;
    void setStrokeColoring( const QgsMeshStrokeColor &strokeColoring );

    QgsUnitTypes::RenderUnit strokeWidthUnit() const;
    void setStrokeWidthUnit( const QgsUnitTypes::RenderUnit &strokeWidthUnit );

    //! Writes configuration to a new DOM element
    QDomElement writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const;
    //! Reads configuration from the given DOM element
    void readXml( const QDomElement &elem, const QgsReadWriteContext &context );

    void drawLine( double value1, double value2, QgsPointXY point1, QgsPointXY point2, QgsRenderContext &context ) const;

  private:
    QgsMeshStrokeWidth mStrokeWidth;
    QgsMeshStrokeColor mStrokeColoring;
    QgsUnitTypes::RenderUnit mStrokeWidthUnit = QgsUnitTypes::RenderMillimeters;

    QPolygonF varyingWidthLine( double value1, double value2, QPointF point1, QPointF point2, QgsRenderContext &context ) const;
};

#endif // QGSMESHSTROKEPEN_H
