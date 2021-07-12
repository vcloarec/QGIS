/***************************************************************************
  qgsmeshadvancedediting.cpp - QgsMeshAdvancedEditing

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
#include "qgsmeshadvancedediting.h"

#include "qgsmesheditor.h"

QgsMeshAdvancedEditing::QgsMeshAdvancedEditing() = default;

QgsMeshAdvancedEditing::~QgsMeshAdvancedEditing() = default;

void QgsMeshAdvancedEditing::setInputVertices( const QList<int> verticesIndexes )
{
  mInputVertices = verticesIndexes;
}

void QgsMeshAdvancedEditing::setInputFaces( const QList<int> faceIndexes )
{
  mInputFaces = faceIndexes;
}

QString QgsMeshAdvancedEditing::message() const
{
  return mMessage;
}

static int vertexPositionInFace( int vertexIndex, const QgsMeshFace &face )
{
  return face.indexOf( vertexIndex );
}

static int vertexPositionInFace( const QgsMesh &mesh, int vertexIndex, int faceIndex )
{
  if ( faceIndex < 0 || faceIndex >= mesh.faceCount() )
    return -1;

  return vertexPositionInFace( vertexIndex, mesh.face( faceIndex ) );
}

QgsTopologicalMesh::Changes QgsMeshEditRefineFaces::apply( QgsMeshEditor *meshEditor )
{
  QSet<int> facesToRefine = mInputFaces.toSet();
  QHash<int, FaceRefinement> facesRefinement;
  QMultiHash<int, int> borderFaces; //map from border faces to link refined faces

  const QgsMesh &mesh = *meshEditor->topologicalMesh()->mesh();
  const QgsTopologicalMesh &topology = *meshEditor->topologicalMesh();

  // create new vertices
  for ( const int faceIndex : std::as_const( mInputFaces ) )
  {
    QVector<QgsMeshVertex> newVertices;

    FaceRefinement refinement;

    const QgsMeshFace &face = mesh.face( faceIndex );
    int faceSize = face.size();

    refinement.newBorderVertexIndexes.reserve( faceSize );

    const QList<int> &neighbors = topology.neighborsOfFace( faceIndex );

    if ( face.size() == 3 || face.size() == 4 )
    {
      for ( int positionInFace = 0; positionInFace < faceSize; ++positionInFace )
      {
        int neighborFaceIndex = neighbors.at( positionInFace );


        if ( neighborFaceIndex != -1 && !facesToRefine.contains( neighborFaceIndex ) )
        {
          borderFaces.insert( neighborFaceIndex, faceIndex );
        }
        else
        {
          QHash<int, FaceRefinement>::const_iterator it = facesRefinement.constFind( neighborFaceIndex );
          if ( it != facesRefinement.constEnd() )
          {
            const FaceRefinement neighborRefinement = it.value();
            int positionInNeighbor = vertexPositionInFace( mesh, face.at( positionInFace ), neighborFaceIndex );
            refinement.newBorderVertexIndexes[positionInFace] = neighborRefinement.newBorderVertexIndexes.at( positionInNeighbor );
          }
          else
          {
            const QgsMeshVertex &vertex1 = mesh.vertex( face.at( positionInFace ) );
            const QgsMeshVertex &vertex2 = mesh.vertex( face.at( ( positionInFace + 1 ) % faceSize ) );

            refinement.newBorderVertexIndexes[newVertices.count()];

            newVertices.append( QgsMeshVertex( ( vertex1.x() + vertex2.x() ) / 2,
                                               ( vertex1.y() + vertex2.y() ) / 2,
                                               ( vertex1.z() + vertex2.z() ) / 2 ) );
          }
        }
      }
    }
    else if ( face.size() == 4 )
      newVertices.append( QgsMeshUtils::centroid( face, mesh.vertices ) );
    else
    {
      continue;
    }
  }


  return *this;
}
