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
  painter->setPen( pen );
  QBrush brush;
  brush.setStyle( Qt::SolidPattern );

  const QgsMapToPixel &mapToPixel = context.mapToPixel();

  QPointF p1 = mapToPixel.transform( point1 ).toQPointF();
  QPointF p2 = mapToPixel.transform( point2 ).toQPointF();
  QPointF dir = p2 - p1;
  double lenght = sqrt( pow( dir.x(), 2 ) + pow( dir.y(), 2 ) );
  QPointF diru = dir / lenght;
  QPointF orthu = QPointF( -diru.y(), diru.x() );

  QList<double> breakValues;
  QList<QColor> breakColors;
  QList<QLinearGradient> gradients;

  mStrokeColoring.graduatedColors( value1, value2, breakValues, breakColors, gradients );

  if ( gradients.isEmpty() && !breakValues.empty() && !breakColors.isEmpty() ) //exact colors to render
  {
    Q_ASSERT( breakColors.count() == breakValues.count() );
    for ( int i = 0; i < breakValues.count(); ++i )
    {
      double value = breakValues.at( i );
      double width = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value ), mStrokeWidthUnit );
      pen.setWidthF( width );
      pen.setColor( breakColors.at( i ) );
      pen.setCapStyle( Qt::PenCapStyle::RoundCap );
      painter->setPen( pen );
      QPointF point = p1 + dir * ( value - value1 ) / ( value2 - value1 );
      painter->drawPoint( point );
    }
  }
  else
  {
    if ( gradients.isEmpty() && breakValues.empty() && breakColors.count() == 1 ) //only one color to render
    {
      double width1 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value1 ), mStrokeWidthUnit );
      double width2 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value2 ), mStrokeWidthUnit ) ;
      QPolygonF varLine;
      double semiWidth1 = width1 / 2;
      double semiWidth2 = width2 / 2;

      varLine.append( p1 + orthu * semiWidth1 );
      varLine.append( p2 + orthu * semiWidth2 );
      varLine.append( p2 - orthu * semiWidth2 );
      varLine.append( p1 - orthu * semiWidth1 );

      painter->drawPolygon( varLine );
    }
    else
    {
      Q_ASSERT( breakColors.count() == breakValues.count() );
      Q_ASSERT( breakColors.count() == gradients.count() + 1 );
      for ( int i = 0; i < gradients.count(); ++i )
      {
        double firstValue = breakValues.at( i );
        double secondValue = breakValues.at( i + 1 );
        double width1 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( firstValue ), mStrokeWidthUnit );
        double width2 = context.convertToPainterUnits( mStrokeWidth.strokeWidth( secondValue ), mStrokeWidthUnit ) ;

        QPointF pointStart = p1 + dir * ( firstValue - value1 ) / ( value2 - value1 );
        QPointF pointEnd = p1 + dir * ( secondValue - value1 ) / ( value2 - value1 );

        QPolygonF varLine;
        double semiWidth1 = width1 / 2;
        double semiWidth2 = width2 / 2;

        QLinearGradient gradient = gradients.at( i );
        gradient.setStart( pointStart );
        gradient.setFinalStop( pointEnd );
        QBrush brush( gradient );
        painter->setBrush( brush );

        varLine.append( pointStart + orthu * semiWidth1 );
        varLine.append( pointEnd + orthu * semiWidth2 );
        varLine.append( pointEnd - orthu * semiWidth2 );
        varLine.append( pointStart - orthu * semiWidth1 );

        painter->drawPolygon( varLine );
      }

    }
//    //Draw pen cap
//
//    QRectF capBox1( p1.x() - width1 / 2, p1.y() - width1 / 2, width1, width1 );
//    QRectF capBox2( p2.x() - width2 / 2, p2.y() - width2 / 2, width2, width2 );
//    int startAngle;
//    startAngle = ( acos( -orthu.x() ) / M_PI ) * 180;
//    if ( orthu.y() < 0 )
//      startAngle = 360 - startAngle;

//    brush.setColor( mStrokeColoring.color( value1 ) );
//    painter->setBrush( brush );
//    painter->drawPie( capBox1, ( startAngle - 1 ) * 16, 182 * 16 );
//    brush.setColor( mStrokeColoring.color( value2 ) );
//    painter->setBrush( brush );
//    painter->drawPie( capBox2, ( startAngle + 179 ) * 16, 182 * 16 );


  }


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

void QgsMeshStrokeColor::graduatedColors( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients ) const
{
  breakValues.clear();
  breakColors.clear();
  gradients.clear();
  if ( mColoringMethod == SingleColor )
  {
    breakValues.append( value1 );
    breakColors.append( mSingleColor );
    breakValues.append( value2 );
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

  if ( mColorRampShader.colorRampType() == QgsColorRampShader::Exact )
    graduatedColorsExact( firstvalue, secondValue, breakValues, breakColors, gradients, invert );
  else
    graduatedColors( firstvalue, secondValue, breakValues, breakColors, gradients, invert );
}

QLinearGradient QgsMeshStrokeColor::makeSimpleLinearGradient( const QColor &color1, const QColor &color2, bool invert = false ) const
{
  QLinearGradient gradient;
  gradient.setColorAt( invert ? 1 : 0, color1 );
  gradient.setColorAt( invert ? 0 : 1, color2 );

  return gradient;
}

int QgsMeshStrokeColor::itemColorIndexInf( double value ) const
{
  QList<QgsColorRampShader::ColorRampItem> itemList = mColorRampShader.colorRampItemList();

  if ( itemList.isEmpty() || itemList.first().value > value )
    return -1;

  if ( mColorRampShader.colorRampType() == QgsColorRampShader::Discrete )
    itemList.removeLast(); //remove the inf value

  if ( value > itemList.last().value )
    return itemList.count() - 1;

  int indSup = itemList.count() - 1;
  int indInf = 0;

  while ( true )
  {
    if ( abs( indSup - indInf ) <= 1 ) //always indSup>indInf, but abs to prevent infinity loop
      return indInf;

    int newInd = ( indInf + indSup ) / 2;

    if ( itemList.at( newInd ).value == std::numeric_limits<double>::quiet_NaN() )
      return -1;

    if ( itemList.at( newInd ).value <= value )
      indInf = newInd;
    else
      indSup = newInd;
  }
}

void QgsMeshStrokeColor::graduatedColorsExact( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Exact );
  Q_ASSERT( breakValues.isEmpty() );
  Q_ASSERT( breakColors.isEmpty() );
  Q_ASSERT( gradients.isEmpty() );

  Q_UNUSED( invert );

  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  int index = itemColorIndexInf( value1 );
  if ( index < 0 || !qgsDoubleNear( value1, itemList.at( index ).value ) )
    index++;

  while ( index < itemList.count() && itemList.at( index ).value < value2 )
  {
    breakValues.append( itemList.at( index ).value );
    breakColors.append( itemList.at( index ).color );
    index++;
  }
}

void QgsMeshStrokeColor::graduatedColors( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients, bool invert ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() != QgsColorRampShader::Exact );
  Q_ASSERT( breakValues.isEmpty() );
  Q_ASSERT( breakColors.isEmpty() );
  Q_ASSERT( gradients.isEmpty() );

  bool discrete = mColorRampShader.colorRampType() == QgsColorRampShader::Discrete;

  QList<QgsColorRampShader::ColorRampItem> itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  if ( discrete )
    itemList.removeLast(); // remove the last one that is inf

  if ( value2 <= itemList.first().value ) // completly out of range and lesser
  {
    if ( !mColorRampShader.clip() || discrete )
      breakColors.append( itemList.first().color );
    return;
  }

  if ( value1 >= itemList.last().value ) // completly out of range and greater
  {
    if ( !mColorRampShader.clip() || discrete )
      breakColors.append( itemList.last().color );
    return;
  }

  int index = itemColorIndexInf( value1 ); // index of the inf value of the interval in the color ramp shader
  QColor firstColor;
  QColor secondColor;
  if ( index < 0 ) // value1 out of range
  {
    if ( mColorRampShader.clip() )
    {
      // append only the first values to return
      breakValues.append( itemList.first().value );
      breakColors.append( itemList.first().color );
    }
    else
    {
      // construct the first interval to return
      breakValues.append( value1 );
      breakValues.append( itemList.first().value );
      QColor color = itemList.first().color;
      breakColors.append( color );
      breakColors.append( color );
      gradients.append( makeSimpleLinearGradient( color, color, invert ) );
    }
  }
  else // append the first value with corresponding color
  {
    Q_ASSERT( itemList.count() > 1 );

    int r, g, b, a;
    QColor color;
    if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
      color = QColor( r, g, b, a );
    breakValues.append( value1 );
    breakColors.append( color );
  }

  index++; //increment the index before go through the intervals

  while ( index < itemList.count() && itemList.at( index ).value < value2 )
  {
    QColor firstColor = breakColors.last();
    QColor secondColor = itemList.at( index ).color;
    breakValues.append( itemList.at( index ).value );
    breakColors.append( secondColor );
    if ( discrete )
      gradients.append( makeSimpleLinearGradient( secondColor, secondColor, invert ) );
    else
      gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );
    index++;
  }

  if ( value2 < itemList.last().value || !mColorRampShader.clip() || discrete ) //add value2 to close
  {
    QColor firstColor = breakColors.last();
    QColor secondColor = itemList.last().color;
    if ( value2 < itemList.last().value || !discrete )
    {
      int r, g, b, a;
      if ( mColorRampShader.shade( value2, &r, &g, &b, &a ) )
        secondColor = QColor( r, g, b, a );
    }
    breakColors.append( secondColor );
    breakValues.append( value2 );
    if ( discrete )
      gradients.append( makeSimpleLinearGradient( secondColor, secondColor, invert ) );
    else
      gradients.append( makeSimpleLinearGradient( firstColor, secondColor, invert ) );
  }

}
