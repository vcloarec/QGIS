/***************************************************************************
    qgsmeshdatasetgrouptreeview.cpp
    -------------------------------
    begin                : June 2018
    copyright            : (C) 2018 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmeshdatasetgrouptreeview.h"

#include "qgis.h"
#include "qgsmeshlayer.h"

#include <QList>
#include <QItemSelectionModel>
#include <QMouseEvent>

QgsMeshDatasetGroupTreeItem::QgsMeshDatasetGroupTreeItem( QgsMeshDatasetGroupTreeItem *parent )
  : mParent( parent )
{
}

QgsMeshDatasetGroupTreeItem::QgsMeshDatasetGroupTreeItem( const QString &name,
    bool isVector,
    int index, bool isUsed,
    QgsMeshDatasetGroupTreeItem *parent )
  : mParent( parent )
  , mName( name )
  , mIsVector( isVector )
  , mDatasetGroupIndex( index )
  , mUsed( isUsed )
{
}

QgsMeshDatasetGroupTreeItem::~QgsMeshDatasetGroupTreeItem()
{
  qDeleteAll( mChildren );
}

void QgsMeshDatasetGroupTreeItem::appendChild( QgsMeshDatasetGroupTreeItem *node )
{
  mChildren.append( node );
}

QgsMeshDatasetGroupTreeItem *QgsMeshDatasetGroupTreeItem::child( int row ) const
{
  if ( row < mChildren.count() )
    return mChildren.at( row );
  else
    return nullptr;
}

int QgsMeshDatasetGroupTreeItem::childCount() const
{
  return mChildren.count();
}

QgsMeshDatasetGroupTreeItem *QgsMeshDatasetGroupTreeItem::parentItem() const
{
  return mParent;
}

int QgsMeshDatasetGroupTreeItem::row() const
{
  if ( mParent )
    return mParent->mChildren.indexOf( const_cast<QgsMeshDatasetGroupTreeItem *>( this ) );

  return 0;
}

QString QgsMeshDatasetGroupTreeItem::name() const
{
  return mName;
}

bool QgsMeshDatasetGroupTreeItem::isVector() const
{
  return mIsVector;
}

int QgsMeshDatasetGroupTreeItem::datasetGroupIndex() const
{
  return mDatasetGroupIndex;
}

bool QgsMeshDatasetGroupTreeItem::used() const
{
  return mUsed;
}

void QgsMeshDatasetGroupTreeItem::setUsed( bool used )
{
  mUsed = used;
}

void QgsMeshDatasetGroupTreeItem::setName( const QString &name )
{
  mName = name;
}


/////////////////////////////////////////////////////////////////////////////////////////

QgsMeshDatasetGroupProvidedTreeModel::QgsMeshDatasetGroupProvidedTreeModel( QObject *parent )
  : QAbstractItemModel( parent )
  ,  mRootItem( new QgsMeshDatasetGroupTreeItem() )
{
}

int QgsMeshDatasetGroupProvidedTreeModel::columnCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return 1;
}

int QgsMeshDatasetGroupProvidedTreeModel::activeScalarGroup() const
{
  return mActiveScalarGroupIndex;
}

void QgsMeshDatasetGroupProvidedTreeModel::setActiveScalarGroup( int group )
{
  if ( mActiveScalarGroupIndex == group )
    return;

  int oldGroupIndex = mActiveScalarGroupIndex;
  mActiveScalarGroupIndex = group;

  if ( oldGroupIndex > -1 )
  {
    const auto index = groupIndexToModelIndex( oldGroupIndex );
    emit dataChanged( index, index );
  }

  if ( group > -1 )
  {
    const auto index = groupIndexToModelIndex( group );
    emit dataChanged( index, index );
  }
}

int QgsMeshDatasetGroupProvidedTreeModel::activeVectorGroup() const
{
  return mActiveVectorGroupIndex;
}

void QgsMeshDatasetGroupProvidedTreeModel::setActiveVectorGroup( int group )
{
  if ( mActiveVectorGroupIndex == group )
    return;

  int oldGroupIndex = mActiveVectorGroupIndex;
  mActiveVectorGroupIndex = group;

  if ( oldGroupIndex > -1 )
  {
    const auto index = groupIndexToModelIndex( oldGroupIndex );
    emit dataChanged( index, index );
  }

  if ( group > -1 )
  {
    const auto index = groupIndexToModelIndex( group );
    emit dataChanged( index, index );
  }
}

QVariant QgsMeshDatasetGroupProvidedTreeModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() )
    return QVariant();

  QgsMeshDatasetGroupTreeItem *item = static_cast<QgsMeshDatasetGroupTreeItem *>( index.internalPointer() );

  switch ( role )
  {
    case Qt::DisplayRole:
    case Name:
      return item->name();
    case IsVector:
      return item->isVector();
    case IsActiveScalarDatasetGroup:
      return item->datasetGroupIndex() == mActiveScalarGroupIndex;
    case IsActiveVectorDatasetGroup:
      return item->datasetGroupIndex() == mActiveVectorGroupIndex;
    case DatasetGroupIndex:
      return item->datasetGroupIndex();
    case Qt::CheckStateRole :
      return static_cast< int >( item->used() ? Qt::Checked : Qt::Unchecked );
  }

  return QVariant();
}

bool QgsMeshDatasetGroupProvidedTreeModel::setData( const QModelIndex &index, const QVariant &value, int role )
{
  if ( !index.isValid() )
    return false;
  QgsMeshDatasetGroupTreeItem *item = static_cast<QgsMeshDatasetGroupTreeItem *>( index.internalPointer() );

  switch ( role )
  {
    case Qt::EditRole:
    case Name:
      item->setName( value.toString() );
      return true;
    case Qt::CheckStateRole :
      item->setUsed( value.toBool() );
      return true;
  }
  return false;
}

QgsMeshDatasetGroupUsedFilterModel::QgsMeshDatasetGroupUsedFilterModel( QAbstractItemModel *sourceModel ):
  QSortFilterProxyModel( sourceModel )
{
  setSourceModel( sourceModel );
}

Qt::ItemFlags QgsMeshDatasetGroupUsedFilterModel::flags( const QModelIndex &index ) const
{
  if ( !index.isValid() )
    return Qt::NoItemFlags;

  return Qt::ItemIsEnabled;
}

QVariant QgsMeshDatasetGroupUsedFilterModel::data( const QModelIndex &index, int role ) const
{
  if ( role == Qt::CheckStateRole )
    return QVariant();

  return QSortFilterProxyModel::data( index, role );
}

bool QgsMeshDatasetGroupUsedFilterModel::filterAcceptsRow( int source_row, const QModelIndex &source_parent ) const
{
  QgsMeshDatasetGroupTreeItem *parentItem;

  if ( !source_parent.isValid() )
    parentItem = static_cast<QgsMeshDatasetGroupProvidedTreeModel *>( sourceModel() )->rootItem();
  else
    parentItem = static_cast<QgsMeshDatasetGroupTreeItem *>( source_parent.internalPointer() );

  if ( parentItem->childCount() < source_row )
    return false;

  QgsMeshDatasetGroupTreeItem *item = parentItem->child( source_row );

  if ( item )
    return item->used();

  return false;
}

Qt::ItemFlags QgsMeshDatasetGroupProvidedTreeModel::flags( const QModelIndex &index ) const
{
  if ( !index.isValid() )
    return Qt::NoItemFlags;

  return Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
}

QVariant QgsMeshDatasetGroupProvidedTreeModel::headerData( int section,
    Qt::Orientation orientation,
    int role ) const
{
  Q_UNUSED( section )

  if ( orientation == Qt::Horizontal && role == Qt::DisplayRole )
    return tr( "Groups" );

  return QVariant();
}

QModelIndex QgsMeshDatasetGroupProvidedTreeModel::index( int row, int column, const QModelIndex &parent )
const
{
  if ( !hasIndex( row, column, parent ) )
    return QModelIndex();

  QgsMeshDatasetGroupTreeItem *parentItem;

  if ( !parent.isValid() )
    parentItem = mRootItem.get();
  else
    parentItem = static_cast<QgsMeshDatasetGroupTreeItem *>( parent.internalPointer() );

  QgsMeshDatasetGroupTreeItem *childItem = parentItem->child( row );
  if ( childItem )
    return createIndex( row, column, childItem );
  else
    return QModelIndex();
}

QModelIndex QgsMeshDatasetGroupProvidedTreeModel::parent( const QModelIndex &index ) const
{
  if ( !index.isValid() )
    return QModelIndex();

  QgsMeshDatasetGroupTreeItem *childItem = static_cast<QgsMeshDatasetGroupTreeItem *>( index.internalPointer() );
  QgsMeshDatasetGroupTreeItem *parentItem = childItem->parentItem();

  if ( parentItem == mRootItem.get() )
    return QModelIndex();

  return createIndex( parentItem->row(), 0, parentItem );
}

int QgsMeshDatasetGroupProvidedTreeModel::rowCount( const QModelIndex &parent ) const
{
  QgsMeshDatasetGroupTreeItem *parentItem;
  if ( parent.column() > 0 )
    return 0;

  if ( !parent.isValid() )
    parentItem = mRootItem.get();
  else
    parentItem = static_cast<QgsMeshDatasetGroupTreeItem *>( parent.internalPointer() );

  return parentItem->childCount();
}

void QgsMeshDatasetGroupProvidedTreeModel::syncToLayer( QgsMeshLayer *layer )
{
  beginResetModel();

  mRootItem.reset( new QgsMeshDatasetGroupTreeItem() );
  mNameToItem.clear();
  mDatasetGroupIndexToItem.clear();

  if ( layer && layer->dataProvider() )
  {
    const QgsMeshDataProvider *dp = layer->dataProvider();
    for ( int groupIndex = 0; groupIndex < dp->datasetGroupCount(); ++groupIndex )
    {
      const QgsMeshDatasetGroupState &state = layer->datasetGroupsState().value( groupIndex, QgsMeshDatasetGroupState() );
      const QgsMeshDatasetGroupMetadata meta = dp->datasetGroupMetadata( groupIndex );
      const QString metaName = meta.name();

      QString name = state.renaming;
      if ( name.isEmpty() )
        name = state.originalName;

      const bool isVector = meta.isVector();
      const QStringList subdatasets = metaName.split( '/' );

      if ( subdatasets.size() == 1 )
      {
        if ( name.isEmpty() )
          name = metaName;
        addTreeItem( metaName, name, isVector, groupIndex, state.used, mRootItem.get() );
      }
      else if ( subdatasets.size() == 2 )
      {
        auto i = mNameToItem.find( subdatasets[0] );
        if ( i == mNameToItem.end() )
        {
          QgsDebugMsg( QStringLiteral( "Unable to find parent group for %1." ).arg( metaName ) );
          addTreeItem( metaName, name, isVector, groupIndex, state.used, mRootItem.get() );
        }
        else
        {
          if ( name.isEmpty() )
            name = subdatasets[1];
          addTreeItem( metaName, name, isVector, groupIndex, state.used, i.value() );
        }
      }
      else
      {
        QgsDebugMsg( QStringLiteral( "Ignoring too deep child group name %1." ).arg( name ) );
        addTreeItem( metaName, name, isVector, groupIndex, state.used, mRootItem.get() );
      }
    }
  }
  endResetModel();
  updateActiveGroup();
}

QMap<int, QgsMeshDatasetGroupState> QgsMeshDatasetGroupProvidedTreeModel::groupStates() const
{
  QMap<int, QgsMeshDatasetGroupState> ret;

  for ( const QgsMeshDatasetGroupTreeItem *item : mDatasetGroupIndexToItem )
  {
    QgsMeshDatasetGroupState state;
    state.used = item->used();
    state.renaming = item->name();

    ret[item->datasetGroupIndex()] = state;
  }
  return ret;
}

void QgsMeshDatasetGroupProvidedTreeModel::updateActiveGroup()
{
  while ( mActiveScalarGroupIndex != -1 &&
          !groupStates()[mActiveScalarGroupIndex].used )
    mActiveScalarGroupIndex--;

  while ( mActiveVectorGroupIndex != -1 &&
          ( !groupStates()[mActiveVectorGroupIndex].used ||
            !mDatasetGroupIndexToItem[mActiveVectorGroupIndex]->isVector() ) )
    mActiveVectorGroupIndex--;
}

void QgsMeshDatasetGroupProvidedTreeModel::addTreeItem( const QString &groupName,
    const QString &displayName,
    bool isVector,
    int groupIndex,
    bool isUsed,
    QgsMeshDatasetGroupTreeItem *parent )
{
  Q_ASSERT( parent );
  QgsMeshDatasetGroupTreeItem *item = new QgsMeshDatasetGroupTreeItem( displayName, isVector, groupIndex, isUsed, parent );
  parent->appendChild( item );

  if ( mNameToItem.contains( groupName ) )
  {
    QgsDebugMsg( QStringLiteral( "Group %1 is not unique" ).arg( groupName ) );
  }
  mNameToItem[groupName] = item;

  if ( mDatasetGroupIndexToItem.contains( groupIndex ) )
  {
    QgsDebugMsg( QStringLiteral( "Group index %1 is not unique" ).arg( groupIndex ) );
  }
  mDatasetGroupIndexToItem[groupIndex] = item;
}

QModelIndex QgsMeshDatasetGroupProvidedTreeModel::groupIndexToModelIndex( int groupIndex ) const
{
  if ( groupIndex < 0 || !mDatasetGroupIndexToItem.contains( groupIndex ) )
    return QModelIndex();

  const auto item = mDatasetGroupIndexToItem[groupIndex];
  auto parentItem = item->parentItem();
  if ( parentItem )
  {
    const auto parentIndex = index( parentItem->row(), 0, QModelIndex() );
    return index( item->row(), 0, parentIndex );
  }
  else
    return QModelIndex();

}

/////////////////////////////////////////////////////////////////////////////////////////


QgsMeshDatasetGroupTreeItemDelagate::QgsMeshDatasetGroupTreeItemDelagate( QObject *parent )
  : QStyledItemDelegate( parent )
  , mScalarSelectedPixmap( QStringLiteral( ":/images/themes/default/propertyicons/meshcontours.svg" ) )
  , mScalarDeselectedPixmap( QStringLiteral( ":/images/themes/default/propertyicons/meshcontoursoff.svg" ) )
  , mVectorSelectedPixmap( QStringLiteral( ":/images/themes/default/propertyicons/meshvectors.svg" ) )
  , mVectorDeselectedPixmap( QStringLiteral( ":/images/themes/default/propertyicons/meshvectorsoff.svg" ) )
{
}

void QgsMeshDatasetGroupTreeItemDelagate::paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  if ( !painter )
    return;

  QStyledItemDelegate::paint( painter, option, index );
  bool isVector = index.data( QgsMeshDatasetGroupProvidedTreeModel::IsVector ).toBool();
  if ( isVector )
  {
    bool isActive = index.data( QgsMeshDatasetGroupProvidedTreeModel::IsActiveVectorDatasetGroup ).toBool();
    painter->drawPixmap( iconRect( option.rect, true ), isActive ? mVectorSelectedPixmap : mVectorDeselectedPixmap );
  }
  bool isActive = index.data( QgsMeshDatasetGroupProvidedTreeModel::IsActiveScalarDatasetGroup ).toBool();
  painter->drawPixmap( iconRect( option.rect, false ), isActive ? mScalarSelectedPixmap : mScalarDeselectedPixmap );
}

QRect QgsMeshDatasetGroupTreeItemDelagate::iconRect( const QRect rect, bool isVector ) const
{
  int iw = mScalarSelectedPixmap.width();
  int ih = mScalarSelectedPixmap.height();
  int margin = ( rect.height() - ih ) / 2;
  int i = isVector ? 1 : 2;
  return QRect( rect.right() - i * ( iw + margin ), rect.top() + margin, iw, ih );
}

QSize QgsMeshDatasetGroupTreeItemDelagate::sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  QSize hint = QStyledItemDelegate::sizeHint( option, index );
  if ( hint.height() < 16 )
    hint.setHeight( 16 );
  return hint;
}

/////////////////////////////////////////////////////////////////////////////////////////

QgsMeshDatasetGroupUsedTreeView::QgsMeshDatasetGroupUsedTreeView( QWidget *parent )
  : QTreeView( parent ),
    mModel( new QgsMeshDatasetGroupProvidedTreeModel( this ) ),
    mUsedModel( new QgsMeshDatasetGroupUsedFilterModel( mModel ) )
{
  setModel( mUsedModel );
  setItemDelegate( &mDelegate );
  setSelectionMode( QAbstractItemView::SingleSelection );
}

void QgsMeshDatasetGroupUsedTreeView::setLayer( QgsMeshLayer *layer )
{
  if ( layer != mMeshLayer )
  {
    mMeshLayer = layer;
    syncToLayer();
  }
}

int QgsMeshDatasetGroupUsedTreeView::activeScalarGroup() const
{
  return mModel->activeScalarGroup();
}

void QgsMeshDatasetGroupUsedTreeView::setActiveScalarGroup( int group )
{
  if ( mModel->activeScalarGroup() != group )
  {
    mModel->setActiveScalarGroup( group );
    emit activeScalarGroupChanged( group );
  }
}

int QgsMeshDatasetGroupUsedTreeView::activeVectorGroup() const
{
  return mModel->activeVectorGroup();
}

void QgsMeshDatasetGroupUsedTreeView::setActiveVectorGroup( int group )
{
  if ( mModel->activeVectorGroup() != group )
  {
    mModel->setActiveVectorGroup( group );
    emit activeVectorGroupChanged( group );
  }
}

void QgsMeshDatasetGroupUsedTreeView::syncToLayer()
{
  mModel->syncToLayer( mMeshLayer );
  setActiveGroupFromActiveDataset();
}

void QgsMeshDatasetGroupUsedTreeView::updateGroupFromLayer()
{
  mModel->syncToLayer( mMeshLayer );
}

void QgsMeshDatasetGroupUsedTreeView::mousePressEvent( QMouseEvent *event )
{
  if ( !event )
    return;

  bool processed = false;
  const QModelIndex idx = indexAt( event->pos() );
  if ( idx.isValid() )
  {
    const QRect vr = visualRect( idx );
    if ( mDelegate.iconRect( vr, true ).contains( event->pos() ) )
    {
      bool isVector = idx.data( QgsMeshDatasetGroupProvidedTreeModel::IsVector ).toBool();
      if ( isVector )
      {
        setActiveVectorGroup( idx.data( QgsMeshDatasetGroupProvidedTreeModel::DatasetGroupIndex ).toInt() );
        processed = true;
      }
    }
    else if ( mDelegate.iconRect( vr, false ).contains( event->pos() ) )
    {
      setActiveScalarGroup( idx.data( QgsMeshDatasetGroupProvidedTreeModel::DatasetGroupIndex ).toInt() );
      processed = true;
    }
  }

  // only if the user did not click one of the icons do usual handling
  if ( !processed )
    QTreeView::mousePressEvent( event );
}

void QgsMeshDatasetGroupUsedTreeView::setActiveGroupFromActiveDataset()
{
  int scalarGroup = -1;
  int vectorGroup = -1;

  // find active dataset
  if ( mMeshLayer )
  {
    const QgsMeshRendererSettings rendererSettings = mMeshLayer->rendererSettings();
    scalarGroup = rendererSettings.activeScalarDatasetGroup();
    vectorGroup = rendererSettings.activeVectorDatasetGroup();
  }

  setActiveScalarGroup( scalarGroup );
  setActiveVectorGroup( vectorGroup );
}

void QgsMeshDatasetGroupProvidedListModel::syncToLayer( QgsMeshLayer *layer )
{
  mLayer = layer;
}

int QgsMeshDatasetGroupProvidedListModel::rowCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent );
  if ( mLayer )
    return mLayer->dataProvider()->datasetGroupCount();
  else
    return 0;
}

QVariant QgsMeshDatasetGroupProvidedListModel::data( const QModelIndex &index, int role ) const
{
  if ( !mLayer || ! index.isValid() )
    return QVariant();

  QgsMeshDataProvider *dataProvider = mLayer->dataProvider();

  if ( !dataProvider || index.row() >= dataProvider->datasetGroupCount() )
    return QVariant();

  QgsMeshDatasetGroupMetadata meta = dataProvider->datasetGroupMetadata( index.row() );

  if ( role == Qt::DisplayRole )
  {
    return meta.name();
  }

  return QVariant();
}

QgsMeshDatasetGroupProvidedTreeView::QgsMeshDatasetGroupProvidedTreeView( QWidget *parent ):
  QTreeView( parent )
  , mModel( new QgsMeshDatasetGroupProvidedTreeModel( this ) )
{
  setModel( mModel );
}
