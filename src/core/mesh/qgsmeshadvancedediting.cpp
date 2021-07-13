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
  QVector<QgsMeshVertex> newVertices;
  QVector<QgsMeshFace> newFaces;

  // create new vertices


  return *this;
}

void QgsMeshEditRefineFaces::createNewVerticesAndRefinedFaces( QgsMeshEditor *meshEditor,
    QVector<QgsMeshVertex> &newVertices,
    QSet<int> &facesToRefine,
    QHash<int, FaceRefinement> &facesRefinement,
    QVector<QgsMeshFace> &newFaces )
{
  const QgsTopologicalMesh &topology = *meshEditor->topologicalMesh();
  const QgsMesh &mesh = *meshEditor->topologicalMesh()->mesh();

  int startingVertexIndex = mesh.vertexCount();

  for ( const int faceIndex : std::as_const( mInputFaces ) )
  {
    FaceRefinement refinement;

    const QgsMeshFace &face = mesh.face( faceIndex );
    int faceSize = face.size();

    if ( faceSize == 3 || faceSize == 4 )
    {
      refinement.newVerticesLocalIndex.reserve( faceSize );
      refinement.refinedFaceNeighbor.reserve( faceSize );
      refinement.borderFaceNeighbor.reserve( faceSize );
      const QList<int> &neighbors = topology.neighborsOfFace( faceIndex );

      for ( int positionInFace = 0; positionInFace < faceSize; ++positionInFace )
      {
        refinement.refinedFaceNeighbor.append( false );
        refinement.borderFaceNeighbor.append( false );

        int neighborFaceIndex = neighbors.at( positionInFace );
        bool needCreateVertex = true;
        if ( neighborFaceIndex != -1 && facesToRefine.contains( neighborFaceIndex ) )
        {
          int positionVertexInNeighbor = vertexPositionInFace( mesh, face.at( positionInFace ), neighborFaceIndex );
          refinement.refinedFaceNeighbor[positionInFace] = true;
          QHash<int, FaceRefinement>::iterator it = facesRefinement.find( neighborFaceIndex );
          if ( it != facesRefinement.end() )
          {
            FaceRefinement &neighborRefinement = it.value();
            refinement.newVerticesLocalIndex.append( neighborRefinement.newVerticesLocalIndex.at( positionVertexInNeighbor ) );

            needCreateVertex = false;
          }
        }

        if ( needCreateVertex )
        {
          const QgsMeshVertex &vertex1 = mesh.vertex( face.at( positionInFace ) );
          const QgsMeshVertex &vertex2 = mesh.vertex( face.at( ( positionInFace + 1 ) % faceSize ) );

          refinement.newVerticesLocalIndex.append( newVertices.count() );

          newVertices.append( QgsMeshVertex( ( vertex1.x() + vertex2.x() ) / 2,
                                             ( vertex1.y() + vertex2.y() ) / 2,
                                             ( vertex1.z() + vertex2.z() ) / 2 ) );
        }
      }

      if ( faceSize == 3 )
      {
        for ( int i = 0; i < faceSize; ++i )
        {
          QgsMeshFace newFace( {face.at( i ),
                                refinement.newVerticesLocalIndex.at( i ) + startingVertexIndex,
                                refinement.newVerticesLocalIndex.at( ( i + faceSize - 1 ) % faceSize ) + startingVertexIndex} );
          refinement.newFacesLocalIndex.append( newFaces.count() );
          newFaces.append( newFace );

        }
        QgsMeshFace newFace( {refinement.newVerticesLocalIndex.at( 0 ) + startingVertexIndex,
                              refinement.newVerticesLocalIndex.at( 1 ) + startingVertexIndex,
                              refinement.newVerticesLocalIndex.at( ( 2 ) % faceSize ) + startingVertexIndex} );
        refinement.newFacesLocalIndex.append( newFaces.count() );
        newFaces.append( newFace );
      }

      if ( faceSize == 4 )
      {
        int centerVertexIndex = newVertices.count() + startingVertexIndex;
        refinement.newCenterVertexIndex = newVertices.count();
        newVertices.append( QgsMeshUtils::centroid( face, mesh.vertices ) );
        for ( int i = 0; i < faceSize; ++i )
        {
          QgsMeshFace newFace( {face.at( i ),
                                refinement.newVerticesLocalIndex.at( i ) + startingVertexIndex,
                                centerVertexIndex,
                                refinement.newVerticesLocalIndex.at( ( i + faceSize - 1 ) % faceSize ) + startingVertexIndex} );
          refinement.newFacesLocalIndex.append( newFaces.count() );
          newFaces.append( newFace );
        }

      }
      else
        refinement.newCenterVertexIndex = -1;

      facesRefinement.insert( faceIndex, refinement );
    }
    else
    {
      //not 3 or 4 vertices, we do not refine this face
      facesToRefine.remove( faceIndex );
    }
  }
}

void QgsMeshEditRefineFaces::createNewBorderFaces( QgsMeshEditor *meshEditor,
    const QSet<int> &facesToRefine,
    QHash<int, FaceRefinement> &facesRefinement,
    QHash<int, BorderFace> &borderFaces )
{
  const QgsTopologicalMesh &topology = *meshEditor->topologicalMesh();
  const QgsMesh &mesh = *meshEditor->topologicalMesh()->mesh();

  // first create amm the border faces
  for ( int faceIndexToRefine : facesToRefine )
  {
    const QgsMeshFace &faceToRefine = mesh.face( faceIndexToRefine );
    int faceToRefineSize = faceToRefine.size();

    const QList<int> &neighbors = topology.neighborsOfFace( faceIndexToRefine );

    QHash<int, FaceRefinement>::iterator itFace = facesRefinement.find( faceIndexToRefine );

    if ( itFace == facesRefinement.end() )
      Q_ASSERT( false ); // That could not happen

    FaceRefinement &refinement = itFace.value();

    for ( int posInFaceToRefine = 0; posInFaceToRefine < faceToRefineSize; ++posInFaceToRefine )
    {
      int neighborFaceIndex = neighbors.at( posInFaceToRefine );
      if ( neighborFaceIndex != -1 && !facesToRefine.contains( neighborFaceIndex ) )
      {
        const QgsMeshFace &neighborFace = mesh.face( neighborFaceIndex );
        int neighborFaceSize = neighborFace.size();
        int positionInNeighbor = vertexPositionInFace( mesh, faceToRefine.at( posInFaceToRefine ), neighborFaceIndex );
        positionInNeighbor = ( positionInNeighbor + neighborFaceSize - 1 ) % neighborFaceSize;

        QHash<int, BorderFace>::iterator it = borderFaces.find( neighborFaceIndex );
        if ( it == borderFaces.end() ) //not present for now--> create a border face
        {
          BorderFace borderFace;
          for ( int i = 0; i < neighborFaceSize; ++i )
          {
            borderFace.unchangeFacesNeighbor.append( false );
            borderFace.borderFacesNeighbor.append( false );
            if ( i == positionInNeighbor )
            {
              borderFace.refinedFacesNeighbor.append( true );
              borderFace.newVerticesLocalIndex.append( refinement.newVerticesLocalIndex.at( posInFaceToRefine ) );
            }
            else
            {
              borderFace.refinedFacesNeighbor.append( true );
              borderFace.newVerticesLocalIndex.append( -1 );
            }
          }
          borderFaces.insert( neighborFaceIndex, borderFace );
        }
        else
        {
          BorderFace &borderFace = it.value();
          for ( int i = 0; i < neighborFaceSize; ++i )
          {
            if ( i == positionInNeighbor )
            {
              borderFace.unchangeFacesNeighbor[i] = false;
              borderFace.borderFacesNeighbor[i] = false;
              borderFace.refinedFacesNeighbor[i] = true;
              borderFace.newVerticesLocalIndex[i] = refinement.newVerticesLocalIndex.at( posInFaceToRefine );
            }
          }
        }
      }
    }
  }

  // now link border face each other if needed
  for ( QHash<int, BorderFace>::iterator it = borderFaces.begin(); it != borderFaces.end(); ++it )
  {
    int faceIndex = it.key();
    BorderFace &borderFace = it.value();

    const QgsMeshFace &face = mesh.face( faceIndex );
    int faceSize = face.size();

    const QList<int> &neighbors = topology.neighborsOfFace( faceIndex );
    for ( int posInFace = 0; posInFace < faceSize; ++posInFace )
    {
      int neighborIndex = neighbors.at( posInFace );

      if ( neighborIndex != -1 && facesToRefine.contains( neighborIndex ) )
      {

      }
      else if ( neighborIndex != -1 )
      {
        QHash<int, BorderFace>::iterator itNeighbor;
      }

    }
  }

}
