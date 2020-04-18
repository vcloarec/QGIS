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


#include "qgis.h"
#include "qgscolorrampshader.h"
#include "qgsunittypes.h"

/**
 * \ingroup core
 *
 * Class for coloring vector datasets
 *
 * \note not available in Python bindings
 * \since QGIS 3.14
 */

class CORE_EXPORT QgsMeshStrokeColoring
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
    QgsMeshStrokeColoring() = default;
    //! Constructor of a stroke with varying color depending on magnitude
    QgsMeshStrokeColoring( const QgsColorRampShader &colorRampShader );
    //! Constructor of a stroke with fixed color
    QgsMeshStrokeColoring( const QColor &color );

    //! Sets the color ramp to define the coloring
    void setColor( const QgsColorRampShader &colorRampShader );

    //! Sets the single color to define the coloring
    void setColor( const QColor &color );

    //! Returns the color corresponding to the magnitude
    QColor color( double magnitude ) const;

    //! Returns the coloring method used
    QgsMeshStrokeColoring::ColoringMethod coloringMethod() const;

    //! Returns the color ramp shader
    QgsColorRampShader colorRampShader() const;

  private:
    QgsColorRampShader mColorRampShader;
    QColor mSingleColor = Qt::black;

    QgsMeshStrokeColoring::ColoringMethod mColoringMethod = SingleColor;
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

  private:
    bool mIsWidthVarying = false;

    double mFixedWidth = DEFAULT_LINE_WIDTH;

    double mMinimumValue = 0;
    double mMaximumValue = 10;
    double mMinimumWidth = DEFAULT_LINE_WIDTH;
    double mMaximumWidth = 10;
    bool mIgnoreOutOfRange = false;

    double mLinearCoef = 1;
    void updateLinearFormula();
};

class CORE_EXPORT QgsMeshStrokePen
{
  public:
    //QgsMeshStrokePen()=default;

    QgsMeshStrokeWidth strokeWidth() const;
    void setStrokeWidth( const QgsMeshStrokeWidth &strokeWidth );

    QgsMeshStrokeColoring strokeColoring() const;
    void setStrokeColoring( const QgsMeshStrokeColoring &strokeColoring );

    QgsUnitTypes::RenderUnit strokeWidthUnit() const;
    void setStrokeWidthUnit( const QgsUnitTypes::RenderUnit &strokeWidthUnit );

  private:
    QgsMeshStrokeWidth mStrokeWidth;
    QgsMeshStrokeColoring mStrokeColoring;
    QgsUnitTypes::RenderUnit mStrokeWidthUnit = QgsUnitTypes::RenderMillimeters;
};

#endif // QGSMESHSTROKEPEN_H
