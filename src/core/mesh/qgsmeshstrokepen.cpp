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
#include "qgssymbollayerutils.h"


QgsMeshStrokeColor QgsMeshStrokePen::strokeColoring() const
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

void QgsMeshStrokePen::setStrokeColoring( const QgsMeshStrokeColor &strokeColoring )
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

QDomElement QgsMeshStrokePen::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context );

  QDomElement elem = doc.createElement( QStringLiteral( "mesh-stroke-pen" ) );

  QDomElement widthElement = mStrokeWidth.writeXml( doc, context );
  elem.appendChild( widthElement );
  QDomElement colorElement = mStrokeColoring.writeXml( doc, context );
  elem.appendChild( colorElement );

  elem.setAttribute( QStringLiteral( "stroke-width-unit" ), QgsUnitTypes::encodeUnit( mStrokeWidthUnit ) );

  return elem;
}

void QgsMeshStrokePen::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  QDomElement elemWidth = elem.firstChildElement( QStringLiteral( "mesh-stroke-width" ) );
  mStrokeWidth.readXml( elemWidth, context );

  QDomElement elemColoring = elem.firstChildElement( QStringLiteral( "mesh-stroke-coloring" ) );
  mStrokeColoring.readXml( elemColoring, context );

  mStrokeWidthUnit = QgsUnitTypes::decodeRenderUnit( elem.attribute( QStringLiteral( "stroke-width-unit" ) ) );
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

QDomElement QgsMeshStrokeWidth::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context );

  QDomElement elem = doc.createElement( QStringLiteral( "mesh-stroke-width" ) );

  elem.setAttribute( QStringLiteral( "width-varying" ), mIsWidthVarying ? 1 : 0 );
  elem.setAttribute( QStringLiteral( "fixed-width" ), mFixedWidth );
  elem.setAttribute( QStringLiteral( "minimum-value" ), mMinimumValue );
  elem.setAttribute( QStringLiteral( "maximum-value" ), mMaximumValue );
  elem.setAttribute( QStringLiteral( "minimum-width" ), mMinimumWidth );
  elem.setAttribute( QStringLiteral( "maximum-width" ), mMaximumWidth );
  elem.setAttribute( QStringLiteral( "ignore-out-of-range" ), mIgnoreOutOfRange ? 1 : 0 );

  return elem;
}

void QgsMeshStrokeWidth::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  Q_UNUSED( context );

  mIsWidthVarying = elem.attribute( QStringLiteral( "width-varying" ) ).toInt();
  mFixedWidth = elem.attribute( QStringLiteral( "fixed-width" ) ).toDouble();
  mMinimumValue = elem.attribute( QStringLiteral( "minimum-value" ) ).toDouble();
  mMaximumValue = elem.attribute( QStringLiteral( "maximum-value" ) ).toDouble();
  mMinimumWidth = elem.attribute( QStringLiteral( "minimum-width" ) ).toDouble();
  mMaximumWidth = elem.attribute( QStringLiteral( "maximum-width" ) ).toDouble();
  mIgnoreOutOfRange = elem.attribute( QStringLiteral( "ignore-out-of-range" ) ).toInt();
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
    mLinearCoef = ( mMaximumWidth - mMinimumWidth ) / ( mMaximumValue - mMinimumValue ) ;
  else
    mLinearCoef = 0;
}

QgsMeshStrokeColor::QgsMeshStrokeColor( const QgsColorRampShader &colorRampShader )
{
  setColor( colorRampShader );
}

QgsMeshStrokeColor::QgsMeshStrokeColor( const QColor &color )
{
  setColor( color );
  mColoringMethod = SingleColor;
}

void QgsMeshStrokeColor::setColor( const QgsColorRampShader &colorRampShader )
{
  mColorRampShader = colorRampShader;
  if ( ( mColorRampShader.sourceColorRamp() ) )
    mColoringMethod = ColorRamp;
  else
    mColoringMethod = SingleColor;
}

void QgsMeshStrokeColor::setColor( const QColor &color )
{
  mColorRampShader = QgsColorRampShader();
  mSingleColor = color;
}

QColor QgsMeshStrokeColor::color( double magnitude ) const
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

QgsMeshStrokeColor::ColoringMethod QgsMeshStrokeColor::coloringMethod() const
{
  return mColoringMethod;
}

QgsColorRampShader QgsMeshStrokeColor::colorRampShader() const
{
  return mColorRampShader;
}

QDomElement QgsMeshStrokeColor::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context );

  QDomElement elem = doc.createElement( QStringLiteral( "mesh-stroke-color" ) );

  elem.setAttribute( QStringLiteral( "single-color" ), QgsSymbolLayerUtils::encodeColor( mSingleColor ) );
  elem.setAttribute( QStringLiteral( "coloring-method" ), mColoringMethod );
  elem.appendChild( mColorRampShader.writeXml( doc ) );

  return elem;
}

void QgsMeshStrokeColor::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  Q_UNUSED( context );

  QDomElement shaderElem = elem.firstChildElement( QStringLiteral( "colorrampshader" ) );
  mColorRampShader.readXml( shaderElem );

  mSingleColor = QgsSymbolLayerUtils::decodeColor( elem.attribute( QStringLiteral( "single-color" ) ) );
  mColoringMethod = static_cast<QgsMeshStrokeColor::ColoringMethod>(
                      elem.attribute( QStringLiteral( "coloring-method" ) ).toInt() );
}
