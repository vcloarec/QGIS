/***************************************************************************
  qgstopologicalmesh.cpp - QgsTopologicalMesh

 ---------------------
 begin                : 18.6.2021
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
#include "qgstopologicalmesh.h"
#include "qgsmesheditor.h"
#include "qgsmessagelog.h"
#include "qgsgeometryutils.h"
#include "qgslinestring.h"

#include <poly2tri.h>
#include <QSet>

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

static double crossProduct( int centralVertex, int vertex1, int vertex2, const QgsMesh &mesh )
{
  QgsMeshVertex vc = mesh.vertices.at( centralVertex );
  QgsMeshVertex v1 = mesh.vertices.at( vertex1 );
  QgsMeshVertex v2 = mesh.vertices.at( vertex2 );

  double ux1 = v1.x() - vc.x();
  double uy1 = v1.y() - vc.y();
  double vx1 = v2.x() - vc.x();
  double vy1 = v2.y() - vc.y();

  return ux1 * vy1 - uy1 * vx1;
}

QgsMeshVertexCirculator::QgsMeshVertexCirculator( const QgsTopologicalMesh &topologicalMesh, int vertexIndex )
  : mFaces( topologicalMesh.mMesh->faces )
  , mFacesNeighborhood( topologicalMesh.mFacesNeighborhood )
  ,  mVertexIndex( vertexIndex )
{
  if ( vertexIndex >= 0 && vertexIndex < topologicalMesh.mMesh->vertexCount() )
  {
    mCurrentFace = topologicalMesh.mVertexToFace[vertexIndex];
    mIsValid = vertexPositionInFace( *topologicalMesh.mesh(), vertexIndex, mCurrentFace ) != -1;
  }
  else
  {
    mIsValid = false;
  }

  if ( mIsValid )
    mLastValidFace = mCurrentFace;
}

QgsMeshVertexCirculator::QgsMeshVertexCirculator( const QgsTopologicalMesh::TopologicalFaces &topologicalFaces, int faceIndex, int vertexIndex )
  : mFaces( topologicalFaces.mFaces )
  , mFacesNeighborhood( topologicalFaces.mFacesNeighborhood )
  , mVertexIndex( vertexIndex )
{
  const QgsMeshFace &face = topologicalFaces.mFaces.at( faceIndex );
  mIsValid = vertexPositionInFace( vertexIndex, face ) != -1;

  mCurrentFace = faceIndex;
  mLastValidFace = mCurrentFace;
}

QgsMeshVertexCirculator::QgsMeshVertexCirculator( const QgsTopologicalMesh::TopologicalFaces &topologicalFaces, int vertexIndex )
  : mFaces( topologicalFaces.mFaces )
  , mFacesNeighborhood( topologicalFaces.mFacesNeighborhood )
  , mVertexIndex( vertexIndex )
{

  mCurrentFace = topologicalFaces.mVerticesToFace.value( vertexIndex, -1 );
  mLastValidFace = mCurrentFace;
  mIsValid = mCurrentFace != -1;
}

int QgsMeshVertexCirculator::turnCounterClockwise() const
{
  if ( mCurrentFace == -1 )
    mCurrentFace = mLastValidFace;
  else
  {
    int currentPos = positionInCurrentFace();
    if ( currentPos == -1 )
      return -1;

    const QgsMeshFace &currentFace = mFaces.at( mCurrentFace );
    int faceSize = currentFace.size();
    mLastValidFace = mCurrentFace;
    mCurrentFace = mFacesNeighborhood.at( mCurrentFace ).at( ( currentPos + faceSize - 1 ) % currentFace.count() );
  }

  return mCurrentFace;
}

int QgsMeshVertexCirculator::turnClockwise() const
{
  if ( mCurrentFace == -1 )
    mCurrentFace = mLastValidFace;
  else
  {
    int currentPos = positionInCurrentFace();
    if ( currentPos == -1 )
      return -1;

    const QgsMeshFace &currentFace = mFaces.at( mCurrentFace );
    int faceSize = currentFace.size();
    mLastValidFace = mCurrentFace;
    mCurrentFace = mFacesNeighborhood.at( mCurrentFace ).at( ( currentPos ) % faceSize );
  }

  return mCurrentFace;
}

int QgsMeshVertexCirculator::currentFaceIndex() const
{
  return mCurrentFace;
}

QgsMeshFace QgsMeshVertexCirculator::currentFace() const
{
  if ( mCurrentFace != -1 )
    return mFaces.at( mCurrentFace );
  else
    return QgsMeshFace();
}

bool QgsMeshVertexCirculator::goBoundaryClockwise() const
{
  if ( !isValid() )
    return false;

  if ( mCurrentFace == -1 )
    mCurrentFace = mLastValidFace;

  int firstFace = mCurrentFace;

  while ( turnClockwise() != -1 && currentFaceIndex() != firstFace ) {}
  if ( mCurrentFace == firstFace )
    return false; // a complete turn, so the vertex is not a boundary vertex, something went wrong

  return ( turnCounterClockwise() != -1 );
}

bool QgsMeshVertexCirculator::goBoundaryCounterClockwise() const
{
  if ( !isValid() )
    return false;

  if ( mCurrentFace == -1 )
    mCurrentFace = mLastValidFace;

  int firstFace = mCurrentFace;

  while ( turnCounterClockwise() != -1 && currentFaceIndex() != firstFace ) {}
  if ( mCurrentFace == firstFace )
    return false; // a complete turn, so the vertex is not a boundary vertex, something went wrong

  return ( turnClockwise() != -1 );
}

int QgsMeshVertexCirculator::oppositeVertexClockWise() const
{
  if ( mCurrentFace == -1 )
    return -1;

  const QgsMeshFace &face = currentFace();

  if ( face.isEmpty() )
    return -1;

  int vertexPosition = vertexPositionInFace( mVertexIndex, currentFace() );

  if ( vertexPosition == -1 )
    return -1;

  return face.at( ( vertexPosition + 1 ) % face.count() );
}

int QgsMeshVertexCirculator::oppositeVertexCounterClockWise() const
{
  if ( mCurrentFace == -1 )
    return -1;

  const QgsMeshFace &face = currentFace();

  if ( face.isEmpty() )
    return -1;

  int vertexPosition = vertexPositionInFace( mVertexIndex, currentFace() );

  if ( vertexPosition == -1 )
    return -1;

  return face.at( ( vertexPosition - 1 + face.count() ) % face.count() );
}

bool QgsMeshVertexCirculator::isValid() const
{
  return mIsValid;
}

int QgsMeshVertexCirculator::positionInCurrentFace() const
{
  if ( mCurrentFace < 0 || mCurrentFace > mFaces.count() )
    return -1;

  return vertexPositionInFace( mVertexIndex, mFaces.at( mCurrentFace ) );
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::addFaces( const QgsTopologicalMesh::TopologicalFaces &topologicalFaces )
{
  Changes changes;
  changes.mFacesToAdd = topologicalFaces.mFaces;
  changes.mAddedFacesFirstIndex = mMesh->faceCount();

  changes.mFacesNeighborhoodToAdd.resize( changes.mFacesToAdd.count() );
  for ( int i = 0; i < changes.mFacesToAdd.count(); ++i )
    changes.mFacesNeighborhoodToAdd[i] = QVector<int>( changes.mFacesToAdd.at( i ).count(), -1 );

  for ( int boundary : topologicalFaces.mBoundaries )
  {
    //if the boundary id a free vertex in the destination mesh, no need to check
    if ( mVertexToFace.at( boundary ) == -1 )
      continue;

    int indexOfStartinFace = topologicalFaces.mVerticesToFace.value( boundary );

    QgsMeshVertexCirculator newFacesCirculator( topologicalFaces, indexOfStartinFace, boundary );
    //search for face boundary on clockwise side of new faces
    newFacesCirculator.goBoundaryClockwise();
    int oppositeVertexForNewFace = newFacesCirculator.oppositeVertexClockWise();
    if ( mVertexToFace.at( oppositeVertexForNewFace ) == -1 )
      continue;

    QgsMeshVertexCirculator meshCirculator = vertexCirculator( boundary );
    meshCirculator.goBoundaryCounterClockwise();
    int oppositeVertexForMeshFace = meshCirculator.oppositeVertexCounterClockWise();

    const QgsMeshFace &newFaceBoundary = newFacesCirculator.currentFace();
    int boundaryPositionInNewFace = vertexPositionInFace( boundary, newFaceBoundary );

    if ( oppositeVertexForMeshFace != oppositeVertexForNewFace )
    {
      changes.mFacesNeighborhoodToAdd[newFacesCirculator.currentFaceIndex()][boundaryPositionInNewFace] = -1 ;
    }
    else
    {
      const QgsMeshFace &meshFaceBoundary = meshCirculator.currentFace();
      int boundaryPositionInMeshFace = vertexPositionInFace( meshCirculator.oppositeVertexCounterClockWise(), meshFaceBoundary );

      changes.mNeighborhoodChanges.append( std::array<int, 4>(
      {
        meshCirculator.currentFaceIndex(),
        boundaryPositionInMeshFace,
        -1,
        changes.addedFaceIndexInMesh( newFacesCirculator.currentFaceIndex() )
      } ) );

      changes.mFacesNeighborhoodToAdd[newFacesCirculator.currentFaceIndex()][boundaryPositionInNewFace] = meshCirculator.currentFaceIndex();
    }
  }

  for ( int f = 0; f < changes.mFacesToAdd.count(); ++f )
    for ( int n = 0; n < changes.mFacesToAdd.at( f ).count(); ++n )
      if ( changes.mFacesNeighborhoodToAdd.at( f ).at( n ) == -1 )
        changes.mFacesNeighborhoodToAdd[f][n] = changes.addedFaceIndexInMesh( topologicalFaces.mFacesNeighborhood.at( f ).at( n ) );

  const QList<int> &verticesToFaceToChange = topologicalFaces.mVerticesToFace.keys();
  for ( const int vtc : verticesToFaceToChange )
    if ( mVertexToFace.at( vtc ) == -1 )
      changes.mVerticesToFaceChanges.append( {vtc,
                                              mVertexToFace.at( vtc ),
                                              changes.addedFaceIndexInMesh( topologicalFaces.mVerticesToFace.value( vtc ) ) } );

  applyChanges( changes );

  return changes;
}

void QgsTopologicalMesh::applyChanges( const QgsTopologicalMesh::Changes &changes )
{
  int initialVerticesCount = mMesh->vertices.count();
  if ( !changes.mVerticesToAdd.empty() )
  {
    int newSize = mMesh->vertices.count() + changes.mVerticesToAdd.count();
    mMesh->vertices.resize( newSize );
    mVertexToFace.resize( newSize );
  }

  if ( !changes.mFacesToAdd.empty() > 0 )
  {
    int newSize = mMesh->faceCount() + changes.mFacesToAdd.count();
    mMesh->faces.resize( newSize );
    mFacesNeighborhood.resize( newSize );
  }

  for ( int i = 0; i < changes.mFacesToRemove.count(); ++i )
  {
    mMesh->faces[changes.removedFaceIndexInmesh( i )] = QgsMeshFace();
    mFacesNeighborhood[changes.removedFaceIndexInmesh( i )] = FaceNeighbors();//changes.facesNeighborhoodToRemove[i];
  }

  for ( int i = 0; i < changes.mVerticesToRemoveIndexes.count(); ++i )
  {
    int vertexIndex = changes.mVerticesToRemoveIndexes.at( i );
    if ( mVertexToFace.at( vertexIndex ) == -1 )
      dereferenceAsFreeVertex( vertexIndex );
    mMesh->vertices[vertexIndex] = QgsMeshVertex();
    mVertexToFace[vertexIndex] = -1;
  }

  for ( int i = 0; i < changes.mVerticesToAdd.count(); ++i )
  {
    mMesh->vertices[initialVerticesCount + i] = changes.mVerticesToAdd.at( i );
    mVertexToFace[initialVerticesCount + i] = changes.mVertexToFaceToAdd.at( i );
    if ( changes.mVertexToFaceToAdd.at( i ) == -1 )
      referenceAsFreeVertex( initialVerticesCount + i );
  }

  for ( int i = 0; i < changes.mFacesToAdd.count(); ++i )
  {
    mMesh->faces[changes.addedFaceIndexInMesh( i )] = changes.mFacesToAdd.at( i );
    mFacesNeighborhood[changes.addedFaceIndexInMesh( i )] = changes.mFacesNeighborhoodToAdd.at( i );
  }

  for ( const std::array<int, 4> neigborChange : std::as_const( changes.mNeighborhoodChanges ) )
  {
    const int faceIndex = neigborChange.at( 0 );
    const int positionInFace = neigborChange.at( 1 );
    const int valueToApply = neigborChange.at( 3 );
    mFacesNeighborhood[faceIndex][positionInFace] = valueToApply;
  }

  for ( const std::array<int, 3> vertexToFaceChange : std::as_const( changes.mVerticesToFaceChanges ) )
  {
    int vertexIndex = vertexToFaceChange.at( 0 );
    mVertexToFace[vertexToFaceChange.at( 0 )] = vertexToFaceChange.at( 2 );

    if ( vertexToFaceChange.at( 2 ) == -1 &&
         vertexToFaceChange.at( 1 ) != -1 &&
         !mMesh->vertices.at( vertexIndex ).isEmpty() )
      referenceAsFreeVertex( vertexIndex );

    if ( vertexToFaceChange.at( 1 ) == -1 && vertexToFaceChange.at( 2 ) != -1 )
      dereferenceAsFreeVertex( vertexIndex );
  }

  for ( int i = 0; i < changes.mChangeCoordinateVerticesIndexes.count(); ++i )
  {
    int vertexIndex = changes.mChangeCoordinateVerticesIndexes.at( i );
    if ( !changes.mNewZValues.isEmpty() )
      mMesh->vertices[vertexIndex].setZ( changes.mNewZValues.at( i ) );
    if ( !changes.mNewXYValues.isEmpty() )
    {
      const QgsPointXY &pt = changes.mNewXYValues.at( i );
      mMesh->vertices[vertexIndex].setX( pt.x() );
      mMesh->vertices[vertexIndex].setY( pt.y() );
    }
  }
}

void QgsTopologicalMesh::reverseChanges( const QgsTopologicalMesh::Changes &changes )
{
  for ( const std::array<int, 4> neigborChange : std::as_const( changes.mNeighborhoodChanges ) )
  {
    const int faceIndex = neigborChange.at( 0 );
    const int positionInFace = neigborChange.at( 1 );
    const int valueToApply = neigborChange.at( 2 );
    mFacesNeighborhood[faceIndex][positionInFace] = valueToApply;
  }

  for ( int i = 0; i < changes.mFacesToRemove.count(); ++i )
  {
    mMesh->faces[changes.removedFaceIndexInmesh( i )] = changes.mFacesToRemove.at( i );
    mFacesNeighborhood[changes.removedFaceIndexInmesh( i )] = changes.mFacesNeighborhoodToRemove.at( i );
  }

  for ( int i = 0; i < changes.mVerticesToRemoveIndexes.count(); ++i )
  {
    int vertexIndex = changes.mVerticesToRemoveIndexes.at( i );
    mMesh->vertices[vertexIndex] = changes.mRemovedVertices.at( i );
    mVertexToFace[vertexIndex] = changes.mVerticesToFaceRemoved.at( i );
    if ( mVertexToFace.at( vertexIndex ) == -1 )
      referenceAsFreeVertex( vertexIndex );
  }

  int verticesToFaceChangesCount = changes.mVerticesToFaceChanges.count();
  for ( int i = 0; i < verticesToFaceChangesCount; ++i )
  {
    const std::array<int, 3> vertexToFaceChange = changes.mVerticesToFaceChanges.at( verticesToFaceChangesCount - i - 1 );
    int vertexIndex = vertexToFaceChange.at( 0 );
    mVertexToFace[vertexIndex] = vertexToFaceChange.at( 1 );

    if ( vertexToFaceChange.at( 2 ) == -1 && vertexToFaceChange.at( 1 ) != -1 )
      dereferenceAsFreeVertex( vertexIndex );

    if ( vertexToFaceChange.at( 1 ) == -1 &&
         vertexToFaceChange.at( 2 ) != -1 &&
         !mMesh->vertex( vertexIndex ).isEmpty() )
      referenceAsFreeVertex( vertexIndex );
  }

  if ( !changes.mFacesToAdd.empty() > 0 )
  {
    int newSize = mMesh->faceCount() - changes.mFacesToAdd.count();
    mMesh->faces.resize( newSize );
    mFacesNeighborhood.resize( newSize );
  }

  if ( !changes.mVerticesToAdd.isEmpty() )
  {
    int newSize = mMesh->vertexCount() - changes.mVerticesToAdd.count();

    for ( int i = newSize; i < mMesh->vertexCount(); ++i )
      if ( mVertexToFace.at( i ) == -1 )
        dereferenceAsFreeVertex( i );

    mMesh->vertices.resize( newSize );
    mVertexToFace.resize( newSize );
  }

  for ( int i = 0; i < changes.mChangeCoordinateVerticesIndexes.count(); ++i )
  {
    int vertexIndex = changes.mChangeCoordinateVerticesIndexes.at( i );
    if ( !changes.mOldZValues.isEmpty() )
      mMesh->vertices[vertexIndex].setZ( changes.mOldZValues.at( i ) );
    if ( !changes.mOldXYValues.isEmpty() )
    {
      const QgsPointXY &pt = changes.mOldXYValues.at( i );
      mMesh->vertices[vertexIndex].setX( pt.x() );
      mMesh->vertices[vertexIndex].setY( pt.y() );
    }
  }
}

QgsMeshVertexCirculator QgsTopologicalMesh::vertexCirculator( int vertexIndex ) const
{
  return QgsMeshVertexCirculator( *this, vertexIndex );
}

bool QgsTopologicalMesh::facesCanBeJoinedWithCommonIndex( const QgsMeshFace &face1, const QgsMeshFace &face2, int commonIndex )
{
  int commonVertexPosition1 = vertexPositionInFace( commonIndex, face1 );
  int commonVertexPosition2 = vertexPositionInFace( commonIndex, face2 );

  bool canBejoined = ( face1.at( ( commonVertexPosition1 + 1 ) % face1.size() ) == face2.at( ( commonVertexPosition2 - 1 + face2.size() ) % face2.size() ) ) ||
                     ( face1.at( ( commonVertexPosition1 - 1 + face1.size() ) % face1.size() ) == face2.at( ( commonVertexPosition2 + 1 ) % face2.size() ) );


  return canBejoined;
}

QSet<int> QgsTopologicalMesh::concernedFacesBy( const QList<int> faceIndexes ) const
{
  QSet<int> faces;
  for ( const int faceIndex : faceIndexes )
  {
    const QgsMeshFace &face = mMesh->face( faceIndex );
    for ( int i = 0; i < face.count(); ++i )
      faces.unite( facesAroundVertex( face.at( i ) ).toSet() );
  }
  return faces;
}

void QgsTopologicalMesh::dereferenceAsFreeVertex( int vertexIndex )
{
  mFreeVertices.remove( vertexIndex );
}

void QgsTopologicalMesh::referenceAsFreeVertex( int vertexIndex )
{
  mFreeVertices.insert( vertexIndex );
}

bool QgsTopologicalMesh::checkConsistency() const
{
  for ( int faceIndex = 0 ; faceIndex < mMesh->faces.count( ); ++faceIndex )
  {
    const QgsMeshFace &face = mMesh->faces.at( faceIndex );
    for ( int i = 0; i < face.count(); ++i )
      if ( mVertexToFace.at( face.at( i ) ) == -1 )
        return false;
  }

  return true;
}

QgsMesh *QgsTopologicalMesh::mesh() const
{
  return mMesh;
}

bool QgsTopologicalMesh::isVertexOnBoundary( int vertexIndex ) const
{
  QgsMeshVertexCirculator circulator = vertexCirculator( vertexIndex );

  if ( circulator.isValid() )
    return circulator.goBoundaryClockwise();

  return false;
}

bool QgsTopologicalMesh::isVertexFree( int vertexIndex ) const
{
  if ( vertexIndex < 0 || vertexIndex >= mMesh->vertexCount() )
    return false;

  if ( mMesh->vertices.at( vertexIndex ).isEmpty() )
    return false;

  return mVertexToFace.at( vertexIndex ) == -1;
}

QList<int> QgsTopologicalMesh::freeVerticesIndexes() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  return mFreeVertices.values();
#else
  return QList<int>( mFreeVertices.begin(), mFreeVertices.end() );
#endif
}

QgsMeshEditingError QgsTopologicalMesh::counterClockWiseFaces( QgsMeshFace &face, QgsMesh *mesh )
{
  // First check if the face is convex and put it counter clockwise
  // If the index are not well ordered (edges intersect), invalid face --> return false
  int faceSize = face.count();
  if ( faceSize < 3 )
    return QgsMeshEditingError( Qgis::MeshEditingErrorType::FlatFace, -1 );

  int direction = 0;
  for ( int i = 0; i < faceSize; ++i )
  {
    int iv0 =  face[i];
    int iv1 = face[( i + 1 ) % faceSize];
    int iv2 = face[( i + 2 ) % faceSize];

    if ( iv0 < 0 || iv0 >= mesh->vertexCount() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv0 );

    if ( iv1 < 0 || iv1 >= mesh->vertexCount() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv1 );

    if ( iv2 < 0 || iv2 >= mesh->vertexCount() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv2 );

    const QgsMeshVertex &v0 = mesh->vertices.at( iv0 ) ;
    const QgsMeshVertex &v1 = mesh->vertices.at( iv1 ) ;
    const QgsMeshVertex &v2 = mesh->vertices.at( iv2 ) ;

    if ( v0.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv0 );

    if ( v1.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv1 );

    if ( v2.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, iv2 );

    double crossProd = crossProduct( iv1, iv0, iv2, *mesh ); //if cross product>0, we have two edges clockwise
    if ( direction != 0 && crossProd * direction < 0 )   // We have a convex face or a (partially) flat face
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, -1 );
    else if ( crossProd == 0 )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::FlatFace, -1 );
    else if ( direction == 0 && crossProd != 0 )
      direction = crossProd / std::fabs( crossProd );
  }

  if ( direction > 0 )// clockwise --> reverse the order of the index;
  {
    for ( int i = 0; i < faceSize / 2; ++i )
    {
      int temp = face[i];
      face[i] = face.at( faceSize - i - 1 );
      face[faceSize - i - 1] = temp;
    }
  }

  return QgsMeshEditingError( Qgis::MeshEditingErrorType::NoError, -1 );
}

void QgsTopologicalMesh::reindex()
{
  QVector<int> oldToNewIndex( mMesh->vertices.count(), -1 );
  int verticesTotalCount = mMesh->vertexCount();
  int oldIndex = 0;
  int newIndex = 0;
  while ( oldIndex < verticesTotalCount )
  {
    if ( mMesh->vertex( oldIndex ).isEmpty() )
    {
      oldIndex++;
    }
    else
    {
      oldToNewIndex[oldIndex] = newIndex;
      if ( oldIndex != newIndex )
        mMesh->vertices[newIndex] = mMesh->vertices[oldIndex];
      oldToNewIndex[oldIndex] = newIndex;
      oldIndex++;
      newIndex++;
    }
  }

  mMesh->vertices.resize( newIndex );

  oldIndex = 0;
  newIndex = 0;
  int facesTotalCount = mMesh->faceCount();
  while ( oldIndex < facesTotalCount )
  {
    if ( mMesh->face( oldIndex ).isEmpty() )
      oldIndex++;
    else
    {
      if ( oldIndex != newIndex )
        mMesh->faces[newIndex] = mMesh->faces[oldIndex];
      QgsMeshFace &face = mMesh->faces[newIndex];
      for ( int i = 0; i < face.count(); ++i )
        face[i] = oldToNewIndex[face.at( i )];
      newIndex++;
      oldIndex++;
    }
  }

  mMesh->faces.resize( newIndex );

  mVertexToFace.clear();
  mFacesNeighborhood.clear();
}

QVector<QgsMeshFace> QgsTopologicalMesh::Changes::addedFaces() const
{
  return mFacesToAdd;
}

QVector<QgsMeshFace> QgsTopologicalMesh::Changes::removedFaces() const
{
  return mFacesToRemove;
}

QList<int> QgsTopologicalMesh::Changes::removedFaceIndexes() const
{
  return mFaceIndexesToRemove;
}

QVector<QgsMeshVertex> QgsTopologicalMesh::Changes::addedVertices() const
{
  return mVerticesToAdd;
}

QList<int> QgsTopologicalMesh::Changes::changedCoordinatesVerticesIndexes() const
{
  return mChangeCoordinateVerticesIndexes;
}

QList<double> QgsTopologicalMesh::Changes::newVerticesZValues() const
{
  return mNewZValues;
}

QList<QgsPointXY> QgsTopologicalMesh::Changes::newVerticesXYValues() const
{
  return mNewXYValues;
}

QList<QgsPointXY> QgsTopologicalMesh::Changes::oldVerticesXYValues() const
{
  return mOldXYValues;
}

QList<int> QgsTopologicalMesh::Changes::nativeFacesIndexesGeometryChanged() const
{
  return mNativeFacesIndexesGeometryChanged;
}

int QgsTopologicalMesh::Changes::addedFaceIndexInMesh( int internalIndex ) const
{
  if ( internalIndex == -1 )
    return -1;

  return internalIndex + mAddedFacesFirstIndex;
}

int QgsTopologicalMesh::Changes::removedFaceIndexInmesh( int internalIndex ) const
{
  if ( internalIndex == -1 )
    return -1;

  return mFaceIndexesToRemove.at( internalIndex );
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::addFreeVertex( const QgsMeshVertex &vertex )
{
  Changes changes;
  changes.mVerticesToAdd.append( vertex );
  changes.mVertexToFaceToAdd.append( -1 );

  mMesh->vertices.append( vertex );
  mVertexToFace.append( -1 );
  referenceAsFreeVertex( mMesh->vertices.count() - 1 );

  return changes;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::removeVertexFillHole( int vertexIndex )
{
  if ( mVertexToFace.at( vertexIndex ) == -1 ) //it is a free vertex
  {
    Changes changes;
    changes.mRemovedVertices.append( mMesh->vertices.at( vertexIndex ) );
    changes.mVerticesToRemoveIndexes.append( vertexIndex );
    changes.mVerticesToFaceRemoved.append( -1 );
    dereferenceAsFreeVertex( vertexIndex );
    mMesh->vertices[vertexIndex] = QgsMeshVertex();
    return changes;
  }

  //search concerned faces
  QgsMeshVertexCirculator circulator = vertexCirculator( vertexIndex );
  circulator.goBoundaryClockwise();
  QList<int> boundariesVertexIndex;
  QList<int> associateFaceToBoundaries;
  QList<int> removedFacesIndexes;
  do
  {
    removedFacesIndexes.append( circulator.currentFaceIndex() );
    boundariesVertexIndex.append( circulator.oppositeVertexClockWise() );
    const QgsMeshFace &currentFace = circulator.currentFace();
    associateFaceToBoundaries.append( mFacesNeighborhood.at( circulator.currentFaceIndex() ).at(
                                        vertexPositionInFace( boundariesVertexIndex.last(), currentFace ) ) );

    if ( currentFace.count() > 3 ) // quad or more, need other vertices
    {
      int posInface = vertexPositionInFace( vertexIndex, currentFace );
      for ( int i = 2; i < currentFace.count() - 1; ++i )
      {
        boundariesVertexIndex.append( currentFace.at( ( posInface + i ) % currentFace.count() ) );
        associateFaceToBoundaries.append( mFacesNeighborhood.at( circulator.currentFaceIndex() ).at(
                                            vertexPositionInFace( boundariesVertexIndex.last(), currentFace ) ) );
      }
    }
  }
  while ( circulator.turnCounterClockwise() != -1 && circulator.currentFaceIndex() != removedFacesIndexes.first() );

  bool boundaryFill = false;
  if ( circulator.currentFaceIndex() == -1 ) //we are on boundary of the mesh
  {
    boundaryFill = true;
    //we need to add last vertex/boundary faces that was not added because we are on mesh boundary
    circulator.goBoundaryCounterClockwise();
    boundariesVertexIndex.append( circulator.oppositeVertexCounterClockWise() );
    associateFaceToBoundaries.append( -1 );

  }

  int currentVertexToFace = mVertexToFace.at( vertexIndex );
  // here, we use the method removeFaces that effectivly removes and then constructs changes
  Changes changes = removeFaces( removedFacesIndexes );
  changes.mRemovedVertices.append( mMesh->vertices.at( vertexIndex ) );
  changes.mVerticesToRemoveIndexes.append( vertexIndex );
  changes.mVerticesToFaceRemoved.append( currentVertexToFace );
  // these changes contain information that will lead to reference the removed vertex as free vertex when reverse/reapply
  dereferenceAsFreeVertex( vertexIndex );
  mMesh->vertices[vertexIndex] = QgsMeshVertex();
  mVertexToFace[vertexIndex] = -1;

  QList<QList<int>> holes;
  QList<QList<int>> associateMeshFacesToHoles;
  if ( boundaryFill )
  {
    // The hole is not a closed polygon, we need to close it, but the closing segment can intersect another segment.
    // In this case we consider as many polygons as necessary.
    while ( boundariesVertexIndex.count() > 1 )
    {
      int concavePointPos = -1;
      const QgsPoint &p1 = mMesh->vertices.at( boundariesVertexIndex.first() );
      const QgsPoint *p2 = &mMesh->vertices.at( boundariesVertexIndex.last() );
      for ( int i = 1; i < boundariesVertexIndex.count() - 1; ++i )
      {
        const QgsPoint *pointToTest = &mMesh->vertices.at( boundariesVertexIndex.at( i ) );
        if ( QgsGeometryUtils::leftOfLine( *pointToTest, p1, *p2 ) < 0 )
        {
          concavePointPos = i;
          p2 = pointToTest;
        }
      }

      if ( concavePointPos == -1 ) //all boundaries are concave
      {
        holes.append( boundariesVertexIndex );
        associateMeshFacesToHoles.append( associateFaceToBoundaries );
        break;
      }
      else if ( concavePointPos == 1 ) //we don't have concave boundarie from the first point
      {
        boundariesVertexIndex.removeFirst();
        associateFaceToBoundaries.removeFirst();
      }
      else  //concave point between first and last --> split
      {
        QList<int> partialHoleVertexIndex = boundariesVertexIndex.mid( 0, concavePointPos + 1 );
        QList<int> partialAssociateFaces = associateFaceToBoundaries.mid( 0, concavePointPos + 1 );
        holes.append( partialHoleVertexIndex );
        partialAssociateFaces[partialAssociateFaces.count() - 1] = -1;
        associateMeshFacesToHoles.append( partialAssociateFaces );
        boundariesVertexIndex = boundariesVertexIndex.mid( concavePointPos );
        associateFaceToBoundaries = associateFaceToBoundaries.mid( concavePointPos );
      }
    }
  }
  else
  {
    holes.append( boundariesVertexIndex );
    associateMeshFacesToHoles.append( associateFaceToBoundaries );
  }
  Q_ASSERT( holes.count() == associateMeshFacesToHoles.count() );

  int oldFacesCount = mMesh->faceCount();
  for ( int i = 0; i < holes.count(); ++i )
  {
    const QList<int> &holeVertices = holes.at( i );
    const QList<int> &associateMeshFacesToHole = associateMeshFacesToHoles.at( i );
    QHash<p2t::Point *, int> mapPoly2TriPointToVertex;
    std::vector<p2t::Point *> holeToFill( holeVertices.count() );
    for ( int i = 0; i < holeVertices.count(); ++i )
    {
      const QgsMeshVertex &vertex = mMesh->vertex( holeVertices.at( i ) );
      holeToFill[i] = new p2t::Point( vertex.x(), vertex.y() );
      mapPoly2TriPointToVertex.insert( holeToFill[i], holeVertices.at( i ) );
    }

    std::unique_ptr<p2t::CDT> cdt( new p2t::CDT( holeToFill ) );
    try
    {
      cdt->Triangulate();
      std::vector<p2t::Triangle *> triangles = cdt->GetTriangles();
      QVector<QgsMeshFace> newFaces( triangles.size() );
      for ( size_t i = 0; i < triangles.size(); ++i )
      {
        QgsMeshFace &face = newFaces[i];
        face.resize( 3 );
        for ( int j = 0; j < 3; j++ )
        {
          int vertInd = mapPoly2TriPointToVertex.value( triangles.at( i )->GetPoint( j ), -1 );
          if ( vertInd == -1 )
            throw std::exception();
          Q_ASSERT( !mMesh->vertices.at( vertInd ).isEmpty() );
          face[j] = vertInd;
        }
      }

      QgsMeshEditingError error;
      QgsTopologicalMesh::TopologicalFaces topologicalFaces = createNewTopologicalFaces( newFaces, error );
      if ( error.errorType != Qgis::MeshEditingErrorType::NoError )
        throw std::exception();
      int newFaceIndexStartIndex = mMesh->faceCount();
      QgsTopologicalMesh::Changes addChanges;
      addChanges.mAddedFacesFirstIndex = newFaceIndexStartIndex;
      addChanges.mFacesToAdd = topologicalFaces.meshFaces();
      addChanges.mFacesNeighborhoodToAdd = topologicalFaces.mFacesNeighborhood;

      // vertices to face changes
      const QList<int> &verticesToFaceToChange = topologicalFaces.mVerticesToFace.keys();
      for ( const int vtc : verticesToFaceToChange )
        if ( mVertexToFace.at( vtc ) == -1 )
          addChanges.mVerticesToFaceChanges.append( {vtc,
              mVertexToFace.at( vtc ),
              addChanges.addedFaceIndexInMesh( topologicalFaces.mVerticesToFace.value( vtc ) ) } );


      // reindex neighborhood for new faces
      for ( int i = 0; i < topologicalFaces.mFaces.count(); ++i )
      {
        FaceNeighbors &faceNeighbors = addChanges.mFacesNeighborhoodToAdd[i];
        faceNeighbors = topologicalFaces.mFacesNeighborhood.at( i );
        for ( int n = 0; n < faceNeighbors.count(); ++n )
        {
          if ( faceNeighbors.at( n ) != -1 )
            faceNeighbors[n] += newFaceIndexStartIndex; //reindex internal neighborhood
        }
      }

      // link neighborhood for boundaries of each side
      for ( int i = 0 ; i < holeVertices.count(); ++i )
      {
        int vertexHoleIndex = holeVertices.at( i );

        int meshFaceBoundaryIndex = associateMeshFacesToHole.at( i );

        QgsMeshVertexCirculator circulator = QgsMeshVertexCirculator( topologicalFaces, vertexHoleIndex );
        circulator.goBoundaryClockwise();
        int newFaceBoundaryLocalIndex = circulator.currentFaceIndex();
        int newFaceBoundaryIndexInMesh = circulator.currentFaceIndex() + newFaceIndexStartIndex;
        const QgsMeshFace &newFace = circulator.currentFace();
        int positionInNewFaces = vertexPositionInFace( vertexHoleIndex, newFace );

        if ( meshFaceBoundaryIndex != -1 )
        {
          const QgsMeshFace meshFace = mMesh->face( meshFaceBoundaryIndex );
          int positionInMeshFaceBoundary = vertexPositionInFace( *mMesh, vertexHoleIndex, meshFaceBoundaryIndex );
          positionInMeshFaceBoundary = ( positionInMeshFaceBoundary - 1 + meshFace.count() ) % meshFace.count(); //take the position just before

          addChanges.mNeighborhoodChanges.append( {meshFaceBoundaryIndex, positionInMeshFaceBoundary, -1, newFaceBoundaryIndexInMesh} );
        }

        addChanges.mFacesNeighborhoodToAdd[newFaceBoundaryLocalIndex][positionInNewFaces] = meshFaceBoundaryIndex;
      }

      applyChanges( addChanges );

      changes.mFacesToAdd.append( addChanges.mFacesToAdd );
      changes.mFacesNeighborhoodToAdd.append( addChanges.mFacesNeighborhoodToAdd );
      //for each neighborhood change, check if a corresponding change already exist and merge them, if not just append
      for ( const std::array<int, 4> &neighborChangeToAdd : std::as_const( addChanges.mNeighborhoodChanges ) )
      {
        bool merged = false;
        for ( std::array<int, 4> &existingNeighborChange : changes.mNeighborhoodChanges )
        {
          if ( existingNeighborChange.at( 0 ) == neighborChangeToAdd.at( 0 ) &&
               existingNeighborChange.at( 1 ) == neighborChangeToAdd.at( 1 ) )
          {
            merged = true;
            Q_ASSERT( existingNeighborChange.at( 3 ) == neighborChangeToAdd.at( 2 ) );
            existingNeighborChange[3] = neighborChangeToAdd.at( 3 );
          }
        }
        if ( !merged )
          changes.mNeighborhoodChanges.append( neighborChangeToAdd );
      }
      //for each vertex to face change, check if a corresponding change already exist and merge them, if not just append
      for ( const std::array<int, 3> &verticesToFaceToAdd : std::as_const( addChanges.mVerticesToFaceChanges ) )
      {
        bool merged = false;
        for ( std::array<int, 3> &existingVerticesToFace : changes.mVerticesToFaceChanges )
        {
          if ( existingVerticesToFace.at( 0 ) == verticesToFaceToAdd.at( 0 ) )
          {
            merged = true;
            Q_ASSERT( existingVerticesToFace.at( 2 ) == verticesToFaceToAdd.at( 1 ) );
            existingVerticesToFace[2] = verticesToFaceToAdd.at( 2 );
          }
        }
        if ( !merged )
          changes.mVerticesToFaceChanges.append( verticesToFaceToAdd );
      }
    }
    catch ( ... )
    {
      QgsMessageLog::logMessage( QObject::tr( "Triangulation failed. Skipping hole…" ), QObject::tr( "Mesh Editing" ) );
    }

    qDeleteAll( holeToFill );
  }

  changes.mAddedFacesFirstIndex = oldFacesCount;

  return changes;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::removeVertices( const QList<int> &vertices )
{
  QSet<int> facesIndex;
  //Search for associated faces
  for ( int vertexIndex : vertices )
    facesIndex.unite( facesAroundVertex( vertexIndex ).toSet() );

  // remove the faces
  Changes changes = removeFaces( facesIndex.values() );

  // removes the vertices
  for ( int vertexIndex : vertices )
  {
    int currentVertexToFace = mVertexToFace.at( vertexIndex );
    // here, we use the method removeFaces that effectivly removes and then constructs changes
    changes.mRemovedVertices.append( mMesh->vertices.at( vertexIndex ) );
    changes.mVerticesToRemoveIndexes.append( vertexIndex );
    changes.mVerticesToFaceRemoved.append( currentVertexToFace );
    // these changes contain information that will lead to reference the removed vertex as free vertex when reverse/reapply
    dereferenceAsFreeVertex( vertexIndex );
    mMesh->vertices[vertexIndex] = QgsMeshVertex();
    mVertexToFace[vertexIndex] = -1;
  }

  return changes;
}

QgsMeshEditingError QgsTopologicalMesh::canFacesBeAdded( const QgsTopologicalMesh::TopologicalFaces &topologicFaces ) const
{
  QList<int> boundaryToCheck = topologicFaces.mBoundaries;

  // go through the boundary to check if there is a unique shared vertex
  while ( !boundaryToCheck.isEmpty() )
  {
    int boundary = boundaryToCheck.takeLast();

    //if the boundary id a free vertex in the destination mesh, no need to check
    if ( mVertexToFace.at( boundary ) == -1 )
      continue;

    int faceIndex = topologicFaces.mVerticesToFace.value( boundary );

    QgsMeshVertexCirculator newFacescirculator( topologicFaces, faceIndex, boundary );
    QgsMeshVertexCirculator meshCirculator = vertexCirculator( boundary );

    if ( !newFacescirculator.isValid() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, faceIndex );

    //search for face boundary on clockwise side of new faces
    if ( !newFacescirculator.goBoundaryClockwise() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, faceIndex );
    const QgsMeshFace &newFaceBoundaryCW = newFacescirculator.currentFace();
    if ( newFaceBoundaryCW.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, faceIndex );

    //search for face boundary on COUNTER clockwise side of existing faces
    if ( !meshCirculator.goBoundaryCounterClockwise() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, boundary );
    const QgsMeshFace &existingFaceBoundaryCCW = meshCirculator.currentFace();
    if ( existingFaceBoundaryCCW.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, boundary );

    if ( facesCanBeJoinedWithCommonIndex( newFaceBoundaryCW, existingFaceBoundaryCCW, boundary ) )
    {
      // Face can be joined
      // Remove other common vertex before continuing
      boundaryToCheck.removeOne( newFacescirculator.oppositeVertexClockWise() );
      continue;
    }

    // Now check with other direction

    //search for face boundary on COUNTER clockwise side of new faces
    if ( !newFacescirculator.goBoundaryCounterClockwise() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, faceIndex );
    const QgsMeshFace &newFaceBoundaryCCW = newFacescirculator.currentFace();
    if ( newFaceBoundaryCCW.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, faceIndex );

    //search for face boundary on clockwise side of existing faces
    if ( !meshCirculator.goBoundaryClockwise() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, boundary );
    const QgsMeshFace &existingFaceBoundaryCW = meshCirculator.currentFace();
    if ( existingFaceBoundaryCW.isEmpty() )
      return QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidVertex, boundary );

    if ( facesCanBeJoinedWithCommonIndex( newFaceBoundaryCCW, existingFaceBoundaryCW, boundary ) )
    {
      // Face can be joined
      // Remove other common vertex before continuing
      boundaryToCheck.removeOne( newFacescirculator.oppositeVertexCounterClockWise() );
      continue;
    }

    //if we are here, face share only one vertices
    return QgsMeshEditingError( Qgis::MeshEditingErrorType::UniqueSharedVertex, boundary );
  }

  return QgsMeshEditingError();
}

void QgsTopologicalMesh::TopologicalFaces::clear()
{
  mFaces.clear();
  mFacesNeighborhood.clear();
  mVerticesToFace.clear();
  mBoundaries.clear();
}

QgsTopologicalMesh QgsTopologicalMesh::createTopologicalMesh( QgsMesh *mesh, int maxVerticesPerFace, QgsMeshEditingError &error )
{
  QgsTopologicalMesh topologicMesh;
  topologicMesh.mMesh = mesh;
  topologicMesh.mVertexToFace = QVector<int>( mesh->vertexCount(), -1 );
  topologicMesh.mMaximumVerticesPerFace = maxVerticesPerFace;
  error.errorType = Qgis::MeshEditingErrorType::NoError;

  for ( int i = 0; i < mesh->faceCount(); ++i )
  {
    if ( maxVerticesPerFace != 0 && mesh->face( i ).count() > maxVerticesPerFace )
    {
      error = QgsMeshEditingError( Qgis::MeshEditingErrorType::InvalidFace, i );
      break;
    }

    error = counterClockWiseFaces( mesh->faces[i], mesh );
    if ( error.errorType != Qgis::MeshEditingErrorType::NoError )
    {
      if ( error.errorType == Qgis::MeshEditingErrorType::InvalidFace || error.errorType == Qgis::MeshEditingErrorType::FlatFace )
        error.elementIndex = i;
      break;
    }
  }

  if ( error.errorType == Qgis::MeshEditingErrorType::NoError )
  {
    TopologicalFaces subMesh = topologicMesh.createTopologicalFaces( mesh->faces, error, false, true );
    topologicMesh.mFacesNeighborhood = subMesh.mFacesNeighborhood;

    for ( int i = 0; i < topologicMesh.mMesh->vertexCount(); ++i )
    {
      if ( topologicMesh.mVertexToFace.at( i ) == -1 )
        topologicMesh.mFreeVertices.insert( i );
    }
  }

  return topologicMesh;
}

QgsTopologicalMesh::TopologicalFaces QgsTopologicalMesh::createNewTopologicalFaces( const QVector<QgsMeshFace> &faces, QgsMeshEditingError &error )
{
  return createTopologicalFaces( faces, error, true, false );
}


QgsTopologicalMesh::TopologicalFaces QgsTopologicalMesh::createTopologicalFaces(
  const QVector<QgsMeshFace> &faces,
  QgsMeshEditingError &error,
  bool allowUniqueSharedVertex,
  bool writeInVertices )
{
  int facesCount = faces.count();
  QVector<FaceNeighbors> faceTopologies;
  QHash<int, int> verticesToFace;

  // Contains for each vertex a map (opposite vertex # edge) --> face index
  // when turning counter clockwise if, v1 first vertex and v2 second one, [v1][v2]--> neighbor face
  QMap<int, QMap<int, int>> verticesToNeighbor;

  for ( int faceIndex = 0; faceIndex < facesCount; ++faceIndex )
  {
    const QgsMeshFace &face = faces.at( faceIndex );
    int faceSize = face.count();
    //Fill vertices to neighbor faces
    for ( int i = 0; i < faceSize; ++i )
    {
      int v1 = face[i % faceSize];
      int v2 = face[( i + 1 ) % faceSize];
      verticesToNeighbor[v2].insert( v1, faceIndex );
    }
  }

  faceTopologies = QVector<FaceNeighbors>( faces.count() );

  QSet<int> boundaryVertices;
  TopologicalFaces ret;

  for ( int faceIndex = 0; faceIndex < facesCount; ++faceIndex )
  {
    const QgsMeshFace &face = faces.at( faceIndex );
    int faceSize = face.size();
    FaceNeighbors &faceTopology = faceTopologies[faceIndex];
    faceTopology.resize( faceSize );

    for ( int i = 0; i < faceSize; ++i )
    {
      int v1 = face.at( i );
      int v2 = face.at( ( i + 1 ) % faceSize );

      if ( writeInVertices )
      {
        if ( mVertexToFace[v1] == -1 )
          mVertexToFace[v1] = faceIndex ;
      }
      else
      {
        if ( !verticesToFace.contains( v1 ) )
          verticesToFace[v1] = faceIndex ;
      }

      QMap<int, int> &edges = verticesToNeighbor[v1];
      if ( edges.contains( v2 ) )
        faceTopology[i] = edges.value( v2 );
      else
      {
        faceTopology[i] = -1;

        if ( !allowUniqueSharedVertex )
        {
          if ( boundaryVertices.contains( v1 ) )
          {
            error = QgsMeshEditingError( Qgis::MeshEditingErrorType::UniqueSharedVertex, v1 ); // if a vertices is more than one time in the boundary, that means faces share only one vertices
            return ret;
          }
        }
        boundaryVertices.insert( v1 );
      }
    }
  }

  ret.mFaces = faces;
  ret.mFacesNeighborhood = faceTopologies;
  ret.mBoundaries = boundaryVertices.values();
  ret.mVerticesToFace = verticesToFace;
  return ret;
}

QList<int> QgsTopologicalMesh::neighborsOfFace( int faceIndex ) const
{
  return mFacesNeighborhood.at( faceIndex ).toList();
}

QList<int> QgsTopologicalMesh::facesAroundVertex( int vertexIndex ) const
{
  QgsMeshVertexCirculator circ = vertexCirculator( vertexIndex );

  QList<int> ret;
  if ( !circ.isValid() || circ.currentFaceIndex() == -1 )
    return ret;

  // try ccw first
  ret.append( circ.currentFaceIndex() );
  while ( circ.turnCounterClockwise() != ret.first() && circ.currentFaceIndex() != -1 )
  {
    ret.append( circ.currentFaceIndex() );
  }

  if ( circ.currentFaceIndex() == -1 )  //we encounter a boundary, restart with other direction
  {
    ret.clear();
    if ( circ.turnClockwise() == -1 )
      return ret;
    ret.append( circ.currentFaceIndex() );
    while ( circ.turnClockwise() != -1 )
    {
      ret.append( circ.currentFaceIndex() );
    }
  }

  return ret;
}

QgsMeshEditingError QgsTopologicalMesh::canFacesBeRemoved( const QList<int> facesIndexes )
{
  QSet<int> removedFaces = facesIndexes.toSet();
  QSet<int> concernedFaces = concernedFacesBy( facesIndexes );

  for ( const int f : std::as_const( removedFaces ) )
    concernedFaces.remove( f );

  QVector<QgsMeshFace> remainingFaces;
  remainingFaces.reserve( concernedFaces.count() );
  for ( const int f : std::as_const( concernedFaces ) )
    remainingFaces.append( mMesh->face( f ) );

  QgsMeshEditingError error;
  createTopologicalFaces( remainingFaces, error, false, false );

  return error;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::removeFaces( const QList<int> facesIndexesToRemove )
{
  Changes changes;
  changes.mFaceIndexesToRemove = facesIndexesToRemove;
  changes.mFacesToRemove.resize( facesIndexesToRemove.count() );
  changes.mFacesNeighborhoodToRemove.resize( facesIndexesToRemove.count() );

  QSet<int> indexSet = facesIndexesToRemove.toSet();
  QSet<int> threatedVertex;

  for ( int i = 0; i < facesIndexesToRemove.count(); ++i )
  {
    const int faceIndex = facesIndexesToRemove.at( i );
    const QgsMeshFace &face = mMesh->face( faceIndex );
    changes.mFacesToRemove[i] = face;
    const FaceNeighbors &neighborhood = mFacesNeighborhood.at( faceIndex );
    changes.mFacesNeighborhoodToRemove[i] = neighborhood;
    for ( int j = 0; j < face.count(); ++j )
    {
      //change the neighborhood for each neighbor face
      int neighborIndex = neighborhood.at( j );
      if ( neighborIndex != -1 && !indexSet.contains( neighborIndex ) )
      {
        int positionInNeighbor = mFacesNeighborhood.at( neighborIndex ).indexOf( faceIndex );
        changes.mNeighborhoodChanges.append( {neighborIndex, positionInNeighbor, faceIndex, -1} );
      }

      //change vertexToFace
      int vertexIndex = face.at( j );
      if ( !threatedVertex.contains( vertexIndex ) && indexSet.contains( mVertexToFace.at( vertexIndex ) ) )
      {
        int oldValue = mVertexToFace.at( vertexIndex );
        //look for another face linked to this vertex
        int refValue = -1;
        if ( neighborIndex != -1 && !indexSet.contains( neighborIndex ) ) //if exist, simpler to take it
          refValue = neighborIndex;
        else
        {
          QList<int> aroundFaces = facesAroundVertex( vertexIndex );
          aroundFaces.removeOne( faceIndex );
          if ( !aroundFaces.isEmpty() )
          {
            while ( !aroundFaces.isEmpty() && refValue == -1 )
            {
              if ( !indexSet.contains( aroundFaces.first() ) )
                refValue = aroundFaces.first();
              else
                aroundFaces.removeFirst();
            }
          }
        }
        changes.mVerticesToFaceChanges.append( {vertexIndex, oldValue, refValue} );
        threatedVertex.insert( vertexIndex );
      }
    }
  }

  applyChanges( changes );

  return changes;
}

bool QgsTopologicalMesh::eitherSideFacesAndVertices( int vertexIndex1,
    int vertexIndex2,
    int &face1,
    int &face2,
    int &neighborVertex1InFace1,
    int &neighborVertex1InFace2,
    int &neighborVertex2inFace1,
    int &neighborVertex2inFace2 ) const
{
  QgsMeshVertexCirculator circulator1 = vertexCirculator( vertexIndex1 );
  QgsMeshVertexCirculator circulator2 = vertexCirculator( vertexIndex2 );

  circulator1.goBoundaryClockwise();
  int firstFace1 = circulator1.currentFaceIndex();
  circulator2.goBoundaryClockwise();
  int firstFace2 = circulator2.currentFaceIndex();

  if ( circulator1.oppositeVertexCounterClockWise() != vertexIndex2 )
    while ( circulator1.turnCounterClockwise() != -1 &&
            circulator1.currentFaceIndex() != firstFace1 &&
            circulator1.oppositeVertexCounterClockWise() != vertexIndex2 ) {}

  if ( circulator2.oppositeVertexCounterClockWise() != vertexIndex1 )
    while ( circulator2.turnCounterClockwise() != -1 &&
            circulator2.currentFaceIndex() != firstFace2 &&
            circulator2.oppositeVertexCounterClockWise() != vertexIndex1 ) {}

  if ( circulator1.oppositeVertexCounterClockWise() != vertexIndex2
       || circulator2.oppositeVertexCounterClockWise() != vertexIndex1 )
    return false;

  face1 = circulator1.currentFaceIndex();
  face2 = circulator2.currentFaceIndex();

  neighborVertex1InFace1 = circulator1.oppositeVertexClockWise();
  circulator1.turnCounterClockwise();
  neighborVertex1InFace2 = circulator1.oppositeVertexCounterClockWise();

  neighborVertex2inFace2 = circulator2.oppositeVertexClockWise();
  circulator2.turnCounterClockwise();
  neighborVertex2inFace1 = circulator2.oppositeVertexCounterClockWise();

  return true;
}

bool QgsTopologicalMesh::edgeCanBeFlipped( int vertexIndex1, int vertexIndex2 ) const
{
  int faceIndex1;
  int faceIndex2;
  int oppositeVertexFace1;
  int oppositeVertexFace2;
  int supposedOppositeVertexFace1;
  int supposedoppositeVertexFace2;

  bool result = eitherSideFacesAndVertices(
                  vertexIndex1,
                  vertexIndex2,
                  faceIndex1,
                  faceIndex2,
                  oppositeVertexFace1,
                  supposedoppositeVertexFace2,
                  supposedOppositeVertexFace1,
                  oppositeVertexFace2 );

  if ( !result ||
       faceIndex1 < 0 ||
       faceIndex2 < 0 ||
       oppositeVertexFace1 < 0 ||
       oppositeVertexFace2 < 0 ||
       supposedOppositeVertexFace1 != oppositeVertexFace1 ||
       supposedoppositeVertexFace2 != oppositeVertexFace2 )
    return false;

  const QgsMeshFace &face1 = mMesh->face( faceIndex1 );
  const QgsMeshFace &face2 = mMesh->face( faceIndex2 );


  if ( face1.count() != 3 || face2.count() != 3 )
    return false;

  double crossProduct1 = crossProduct( vertexIndex1, oppositeVertexFace1, oppositeVertexFace2, *mMesh );
  double crossProduct2 = crossProduct( vertexIndex2, oppositeVertexFace1, oppositeVertexFace2, *mMesh );

  return crossProduct1 * crossProduct2 < 0;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::flipEdge( int vertexIndex1, int vertexIndex2 )
{
  int faceIndex1;
  int faceIndex2;
  int oppositeVertexFace1;
  int oppositeVertexFace2;
  int supposedOppositeVertexFace1;
  int supposedoppositeVertexFace2;

  bool result = eitherSideFacesAndVertices(
                  vertexIndex1,
                  vertexIndex2,
                  faceIndex1,
                  faceIndex2,
                  oppositeVertexFace1,
                  supposedoppositeVertexFace2,
                  supposedOppositeVertexFace1,
                  oppositeVertexFace2 );

  if ( !result ||
       faceIndex1 < 0 ||
       faceIndex2 < 0 ||
       oppositeVertexFace1 < 0 ||
       oppositeVertexFace2 < 0 ||
       supposedOppositeVertexFace1 != oppositeVertexFace1 ||
       supposedoppositeVertexFace2 != oppositeVertexFace2 )
    return Changes();


  Changes changes;

  const QgsMeshFace &face1 = mMesh->face( faceIndex1 );
  const QgsMeshFace &face2 = mMesh->face( faceIndex2 );

  Q_ASSERT( face1.count() == 3 );
  Q_ASSERT( face2.count() == 3 );

  int pos1 = vertexPositionInFace( vertexIndex1, face1 );
  int pos2 = vertexPositionInFace( vertexIndex2, face2 );

  int neighborFace1 = mFacesNeighborhood.at( faceIndex1 ).at( pos1 );
  int posInNeighbor1 = vertexPositionInFace( *mMesh, oppositeVertexFace1, neighborFace1 );
  int neighborFace2 = mFacesNeighborhood.at( faceIndex1 ).at( ( pos1 + 1 ) % 3 );
  int posInNeighbor2 = vertexPositionInFace( *mMesh, vertexIndex2, neighborFace2 );
  int neighborFace3 = mFacesNeighborhood.at( faceIndex2 ).at( pos2 );
  int posInNeighbor3 = vertexPositionInFace( *mMesh, oppositeVertexFace2, neighborFace3 );
  int neighborFace4 = mFacesNeighborhood.at( faceIndex2 ).at( ( pos2 + 1 ) % 3 );
  int posInNeighbor4 = vertexPositionInFace( *mMesh, vertexIndex1, neighborFace4 );

  changes.mFaceIndexesToRemove.append( faceIndex1 );
  changes.mFaceIndexesToRemove.append( faceIndex2 );
  changes.mFacesToRemove.append( face1 );
  changes.mFacesToRemove.append( face2 );
  changes.mFacesNeighborhoodToRemove.append( mFacesNeighborhood.at( faceIndex1 ) );
  changes.mFacesNeighborhoodToRemove.append( mFacesNeighborhood.at( faceIndex2 ) );
  int startIndex = mMesh->faceCount();
  changes.mAddedFacesFirstIndex = startIndex;
  changes.mFacesToAdd.append( {oppositeVertexFace1, oppositeVertexFace2, vertexIndex1} );
  changes.mFacesToAdd.append( {oppositeVertexFace2, oppositeVertexFace1, vertexIndex2} );
  changes.mFacesNeighborhoodToAdd.append( {startIndex + 1,
                                          mFacesNeighborhood.at( faceIndex2 ).at( ( pos2 + 1 ) % 3 ),
                                          mFacesNeighborhood.at( faceIndex1 ).at( pos1 )} );
  changes.mFacesNeighborhoodToAdd.append( {startIndex,
                                          mFacesNeighborhood.at( faceIndex1 ).at( ( pos1 + 1 ) % 3 ),
                                          mFacesNeighborhood.at( faceIndex2 ).at( pos2 )} );

  if ( neighborFace1 >= 0 )
    changes.mNeighborhoodChanges.append( {neighborFace1, posInNeighbor1, faceIndex1, startIndex} );
  if ( neighborFace2 >= 0 )
    changes.mNeighborhoodChanges.append( {neighborFace2, posInNeighbor2, faceIndex1, startIndex + 1} );
  if ( neighborFace3 >= 0 )
    changes.mNeighborhoodChanges.append( {neighborFace3, posInNeighbor3, faceIndex2, startIndex + 1} );
  if ( neighborFace4 >= 0 )
    changes.mNeighborhoodChanges.append( {neighborFace4, posInNeighbor4, faceIndex2, startIndex} );


  if ( mVertexToFace.at( vertexIndex1 ) == faceIndex1 || mVertexToFace.at( vertexIndex1 ) == faceIndex2 )
    changes.mVerticesToFaceChanges.append( {vertexIndex1,  mVertexToFace.at( vertexIndex1 ), startIndex} );
  if ( mVertexToFace.at( vertexIndex2 ) == faceIndex1 || mVertexToFace.at( vertexIndex2 ) == faceIndex2 )
    changes.mVerticesToFaceChanges.append( {vertexIndex2,  mVertexToFace.at( vertexIndex2 ), startIndex + 1} );

  if ( mVertexToFace.at( oppositeVertexFace1 ) == faceIndex1 )
    changes.mVerticesToFaceChanges.append( {oppositeVertexFace1,  faceIndex1, startIndex} );

  if ( mVertexToFace.at( oppositeVertexFace2 ) == faceIndex2 )
    changes.mVerticesToFaceChanges.append( {oppositeVertexFace2,  faceIndex2, startIndex + 1} );

  applyChanges( changes );

  return changes;
}

bool QgsTopologicalMesh::canBeMerged( int vertexIndex1, int vertexIndex2 ) const
{
  int faceIndex1;
  int faceIndex2;
  int neighborVertex1InFace1;
  int neighborVertex1InFace2;
  int neighborVertex2inFace1;
  int neighborVertex2inFace2;

  bool result = eitherSideFacesAndVertices(
                  vertexIndex1,
                  vertexIndex2,
                  faceIndex1,
                  faceIndex2,
                  neighborVertex1InFace1,
                  neighborVertex1InFace2,
                  neighborVertex2inFace1,
                  neighborVertex2inFace2 );

  if ( !result ||
       faceIndex1 < 0 ||
       faceIndex2 < 0 )
    return false;

  const QgsMeshFace &face1 = mMesh->face( faceIndex1 );
  const QgsMeshFace &face2 = mMesh->face( faceIndex2 );

  if ( face1.count() + face2.count() - 2 > mMaximumVerticesPerFace )
    return false;

  QgsMeshVertex v1 = mMesh->vertices.at( vertexIndex1 );
  QgsMeshVertex v2 = mMesh->vertices.at( vertexIndex2 );
  QgsMeshVertex nv11 = mMesh->vertices.at( neighborVertex1InFace1 );
  QgsMeshVertex nv12 = mMesh->vertices.at( neighborVertex1InFace2 );
  QgsMeshVertex nv21 = mMesh->vertices.at( neighborVertex2inFace1 );
  QgsMeshVertex nv22 = mMesh->vertices.at( neighborVertex2inFace2 );

  double crossProduct1 = crossProduct( vertexIndex1, neighborVertex1InFace1, neighborVertex1InFace2, *mMesh );
  double crossProduct2 = crossProduct( vertexIndex2, neighborVertex2inFace1, neighborVertex2inFace2, *mMesh );

  return crossProduct1 * crossProduct2 < 0;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::merge( int vertexIndex1, int vertexIndex2 )
{
  int faceIndex1;
  int faceIndex2;
  int neighborVertex1InFace1;
  int neighborVertex1InFace2;
  int neighborVertex2inFace1;
  int neighborVertex2inFace2;

  bool result = eitherSideFacesAndVertices(
                  vertexIndex1,
                  vertexIndex2,
                  faceIndex1,
                  faceIndex2,
                  neighborVertex1InFace1,
                  neighborVertex1InFace2,
                  neighborVertex2inFace1,
                  neighborVertex2inFace2 );

  if ( !result ||
       faceIndex1 < 0 ||
       faceIndex2 < 0 )
    return Changes();

  Changes changes;

  const QgsMeshFace &face1 = mMesh->face( faceIndex1 );
  const QgsMeshFace &face2 = mMesh->face( faceIndex2 );
  int faceSize1 = face1.count();
  int faceSize2 = face2.count();

  int pos1 = vertexPositionInFace( vertexIndex1, face1 );
  int pos2 = vertexPositionInFace( vertexIndex2, face2 );

  changes.mFaceIndexesToRemove.append( faceIndex1 );
  changes.mFaceIndexesToRemove.append( faceIndex2 );
  changes.mFacesToRemove.append( face1 );
  changes.mFacesToRemove.append( face2 );
  changes.mFacesNeighborhoodToRemove.append( mFacesNeighborhood.at( faceIndex1 ) );
  changes.mFacesNeighborhoodToRemove.append( mFacesNeighborhood.at( faceIndex2 ) );
  int startIndex = mMesh->faceCount();
  changes.mAddedFacesFirstIndex = startIndex;

  QgsMeshFace newface;
  FaceNeighbors newNeighborhood;
  for ( int i = 0; i < faceSize1 - 1; ++i )
  {
    int currentPos = ( pos1 + i ) % faceSize1;
    newface.append( face1.at( currentPos ) ); //add vertex of face1

    int currentNeighbor = mFacesNeighborhood.at( faceIndex1 ).at( currentPos );
    newNeighborhood.append( currentNeighbor );

    if ( currentNeighbor != -1 )
    {
      int currentPosInNeighbor = vertexPositionInFace( *mMesh, face1.at( ( currentPos + 1 ) % faceSize1 ), currentNeighbor );
      changes.mNeighborhoodChanges.append( {currentNeighbor, currentPosInNeighbor, faceIndex1, startIndex} );
    }
  }
  for ( int i = 0; i < faceSize2 - 1; ++i )
  {
    int currentPos = ( pos2 + i ) % faceSize2;
    newface.append( face2.at( currentPos ) ); //add vertex of face2

    int currentNeighbor = mFacesNeighborhood.at( faceIndex2 ).at( currentPos );
    newNeighborhood.append( currentNeighbor );

    if ( currentNeighbor != -1 )
    {
      int currentPosInNeighbor = vertexPositionInFace( *mMesh, face2.at( ( currentPos + 1 ) % faceSize2 ), currentNeighbor );
      changes.mNeighborhoodChanges.append( {currentNeighbor, currentPosInNeighbor, faceIndex2, startIndex} );
    }
  }

  for ( int i = 0; i < faceSize1; ++i )
    if ( mVertexToFace.at( face1.at( i ) ) == faceIndex1 )
      changes.mVerticesToFaceChanges.append( {face1.at( i ), faceIndex1, startIndex} );

  for ( int i = 0; i < faceSize2; ++i )
    if ( mVertexToFace.at( face2.at( i ) ) == faceIndex2 )
      changes.mVerticesToFaceChanges.append( {face2.at( i ), faceIndex2, startIndex} );

  changes.mFacesToAdd.append( newface );
  changes.mFacesNeighborhoodToAdd.append( newNeighborhood );

  applyChanges( changes );

  return changes;
}

bool QgsTopologicalMesh::faceCanBeSplit( int faceIndex ) const
{
  const QgsMeshFace face = mMesh->face( faceIndex );

  return face.count() == 4;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::splitFace( int faceIndex )
{
  //search for the spliited angle (greater angle)
  const QgsMeshFace &face = mMesh->face( faceIndex );
  int faceSize = face.count();

  Q_ASSERT( faceSize == 4 );

  double maxAngle = 0;
  int splitVertexPos = -1;
  for ( int i = 0; i < faceSize; ++i )
  {
    QgsVector vect1( mMesh->vertex( face.at( i ) ) - mMesh->vertex( face.at( ( i + 1 ) % faceSize ) ) );
    QgsVector vect2( mMesh->vertex( face.at( ( i + 2 ) % faceSize ) ) - mMesh->vertex( face.at( ( i + 1 ) % faceSize ) ) );

    double angle = std::abs( vect1.angle( vect2 ) );
    angle = std::min( angle, 2.0 * M_PI - angle );
    if ( angle > maxAngle )
    {
      maxAngle = angle;
      splitVertexPos = ( i + 1 ) % faceSize;
    }
  }

  Changes changes;

  const QgsMeshFace newFace1 = {face.at( splitVertexPos ),
                                face.at( ( splitVertexPos + 1 ) % faceSize ),
                                face.at( ( splitVertexPos + 2 ) % faceSize )
                               };

  const QgsMeshFace newFace2 = {face.at( splitVertexPos ),
                                face.at( ( splitVertexPos + 2 ) % faceSize ),
                                face.at( ( splitVertexPos + 3 ) % faceSize )
                               };

  QVector<int> neighborIndex( faceSize );
  QVector<int> posInNeighbor( faceSize );

  for ( int i = 0; i < faceSize; ++i )
  {
    neighborIndex[i] = mFacesNeighborhood.at( faceIndex ).at( ( splitVertexPos + i ) % faceSize );
    posInNeighbor[i] = vertexPositionInFace( *mMesh,  face.at( ( splitVertexPos + i + 1 ) % faceSize ), neighborIndex[i] );
  }

  changes.mFaceIndexesToRemove.append( faceIndex );
  changes.mFacesToRemove.append( face );
  changes.mFacesNeighborhoodToRemove.append( mFacesNeighborhood.at( faceIndex ) );
  int startIndex = mMesh->faceCount();
  changes.mAddedFacesFirstIndex = startIndex;
  changes.mFacesToAdd.append( newFace1 );
  changes.mFacesToAdd.append( newFace2 );

  changes.mFacesNeighborhoodToAdd.append( {mFacesNeighborhood.at( faceIndex ).at( splitVertexPos ),
                                          mFacesNeighborhood.at( faceIndex ).at( ( splitVertexPos + 1 ) % faceSize ),
                                          startIndex + 1} );
  changes.mFacesNeighborhoodToAdd.append( {startIndex,
                                          mFacesNeighborhood.at( faceIndex ).at( ( splitVertexPos + 2 ) % faceSize ),
                                          mFacesNeighborhood.at( faceIndex ).at( ( splitVertexPos + 3 ) % faceSize )} );

  for ( int i = 0; i < faceSize; ++i )
  {
    if ( neighborIndex[i] >= 0 )
      changes.mNeighborhoodChanges.append( {neighborIndex[i], posInNeighbor[i], faceIndex, startIndex + int( i / 2 )} );

    int vertexIndex = face.at( ( splitVertexPos + i ) % faceSize );
    if ( mVertexToFace.at( vertexIndex ) == faceIndex )
      changes.mVerticesToFaceChanges.append( {vertexIndex, faceIndex, startIndex + int( i / 2 )} );
  }

  applyChanges( changes );

  return changes;
}


QgsTopologicalMesh::Changes QgsTopologicalMesh::addVertexInface( int includingFaceIndex, const QgsMeshVertex &vertex )
{
  Changes changes;
  changes.mVerticesToAdd.append( vertex );
  changes.mVertexToFaceToAdd.append( -1 );

  mMesh->vertices.append( vertex );
  mVertexToFace.append( -1 );
  changes.mAddedFacesFirstIndex = mMesh->faceCount();

  const QgsMeshFace includingFace = mMesh->face( includingFaceIndex );
  const FaceNeighbors includingFaceNeighborhood = mFacesNeighborhood.at( includingFaceIndex );
  int includingFaceSize = includingFace.count();

  for ( int i = 0; i < includingFaceSize; ++i )
  {
    // add a new face
    QgsMeshFace face( 3 );
    face[0] = mMesh->vertexCount() - 1;
    face[1] = includingFace.at( i );
    face[2] = includingFace.at( ( i + 1 ) % includingFaceSize );
    mMesh->faces.append( face );
    changes.mFacesToAdd.append( face );

    int currentVertexIndex = includingFace.at( i );
    if ( mVertexToFace.at( currentVertexIndex ) == includingFaceIndex )
    {
      int newFaceIndex = mMesh->faceCount() - 1;
      mVertexToFace[currentVertexIndex] = newFaceIndex;
      changes.mVerticesToFaceChanges.append( {currentVertexIndex, includingFaceIndex, newFaceIndex} );
    }

    int includingFaceNeighbor = includingFaceNeighborhood.at( i );
    FaceNeighbors neighbors(
    {
      changes.mAddedFacesFirstIndex + ( i + includingFaceSize - 1 ) % includingFaceSize,
      includingFaceNeighbor,
      changes.mAddedFacesFirstIndex + ( i + includingFaceSize + 1 ) % includingFaceSize
    } );
    mFacesNeighborhood.append( neighbors );
    changes.mFacesNeighborhoodToAdd.append( neighbors );

    if ( includingFaceNeighbor != -1 )
    {
      int indexInNeighbor = vertexPositionInFace( *mMesh, includingFace.at( ( i + 1 ) % includingFaceSize ), includingFaceNeighbor );
      int oldValue = mFacesNeighborhood[includingFaceNeighbor][indexInNeighbor];
      mFacesNeighborhood[includingFaceNeighbor][indexInNeighbor] = changes.mAddedFacesFirstIndex + i;
      changes.mNeighborhoodChanges.append( {includingFaceNeighbor, indexInNeighbor, oldValue, changes.mAddedFacesFirstIndex + i} );
    }
  }

  changes.mFacesToRemove.append( includingFace );
  changes.mFaceIndexesToRemove.append( includingFaceIndex );
  changes.mFacesNeighborhoodToRemove.append( includingFaceNeighborhood );

  mFacesNeighborhood[includingFaceIndex] = FaceNeighbors();
  mMesh->faces[includingFaceIndex] = QgsMeshFace();
  mVertexToFace[mVertexToFace.count() - 1] = mMesh->faceCount() - 1;
  changes.mVertexToFaceToAdd[changes.mVertexToFaceToAdd.count() - 1] = mMesh->faceCount() - 1 ;

  return changes;
}


QgsTopologicalMesh::Changes QgsTopologicalMesh::changeZValue( const QList<int> &verticesIndexes, const QList<double> &newValues )
{
  Q_ASSERT( verticesIndexes.count() == newValues.count() );
  Changes changes;
  changes.mChangeCoordinateVerticesIndexes.reserve( verticesIndexes.count() );
  changes.mNewZValues.reserve( verticesIndexes.count() );
  changes.mOldZValues.reserve( verticesIndexes.count() );
  for ( int i = 0; i < verticesIndexes.count(); ++i )
  {
    changes.mChangeCoordinateVerticesIndexes.append( verticesIndexes.at( i ) );
    changes.mOldZValues.append( mMesh->vertices.at( verticesIndexes.at( i ) ).z() );
    changes.mNewZValues.append( newValues.at( i ) );
  }

  applyChanges( changes );

  return changes;
}

QgsTopologicalMesh::Changes QgsTopologicalMesh::changeXYValue( const QList<int> &verticesIndexes, const QList<QgsPointXY> &newValues )
{
  Q_ASSERT( verticesIndexes.count() == newValues.count() );
  Changes changes;
  changes.mChangeCoordinateVerticesIndexes.reserve( verticesIndexes.count() );
  changes.mNewXYValues.reserve( verticesIndexes.count() );
  changes.mOldXYValues.reserve( verticesIndexes.count() );
  QSet<int> concernedFace;
  for ( int i = 0; i < verticesIndexes.count(); ++i )
  {
    changes.mChangeCoordinateVerticesIndexes.append( verticesIndexes.at( i ) );
    changes.mOldXYValues.append( mMesh->vertices.at( verticesIndexes.at( i ) ) );
    changes.mNewXYValues.append( newValues.at( i ) );
    concernedFace.unite( facesAroundVertex( verticesIndexes.at( i ) ).toSet() );
  }

  changes.mNativeFacesIndexesGeometryChanged = concernedFace.values();

  applyChanges( changes );

  return changes;
}
