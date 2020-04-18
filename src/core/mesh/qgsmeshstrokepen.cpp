/***************************************************************************
                         qgsmeshstrokepen.cpp
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


#include "qgsmeshstrokepen.h"


QgsMeshStrokeColoring QgsMeshStrokePen::strokeColoring() const
{
  return mStrokeColoring;
}

QgsMeshStrokeWidth QgsMeshStrokePen::strokeWidth() const
{
  return mStrokeWidth;
}

void QgsMeshStrokePen::setStrokeWidth( const QgsMeshStrokeWidth &strokeWidth )
{
  mStrokeWidth = strokeWidth;
}

void QgsMeshStrokePen::setStrokeColoring( const QgsMeshStrokeColoring &strokeColoring )
{
  mStrokeColoring = strokeColoring;
}

QgsUnitTypes::RenderUnit QgsMeshStrokePen::strokeWidthUnit() const
{
  return mStrokeWidthUnit;
}

void QgsMeshStrokePen::setStrokeWidthUnit( const QgsUnitTypes::RenderUnit &strokeWidthUnit )
{
  mStrokeWidthUnit = strokeWidthUnit;
}

double QgsMeshStrokeWidth::minimumValue() const
{
  return mMinimumValue;
}

void QgsMeshStrokeWidth::setMinimumValue( double minimumValue )
{
  mMinimumValue = minimumValue;
  updateLinearFormula();
}

double QgsMeshStrokeWidth::maximumValue() const
{
  return mMaximumValue;
}

void QgsMeshStrokeWidth::setMaximumValue( double maximumValue )
{
  mMaximumValue = maximumValue;
  updateLinearFormula();
}

double QgsMeshStrokeWidth::minimumWidth() const
{
  return mMinimumWidth;
}

void QgsMeshStrokeWidth::setMinimumWidth( double minimumWidth )
{
  mMinimumWidth = minimumWidth;
  updateLinearFormula();
}

double QgsMeshStrokeWidth::maximumWidth() const
{
  return mMaximumWidth;
}

void QgsMeshStrokeWidth::setMaximumWidth( double maximumWidth )
{
  mMaximumWidth = maximumWidth;
  updateLinearFormula();
}

double QgsMeshStrokeWidth::strokeWidth( double value ) const
{
  if ( mIsWidthVarying )
  {
    if ( value > mMaximumValue )
    {
      if ( mIgnoreOutOfRange )
        return std::numeric_limits<double>::quiet_NaN();
      else
        return mMaximumWidth;
    }

    if ( value < mMinimumValue )
    {
      if ( mIgnoreOutOfRange )
        return std::numeric_limits<double>::quiet_NaN();
      else
        return mMinimumWidth;
    }

    return ( value - mMinimumValue ) * mLinearCoef + mMinimumWidth;
  }
  else
    return fixedStrokeWidth();
}

double QgsMeshStrokeWidth::fixedStrokeWidth() const
{
  return mFixedWidth;
}

bool QgsMeshStrokeWidth::ignoreOutOfRange() const
{
  return mIgnoreOutOfRange;
}

void QgsMeshStrokeWidth::setIgnoreOutOfRange( bool ignoreOutOfRange )
{
  mIgnoreOutOfRange = ignoreOutOfRange;
}

bool QgsMeshStrokeWidth::isWidthVarying() const
{
  return mIsWidthVarying;
}

void QgsMeshStrokeWidth::setIsWidthVarying( bool isWidthVarying )
{
  mIsWidthVarying = isWidthVarying;
}

void QgsMeshStrokeWidth::setFixedStrokeWidth( double fixedWidth )
{
  mFixedWidth = fixedWidth;
}

void QgsMeshStrokeWidth::updateLinearFormula()
{
  if ( mMaximumWidth - mMinimumWidth != 0 )
    mLinearCoef = 1 / ( mMaximumValue - mMinimumValue ) * ( mMaximumWidth - mMinimumWidth );
  else
    mLinearCoef = 0;
}

QgsMeshStrokeColoring::QgsMeshStrokeColoring( const QgsColorRampShader &colorRampShader )
{
  setColor( colorRampShader );
}

QgsMeshStrokeColoring::QgsMeshStrokeColoring( const QColor &color )
{
  setColor( color );
  mColoringMethod = SingleColor;
}

void QgsMeshStrokeColoring::setColor( const QgsColorRampShader &colorRampShader )
{
  mColorRampShader = colorRampShader;
  if ( ( mColorRampShader.sourceColorRamp() ) )
    mColoringMethod = ColorRamp;
  else
    mColoringMethod = SingleColor;
}

void QgsMeshStrokeColoring::setColor( const QColor &color )
{
  mColorRampShader = QgsColorRampShader();
  mSingleColor = color;
}

QColor QgsMeshStrokeColoring::color( double magnitude ) const
{
  if ( mColorRampShader.sourceColorRamp() )
  {
    if ( mColorRampShader.isEmpty() )
      return mColorRampShader.sourceColorRamp()->color( 0 );

    int r, g, b, a;
    if ( mColorRampShader.shade( magnitude, &r, &g, &b, &a ) )
      return QColor( r, g, b, a );
    else
      return QColor( 0, 0, 0, 0 );
  }
  else
  {
    return mSingleColor;
  }
}

QgsMeshStrokeColoring::ColoringMethod QgsMeshStrokeColoring::coloringMethod() const
{
  return mColoringMethod;
}

QgsColorRampShader QgsMeshStrokeColoring::colorRampShader() const
{
  return mColorRampShader;
}
