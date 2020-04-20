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

#include <QPainter>

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

  QDomElement elemColoring = elem.firstChildElement( QStringLiteral( "mesh-stroke-color" ) );
  mStrokeColoring.readXml( elemColoring, context );

  mStrokeWidthUnit = QgsUnitTypes::decodeRenderUnit( elem.attribute( QStringLiteral( "stroke-width-unit" ) ) );
}

void QgsMeshStrokePen::drawLine( double value1, double value2, QgsPointXY point1, QgsPointXY point2, QgsRenderContext &context ) const
{
  QPainter *painter = context.painter();
  painter->save();
  if ( context.flags() & QgsRenderContext::Antialiasing )
    painter->setRenderHint( QPainter::Antialiasing, true );
  QPen pen;
  pen.setStyle( Qt::NoPen );
  painter->setPen( pen );
  QBrush brush;
  brush.setStyle( Qt::SolidPattern );

  const QgsMapToPixel &mapToPixel = context.mapToPixel();

  QPointF p1 = mapToPixel.transform( point1 ).toQPointF();
  QPointF p2 = mapToPixel.transform( point2 ).toQPointF();
  QPointF dir = p2 - p1;
  QPointF diru = dir / sqrt( pow( dir.x(), 2 ) + pow( dir.y(), 2 ) );
  QPointF orthu = QPointF( -diru.y(), diru.x() );
  double width1 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value1 ), mStrokeWidthUnit );
  double width2 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value2 ), mStrokeWidthUnit ) ;

  //Draw pen cap

  QRectF capBox1( p1.x() - width1 / 2, p1.y() - width1 / 2, width1, width1 );
  QRectF capBox2( p2.x() - width2 / 2, p2.y() - width2 / 2, width2, width2 );
  int startAngle;
  startAngle = ( acos( -orthu.x() ) / M_PI ) * 180;
  if ( orthu.y() < 0 )
    startAngle = 360 - startAngle;

  brush.setColor( mStrokeColoring.color( value1 ) );
  painter->setBrush( brush );
  painter->drawPie( capBox1, ( startAngle - 1 ) * 16, 182 * 16 );
  brush.setColor( mStrokeColoring.color( value2 ) );
  painter->setBrush( brush );
  painter->drawPie( capBox2, ( startAngle + 179 ) * 16, 182 * 16 );

  QPolygonF varLine;

  double semiWidth1 = width1 / 2;
  double semiWidth2 = width2 / 2;

  varLine.append( p1 + orthu * semiWidth1 );
  varLine.append( p2 + orthu * semiWidth2 );
  varLine.append( p2 - orthu * semiWidth2 );
  varLine.append( p1 - orthu * semiWidth1 );

  painter->drawPolygon( varLine );

  painter->restore();
}

double QgsMeshStrokeWidth::minimumValue() const
{
  return mMinimumValue;
}

void QgsMeshStrokeWidth::setMinimumValue( double minimumValue )
{
  mMinimumValue = minimumValue;
  mNeedUpdateFormula = true;
}

double QgsMeshStrokeWidth::maximumValue() const
{
  return mMaximumValue;
}

void QgsMeshStrokeWidth::setMaximumValue( double maximumValue )
{
  mMaximumValue = maximumValue;
  mNeedUpdateFormula = true;
}

double QgsMeshStrokeWidth::minimumWidth() const
{
  return mMinimumWidth;
}

void QgsMeshStrokeWidth::setMinimumWidth( double minimumWidth )
{
  mMinimumWidth = minimumWidth;
  mNeedUpdateFormula = true;
}

double QgsMeshStrokeWidth::maximumWidth() const
{
  return mMaximumWidth;
}

void QgsMeshStrokeWidth::setMaximumWidth( double maximumWidth )
{
  mMaximumWidth = maximumWidth;
  mNeedUpdateFormula = true;
}

double QgsMeshStrokeWidth::strokeWidth( double value ) const
{
  if ( mIsWidthVarying )
  {
    if ( mNeedUpdateFormula )
      updateLinearFormula();

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

void QgsMeshStrokeWidth::updateLinearFormula() const
{
  if ( mMaximumWidth - mMinimumWidth != 0 )
    mLinearCoef = ( mMaximumWidth - mMinimumWidth ) / ( mMaximumValue - mMinimumValue ) ;
  else
    mLinearCoef = 0;
  mNeedUpdateFormula = false;
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

void QgsMeshStrokeColor::graduatedColors( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients )
{
  breakValues.clear();
  breakColors.clear();
  gradients.clear();
  if ( mColoringMethod == SingleColor )
  {
    breakColors.append( mSingleColor );
    breakValues.append( value1 );
    breakValues.append( value2 );
    breakColors.append( mSingleColor );
    breakColors.append( mSingleColor );
    gradients.append( makeSimpleLinearGradient( mSingleColor, mSingleColor, false ) );
    return;
  }

  double firstvalue = value1;
  double secondValue = value2;
  bool invert = value1 > value2;
  if ( invert )
  {
    firstvalue = value2;
    secondValue = value1;
  }

//  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
//  if ( itemList.isEmpty() )
//    return;

//  if ( ( firstvalue > itemList.last().value ) //out of range with greater value
//       && ( ( mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated && !mColorRampShader.clip() ) ||
//            mColorRampShader.colorRampType() == QgsColorRampShader::Discrete ) )
//  {
//    QColor color = itemList.last().color;
//    breakValues.append( firstvalue );
//    breakValues.append( secondValue );
//    breakColors.append( color );
//    breakColors.append( color );
//    gradients.append( makeSimpleLinearGradient( color, color, invert ) );
//  }

//  if ( ( secondValue < itemList.first().value ) //out of range with lesser value
//       && ( ( mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated && !mColorRampShader.clip() ) ||
//            mColorRampShader.colorRampType() == QgsColorRampShader::Discrete ) )
//  {
//    QColor color = itemList.first().color;
//    breakValues.append( firstvalue );
//    breakValues.append( secondValue );
//    breakColors.append( color );
//    breakColors.append( color );
//    gradients.append( makeSimpleLinearGradient( color, color, invert ) );
//  }

//  int i = 0;
//  //If the first value is out of range (lesser) and Interpolated(not ignore out of range) or Discrete
//  if ( firstvalue < itemList.first().value )

//    if ( ( !mColorRampShader.clip() && //if render out of range
//           mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated ) ||
//         mColorRampShader.colorRampType() == QgsColorRampShader::Discrete )
//    {
//      //add the first value out of range
//      breakValues.append( firstvalue );
//      const QColor firstColor = itemList.first().color;
//      breakColors.append( firstColor );
//      gradients.append( makeSimpleLinearGradient( firstColor, firstColor, invert ) );
//    }
//  {
//      //search the fist interval
//      while ( ( i < itemList.count() ) && ( firstvalue < itemList.at( i ).value ) )
//        i++;

//      //add the first value int the range
//      if ( mColorRampShader.colorRampType() != QgsColorRampShader::Exact )
//      {
//        if ( i < itemList.count() )
//        {
//          int r, g, b, a;
//          QColor firstColor;
//          if ( mColorRampShader.shade( firstvalue, &r, &g, &b, &a ) )
//          {
//            firstColor = QColor( r, g, b, a );
//          }
//          else
//            firstColor = QColor( 0, 0, 0, 0 );


//        }
//      }
//  }

//  while ( i < ( itemList.count() - 1 ) && secondValue > itemList.at( i ).value )
//  {
//    breakValues.append( itemList.at( i ).value );
//    const QColor &firstColor = itemList.at( i ).color;
//    const QColor &secondColor = itemList.at( i + 1 ).color;
//    breakValues.append( itemList.at( i ).value );
//    breakColors.append( firstColor );
//    if ( mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated )
//      gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );
//    if ( mColorRampShader.colorRampType() == QgsColorRampShader::Discrete ) // Gradient with uniform color
//      gradients.append( makeSimpleLinearGradient( secondColor, secondColor, invert ) );
//    i++;
//  }

//// If the second value is out of range



}

QLinearGradient QgsMeshStrokeColor::makeSimpleLinearGradient( const QColor &color1, const QColor &color2, bool invert = false ) const
{
  QLinearGradient gradient;
  gradient.setColorAt( invert ? 1 : 0, color1 );
  gradient.setColorAt( invert ? 0 : 1, color2 );

  return gradient;
}

void QgsMeshStrokeColor::graduatedColorsExact( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Exact );

  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  if ( value2 < itemList.first().value )
    return;

  if ( value1 > itemList.last().value )
    return;

  for ( int i = 0; i < itemList.count(); ++i )
  {
    if ( itemList.at( i ).value >= value1 && itemList.at( i ).value <= value2 )
    {
      breakValues.append( itemList.at( i ).value );
      breakColors.append( itemList.at( i ).color );
    }
  }
}

void QgsMeshStrokeColor::graduatedColorsInterpolated( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated );

  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  if ( value2 < itemList.first().value ) // out of range and lesser
  {
    if ( !mColorRampShader.clip() )
    {
      QColor color = itemList.first().color;
      breakValues.append( value1 );
      breakValues.append( value2 );
      breakColors.append( color );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
    return;
  }

  if ( value1 > itemList.last().value ) // out of range and lesser
  {
    if ( !mColorRampShader.clip() )
    {
      QColor color = itemList.last().color;
      breakValues.append( value1 );
      breakValues.append( value2 );
      breakColors.append( color );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
    return;
  }

  int i = 0; //index of the interval in the color ramp shader

  if ( value1 < itemList.first().value ) //only first value out of range
  {
    if ( !mColorRampShader.clip() )
    {
      QColor color = itemList.first().color;
      breakValues.append( value1 );
      breakValues.append( itemList.first().value );
      breakColors.append( color );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
  }
  else // first value in the range
  {
    //search for the fist insterval
    while ( ( i < itemList.count() - 1 ) && value1 < itemList.at( i ).value )
      ++i;

    if ( i < itemList.count() - 1 )
    {
      int r, g, b, a;
      QColor firstColor;
      if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
        firstColor = QColor( r, g, b, a );
      QColor secondColor = itemList.at( i + 1 ).color;
      breakValues.append( value1 );
      breakValues.append( itemList.at( i + 1 ).value );
      breakColors.append( firstColor );
      breakColors.append( secondColor );
      gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );

      ++i;
    }
  }
  // go through the item list and fill with intermediate intervals
  while ( ( i < itemList.count() - 1 ) && itemList.at( i + 1 ).value < value2 )
  {
    QColor firstColor = breakColors.last();
    QColor secondColor = itemList.at( i + 1 ).color;
    breakValues.append( itemList.at( i + 1 ).value );
    breakColors.append( secondColor );
    gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );

    ++i;
  }

  // handle with the last value
  if ( value2 <= itemList.last().value && itemList.count() > 1 )
  {
    QColor firstColor = itemList.at( itemList.count() - 2 ).color;
    QColor secondColor;
    int r, g, b, a;
    if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
      secondColor = QColor( r, g, b, a );
    breakValues.append( itemList.last().value );
    breakColors.append( secondColor );
    gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );
  }
  else //value2 out of range
  {
    if ( !mColorRampShader.clip() )
    {
      QColor color = itemList.last().color;
      breakValues.append( value2 );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
  }
}

void QgsMeshStrokeColor::graduatedColorsDiscret( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Discrete );

  QList<QgsColorRampShader::ColorRampItem> itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  itemList.removeLast(); //remove the last one that is inf


  if ( value2 < itemList.first().value ) // out of range and lesser
  {
    QColor color = itemList.first().color;
    breakValues.append( value1 );
    breakValues.append( value2 );
    breakColors.append( color );
    breakColors.append( color );
    gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    return;
  }

  if ( value1 > itemList.last().value ) // out of range and lesser
  {
    QColor color = itemList.last().color;
    breakValues.append( value1 );
    breakValues.append( value2 );
    breakColors.append( color );
    breakColors.append( color );
    gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    return;
  }

  int i = 0; //index of the interval in the color ramp shader

  if ( value1 < itemList.first().value )
  {
    QColor color = itemList.first().color;
    breakValues.append( value1 );
    breakValues.append( itemList.first().value );
    breakColors.append( color );
    breakColors.append( color );
    gradients.append( makeSimpleLinearGradient( color, color, invert ) );
  }
  else // first value in the range
  {
    //search for the fist insterval
    while ( ( i < itemList.count() - 1 ) && value1 < itemList.at( i ).value )
      ++i;

    if ( i < itemList.count() - 1 )
    {
      int r, g, b, a;
      QColor firstColor;
      if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
        firstColor = QColor( r, g, b, a );
      QColor secondColor = itemList.at( i + 1 ).color;
      breakValues.append( value1 );
      breakValues.append( itemList.at( i + 1 ).value );
      breakColors.append( firstColor );
      breakColors.append( secondColor );
      gradients.append( makeSimpleLinearGradient( secondColor, secondColor, invert ) );

      ++i;
    }
  }
  // go through the item list and fill with intermediate intervals
  while ( ( i < itemList.count() - 1 ) && itemList.at( i + 1 ).value < value2 )
  {
    QColor firstColor = breakColors.last();
    QColor secondColor = itemList.at( i + 1 ).color;
    breakValues.append( itemList.at( i + 1 ).value );
    breakColors.append( secondColor );
    gradients.append( makeSimpleLinearGradient( secondColor, secondColor, invert ) );

    ++i;
  }

  // handle with the last value
  if ( value2 <= itemList.last().value && itemList.count() > 1 )
  {
    QColor firstColor = itemList.at( itemList.count() - 2 ).color;
    QColor secondColor;
    int r, g, b, a;
    if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
      secondColor = QColor( r, g, b, a );
    breakValues.append( itemList.last().value );
    breakColors.append( secondColor );
    gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );
  }
  else //value2 out of range
  {
    if ( !mColorRampShader.clip() )
    {
      QColor color = itemList.last().color;
      breakValues.append( value2 );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
  }
}

