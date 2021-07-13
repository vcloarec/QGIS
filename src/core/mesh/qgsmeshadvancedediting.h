/***************************************************************************
  qgsmeshadvancedediting.h - QgsMeshAdvancedEditing

 ---------------------
 begin                : 9.7.2021
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
#ifndef QGSMESHADVANCEDEDITING_H
#define QGSMESHADVANCEDEDITING_H


#include "qgis_core.h"
#include "qgstopologicalmesh.h"
#include "qgstriangularmesh.h"

class QgsMeshEditor;
class QgsProcessingFeedback;


class CORE_EXPORT QgsMeshAdvancedEditing : protected QgsTopologicalMesh::Changes
{
  public:
    QgsMeshAdvancedEditing();
    virtual ~QgsMeshAdvancedEditing();
    virtual QgsTopologicalMesh::Changes apply( QgsMeshEditor *meshEditor ) = 0; SIP_SKIP

    void setInputVertices( const QList<int> verticesIndexes );

    void setInputFaces( const QList<int> faceIndexes );

    bool isFinished() const {return mIsFinished;}

    QString message() const;

  protected:
    bool mIsFinished = false;
    QList<int> mInputVertices;
    QList<int> mInputFaces;
    QString mMessage;

};

class CORE_EXPORT QgsMeshEditRefineFaces : public QgsMeshAdvancedEditing
{
  public:
    QgsTopologicalMesh::Changes apply( QgsMeshEditor *meshEditor );

  private:

    struct FaceRefinement
    {
      QList<int> newVerticesLocalIndex; // new vertices in the same order of the vertex index (ccw)
      QList<bool> refinedFaceNeighbor;
      QList<bool> borderFaceNeighbor;
      int newCenterVertexIndex;
      QList<int> newFacesLocalIndex;
    };

    struct BorderFace
    {
      QList<bool> refinedFacesNeighbor;
      QList<bool> borderFacesNeighbor;
      QList<bool> unchangeFacesNeighbor;
      QList<int> newVerticesLocalIndex;
    };

    //! Create new vertices of the refinement and populate helper containers
    void createNewVerticesAndRefinedFaces( QgsMeshEditor *meshEditor,
                                           QVector<QgsMeshVertex> &newVertices,
                                           QSet<int> &facesToRefine,
                                           QHash<int, FaceRefinement> &facesRefinement, QVector<QgsMeshFace> &newFaces );

    void createNewBorderFaces( QgsMeshEditor *meshEditor,
                               const QSet<int> &facesToRefine,
                               QHash<int, FaceRefinement> &facesRefinement,
                               QHash<int, BorderFace> &borderFaces );



    friend class TestQgsMeshEditor;
};


#endif // QGSMESHADVANCEDEDITING_H
