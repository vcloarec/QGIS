/***************************************************************************
  qgsmaptooleditmesh.h - QgsMapToolEditMesh

 ---------------------
 begin                : 24.6.2021
 copyright            : (C) 2021 by Vincent Cloarec
 email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSMAPTOOLEDITMESH_H
#define QGSMAPTOOLEDITMESH_H

#include <QWidget>
#include <QPointer>

#include "qgis_app.h"
#include "qgsmaptooladvanceddigitizing.h"
#include "qgsmeshdataprovider.h"
#include "qgsmesheditor.h"
#include "qgsmeshlayer.h"
#include "qgspointlocator.h"

class QgsRubberBand;
class QgsVertexMarker;
class QgsDoubleSpinBox;
class QgsSnapIndicator;


class QgsZValueWidget : public QWidget
{
    Q_OBJECT
  public:
    QgsZValueWidget( const QString &label, QWidget *parent = nullptr );
    double zValue() const;

    QWidget *spinBox() const;

  private:
    QgsDoubleSpinBox *mZValueSpinBox = nullptr;

};

class APP_EXPORT QgsMapToolEditMesh : public QgsMapToolAdvancedDigitizing
{
    Q_OBJECT
  public:
    QgsMapToolEditMesh( QgsMapCanvas *canvas );
    ~QgsMapToolEditMesh();

    void deactivate() override;
    void activate() override;

  protected:
    bool eventFilter( QObject *obj, QEvent *ev ) override;
    void cadCanvasPressEvent( QgsMapMouseEvent *e ) override;
    void cadCanvasReleaseEvent( QgsMapMouseEvent *e ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e ) override;
    void keyPressEvent( QKeyEvent *e ) override;
    void keyReleaseEvent( QKeyEvent *e ) override;

    //! Start addition of a new vertex on double-click
    void canvasDoubleClickEvent( QgsMapMouseEvent *e ) override;

  private slots:
    void setCurrentLayer( QgsMapLayer *layer );
    void onEdit();
    void onEdidingStarted();
    void onEditingStopped();

  private:
    // methods
    const QgsMeshVertex mapVertex( int index ) const;
    const QgsMeshFace nativeFace( int index ) const;

    void highlightCurrentHoveredFace( const QgsPointXY &mapPoint );
    void highlightCloseVertex( const QgsPointXY &mapPoint );

    void createZValueWidget();
    void deleteZvalueWidget();
    void clear();
    void addVertex( const QgsPointXY &mapPoint, const QgsPointLocator::Match &mapPointMatch );
    void updateFreeVertices();

    QgsPointSequence nativeFaceGeometry( int faceIndex ) const;

    QgsPointXY newFaceMarkerPosition( int vertexIndex );

    void addVertexToFaceCanditate( int vertexIndex );
    bool testNewVertexInFaceCanditate( int vertexIndex );

    // selection private method
    void setSelectedVertex( const QList<int> newSelectedVertex, bool ctrl );
    void clearSelectedvertex();
    void removeSelectedVerticesFromMesh();
    void selectVerticesInGeometry( const QgsGeometry &geometry, bool ctrl );

    // members
    enum State
    {
      None,
      AddingNewFace,
      Selecting,
    };

    State mCurrentState = None;

    QPointer<QgsMeshLayer> mCurrentLayer = nullptr;
    QPointer<QgsMeshEditor> mCurrentEditor = nullptr;
    std::unique_ptr<QgsSnapIndicator> mSnapIndicator;
    int mCurrentFaceIndex = -1;
    int mCurrentVertexIndex = -1;
    QList<int> mNewFaceCandidate;
    QList<int> mSelectedVertex;
    bool mDoubleClick = false;
    bool mCtrlPressed = false;

//    QgsVertexMarker *mEdgeCenterMarker = nullptr;
//    //! rubber band for highlight of a whole feature on mouse over and not dragging anything
//    QgsRubberBand *mFeatureBand = nullptr;
//    //! rubber band for highlight of all vertices of a feature on mouse over and not dragging anything
//
//    //! source layer for mFeatureBand (null if mFeatureBand is null)
//    const QgsVectorLayer *mFaceBandLayer = nullptr;
//    //! highlight of a vertex while mouse pointer is close to a vertex and not dragging anything
//
//    //! highlight of an edge while mouse pointer is close to an edge and not dragging anything
//    QgsRubberBand *mEdgeBand = nullptr;

    //! Rubber band used to highlight a face that is on mouse over and not dragging anything
    QgsRubberBand *mFaceRubberBand = nullptr;
    //! Rubber band used to highlight vertex of the face that is on mouse over and not dragging anything
    QgsRubberBand *mFaceBandMarkers = nullptr;
    //! Rubber band used to highlight the vertex that is in mouse over and not dragging anything
    QgsRubberBand *mVertexBand = nullptr;
    //! Marker used to propose to add a new face when a boundary vertex is higthlight
    QgsVertexMarker *mNewFaceMarker = nullptr;
    //! Rubber band used when adding a new face
    QgsRubberBand *mNewFaceBand = nullptr;
    QColor mInvalidFaceColor;
    QColor mValidFaceColor;
    //! Rubber band for selection of vertices
    QgsRubberBand *mSelectionBand = nullptr;
    QPoint mStartSelectionPos;


    //! Markers that makes visible free vertices
    QList<QgsVertexMarker *> mFreeVertexMarker;

    //! Markers for selected vertices
    QList<QgsVertexMarker *> mSelectedVerticesMarker;

    QgsVertexMarker *mEdgeCenterMarker = nullptr;

    //! Checks if we are closed to a vertex, if yes return the index of the vertex;
    int closeVertex( const QgsPointXY &point ) const;

    QgsZValueWidget *mZValueWidget = nullptr;

};

#endif // QGSMAPTOOLEDITMESH_H
