/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "uistandarditemmodel.h"

#ifndef QT_NO_STANDARDITEMMODEL

#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qpair.h>
#include <QtCore/qvariant.h>
#include <QtCore/qvector.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qbitarray.h>
#include <QtCore/qmimedata.h>

#include <private/uistandarditemmodel_p.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE_UIHELPERS

class UiStandardItemModelLessThan
{
public:
    inline UiStandardItemModelLessThan()
        { }

    inline bool operator()(const QPair<UiStandardItem*, int> &l,
                           const QPair<UiStandardItem*, int> &r) const
    {
        return *(l.first) < *(r.first);
    }
};

class UiStandardItemModelGreaterThan
{
public:
    inline UiStandardItemModelGreaterThan()
        { }

    inline bool operator()(const QPair<UiStandardItem*, int> &l,
                           const QPair<UiStandardItem*, int> &r) const
    {
        return *(r.first) < *(l.first);
    }
};

/*!
  \internal
*/
UiStandardItemPrivate::~UiStandardItemPrivate()
{
    QVector<UiStandardItem*>::const_iterator it;
    for (it = children.constBegin(); it != children.constEnd(); ++it) {
        UiStandardItem *child = *it;
        if (child)
            child->d_func()->setModel(0);
        delete child;
    }
    children.clear();
    if (parent && model)
        parent->d_func()->childDeleted(q_func());
}

/*!
  \internal
*/
QPair<int, int> UiStandardItemPrivate::position() const
{
    if (UiStandardItem *par = parent) {
        int idx = par->d_func()->childIndex(q_func());
        if (idx == -1)
            return QPair<int, int>(-1, -1);
        return QPair<int, int>(idx / par->columnCount(), idx % par->columnCount());
    }
    // ### support header items?
    return QPair<int, int>(-1, -1);
}

/*!
  \internal
*/
void UiStandardItemPrivate::setChild(int row, int column, UiStandardItem *item,
                                    bool emitChanged)
{
    Q_Q(UiStandardItem);
    if (item == q) {
        qWarning("UiStandardItem::setChild: Can't make an item a child of itself %p",
                 item);
        return;
    }
    if ((row < 0) || (column < 0))
        return;
    if (rows <= row)
        q->setRowCount(row + 1);
    if (columns <= column)
        q->setColumnCount(column + 1);
    int index = childIndex(row, column);
    Q_ASSERT(index != -1);
    UiStandardItem *oldItem = children.at(index);
    if (item == oldItem)
        return;
    if (item) {
        if (item->d_func()->parent == 0) {
            item->d_func()->setParentAndModel(q, model);
        } else {
            qWarning("UiStandardItem::setChild: Ignoring duplicate insertion of item %p",
                     item);
            return;
        }
    }
    if (oldItem)
        oldItem->d_func()->setModel(0);
    delete oldItem;
    children.replace(index, item);
    if (emitChanged && model)
        model->d_func()->itemChanged(item);
}


/*!
  \internal
*/
void UiStandardItemPrivate::changeFlags(bool enable, Qt::ItemFlags f)
{
    Q_Q(UiStandardItem);
    Qt::ItemFlags flags = q->flags();
    if (enable)
        flags |= f;
    else
        flags &= ~f;
    q->setFlags(flags);
}

/*!
  \internal
*/
void UiStandardItemPrivate::childDeleted(UiStandardItem *child)
{
    int index = childIndex(child);
    Q_ASSERT(index != -1);
    children.replace(index, 0);
}

/*!
  \internal
*/
void UiStandardItemPrivate::setItemData(const QMap<int, QVariant> &roles)
{
    Q_Q(UiStandardItem);

    //let's build the vector of new values
    QVector<UiStandardItemData> newValues;
    QMap<int, QVariant>::const_iterator it;
    for (it = roles.begin(); it != roles.end(); ++it) {
        QVariant value = it.value();
        if (value.isValid()) {
            int role = it.key();
            role = (role == Qt::EditRole) ? Qt::DisplayRole : role;
            UiStandardItemData wid(role,it.value());
            newValues.append(wid);
        }
    }

    if (values!=newValues) {
        values=newValues;
        if (model)
            model->d_func()->itemChanged(q);
    }
}

/*!
  \internal
*/
const QMap<int, QVariant> UiStandardItemPrivate::itemData() const
{
    QMap<int, QVariant> result;
    QVector<UiStandardItemData>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it)
        result.insert((*it).role, (*it).value);
    return result;
}

/*!
  \internal
*/
void UiStandardItemPrivate::sortChildren(int column, Qt::SortOrder order)
{
    Q_Q(UiStandardItem);
    if (column >= columnCount())
        return;

    QVector<QPair<UiStandardItem*, int> > sortable;
    QVector<int> unsortable;

    sortable.reserve(rowCount());
    unsortable.reserve(rowCount());

    for (int row = 0; row < rowCount(); ++row) {
        UiStandardItem *itm = q->child(row, column);
        if (itm)
            sortable.append(QPair<UiStandardItem*,int>(itm, row));
        else
            unsortable.append(row);
    }

    if (order == Qt::AscendingOrder) {
        UiStandardItemModelLessThan lt;
        qStableSort(sortable.begin(), sortable.end(), lt);
    } else {
        UiStandardItemModelGreaterThan gt;
        qStableSort(sortable.begin(), sortable.end(), gt);
    }

    QModelIndexList changedPersistentIndexesFrom, changedPersistentIndexesTo;
    QVector<UiStandardItem*> sorted_children(children.count());
    for (int i = 0; i < rowCount(); ++i) {
        int r = (i < sortable.count()
                 ? sortable.at(i).second
                 : unsortable.at(i - sortable.count()));
        for (int c = 0; c < columnCount(); ++c) {
            UiStandardItem *itm = q->child(r, c);
            sorted_children[childIndex(i, c)] = itm;
            if (model) {
                QModelIndex from = model->createIndex(r, c, q);
                if (model->d_func()->persistent.indexes.contains(from)) {
                    QModelIndex to = model->createIndex(i, c, q);
                    changedPersistentIndexesFrom.append(from);
                    changedPersistentIndexesTo.append(to);
                }
            }
        }
    }

    children = sorted_children;

    if (model) {
        model->changePersistentIndexList(changedPersistentIndexesFrom, changedPersistentIndexesTo);
    }

    QVector<UiStandardItem*>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        if (*it)
            (*it)->d_func()->sortChildren(column, order);
    }
}

/*!
  \internal
  set the model of this item and all its children
  */
void UiStandardItemPrivate::setModel(UiStandardItemModel *mod)
{
    if (children.isEmpty()) {
        if (model)
            model->d_func()->invalidatePersistentIndex(model->indexFromItem(q_ptr));
        model = mod;
    } else {
        QStack<UiStandardItem*> stack;
        stack.push(q_ptr);
        while (!stack.isEmpty()) {
            UiStandardItem *itm = stack.pop();
            if (itm->d_func()->model) {
                itm->d_func()->model->d_func()->invalidatePersistentIndex(itm->d_func()->model->indexFromItem(itm));
            }
            itm->d_func()->model = mod;
            const QVector<UiStandardItem*> &childList = itm->d_func()->children;
            for (int i = 0; i < childList.count(); ++i) {
                UiStandardItem *chi = childList.at(i);
                if (chi)
                    stack.push(chi);
            }
        }
    }
}

/*!
  \internal
*/
UiStandardItemModelPrivate::UiStandardItemModelPrivate()
    : root(new UiStandardItem),
      itemPrototype(0),
      sortRole(Qt::DisplayRole)
{
}

/*!
  \internal
*/
UiStandardItemModelPrivate::~UiStandardItemModelPrivate()
{
    delete itemPrototype;
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::init()
{
    Q_Q(UiStandardItemModel);
    QObject::connect(q, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                     q, SLOT(_q_emitItemChanged(QModelIndex,QModelIndex)));
}

/*!
    \internal
*/
void UiStandardItemModelPrivate::_q_emitItemChanged(const QModelIndex &topLeft,
                                                   const QModelIndex &bottomRight)
{
    Q_Q(UiStandardItemModel);
    QModelIndex parent = topLeft.parent();
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column) {
            QModelIndex index = q->index(row, column, parent);
            if (UiStandardItem *item = itemFromIndex(index))
                emit q->itemChanged(item);
        }
    }
}

/*!
    \internal
*/
bool UiStandardItemPrivate::insertRows(int row, const QList<UiStandardItem*> &items)
{
    Q_Q(UiStandardItem);
    if ((row < 0) || (row > rowCount()))
        return false;
    int count = items.count();
    if (model)
        model->d_func()->rowsAboutToBeInserted(q, row, row + count - 1);
    if (rowCount() == 0) {
        if (columnCount() == 0)
            q->setColumnCount(1);
        children.resize(columnCount() * count);
        rows = count;
    } else {
        rows += count;
        int index = childIndex(row, 0);
        if (index != -1)
            children.insert(index, columnCount() * count, 0);
    }
    for (int i = 0; i < items.count(); ++i) {
        UiStandardItem *item = items.at(i);
        item->d_func()->model = model;
        item->d_func()->parent = q;
        int index = childIndex(i + row, 0);
        children.replace(index, item);
    }
    if (model)
        model->d_func()->rowsInserted(q, row, count);
    return true;
}

bool UiStandardItemPrivate::insertRows(int row, int count, const QList<UiStandardItem*> &items)
{
    Q_Q(UiStandardItem);
    if ((count < 1) || (row < 0) || (row > rowCount()))
        return false;
    if (model)
        model->d_func()->rowsAboutToBeInserted(q, row, row + count - 1);
    if (rowCount() == 0) {
        children.resize(columnCount() * count);
        rows = count;
    } else {
        rows += count;
        int index = childIndex(row, 0);
        if (index != -1)
            children.insert(index, columnCount() * count, 0);
    }
    if (!items.isEmpty()) {
        int index = childIndex(row, 0);
        int limit = qMin(items.count(), columnCount() * count);
        for (int i = 0; i < limit; ++i) {
            UiStandardItem *item = items.at(i);
            if (item) {
                if (item->d_func()->parent == 0) {
                    item->d_func()->setParentAndModel(q, model);
                } else {
                    qWarning("UiStandardItem::insertRows: Ignoring duplicate insertion of item %p",
                             item);
                    item = 0;
                }
            }
            children.replace(index, item);
            ++index;
        }
    }
    if (model)
        model->d_func()->rowsInserted(q, row, count);
    return true;
}

/*!
    \internal
*/
bool UiStandardItemPrivate::insertColumns(int column, int count, const QList<UiStandardItem*> &items)
{
    Q_Q(UiStandardItem);
    if ((count < 1) || (column < 0) || (column > columnCount()))
        return false;
    if (model)
        model->d_func()->columnsAboutToBeInserted(q, column, column + count - 1);
    if (columnCount() == 0) {
        children.resize(rowCount() * count);
        columns = count;
    } else {
        columns += count;
        int index = childIndex(0, column);
        for (int row = 0; row < rowCount(); ++row) {
            children.insert(index, count, 0);
            index += columnCount();
        }
    }
    if (!items.isEmpty()) {
        int limit = qMin(items.count(), rowCount() * count);
        for (int i = 0; i < limit; ++i) {
            UiStandardItem *item = items.at(i);
            if (item) {
                if (item->d_func()->parent == 0) {
                    item->d_func()->setParentAndModel(q, model);
                } else {
                    qWarning("UiStandardItem::insertColumns: Ignoring duplicate insertion of item %p",
                             item);
                    item = 0;
                }
            }
            int r = i / count;
            int c = column + (i % count);
            int index = childIndex(r, c);
            children.replace(index, item);
        }
    }
    if (model)
        model->d_func()->columnsInserted(q, column, count);
    return true;
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::itemChanged(UiStandardItem *item)
{
    Q_Q(UiStandardItemModel);
    QModelIndex index = q->indexFromItem(item);
    emit q->dataChanged(index, index);
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::rowsAboutToBeInserted(UiStandardItem *parent,
                                                      int start, int end)
{
    Q_Q(UiStandardItemModel);
    QModelIndex index = q->indexFromItem(parent);
    q->beginInsertRows(index, start, end);
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::columnsAboutToBeInserted(UiStandardItem *parent,
                                                         int start, int end)
{
    Q_Q(UiStandardItemModel);
    QModelIndex index = q->indexFromItem(parent);
    q->beginInsertColumns(index, start, end);
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::rowsAboutToBeRemoved(UiStandardItem *parent,
                                                     int start, int end)
{
    Q_Q(UiStandardItemModel);
    QModelIndex index = q->indexFromItem(parent);
    q->beginRemoveRows(index, start, end);
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::columnsAboutToBeRemoved(UiStandardItem *parent,
                                                        int start, int end)
{
    Q_Q(UiStandardItemModel);
    QModelIndex index = q->indexFromItem(parent);
    q->beginRemoveColumns(index, start, end);
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::rowsInserted(UiStandardItem *parent,
                                             int row, int count)
{
    Q_UNUSED(parent)
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_Q(UiStandardItemModel);
    q->endInsertRows();
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::columnsInserted(UiStandardItem *parent,
                                                int column, int count)
{
    Q_UNUSED(parent)
    Q_UNUSED(column)
    Q_UNUSED(count)
    Q_Q(UiStandardItemModel);
    q->endInsertColumns();
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::rowsRemoved(UiStandardItem *parent,
                                            int row, int count)
{
    Q_UNUSED(parent)
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_Q(UiStandardItemModel);
    q->endRemoveRows();
}

/*!
  \internal
*/
void UiStandardItemModelPrivate::columnsRemoved(UiStandardItem *parent,
                                               int column, int count)
{
    Q_UNUSED(parent)
    Q_UNUSED(column)
    Q_UNUSED(count)
    Q_Q(UiStandardItemModel);
    q->endRemoveColumns();
}

/*!
    \class UiStandardItem
    \brief The UiStandardItem class provides an item for use with the
    UiStandardItemModel class.
    \since 4.2
    \ingroup model-view
    \inmodule QtWidgets

    Items usually contain text, icons, or checkboxes.

    Each item can have its own background brush which is set with the
    setBackground() function. The current background brush can be found with
    background().  The text label for each item can be rendered with its own
    font and brush. These are specified with the setFont() and setForeground()
    functions, and read with font() and foreground().

    By default, items are enabled, editable, selectable, checkable, and can be
    used both as the source of a drag and drop operation and as a drop target.
    Each item's flags can be changed by calling setFlags(). Checkable items
    can be checked and unchecked with the setCheckState() function. The
    corresponding checkState() function indicates whether the item is
    currently checked.

    You can store application-specific data in an item by calling setData().

    Each item can have a two-dimensional table of child items. This makes it
    possible to build hierarchies of items. The typical hierarchy is the tree,
    in which case the child table is a table with a single column (a list).

    The dimensions of the child table can be set with setRowCount() and
    setColumnCount(). Items can be positioned in the child table with
    setChild(). Get a pointer to a child item with child(). New rows and
    columns of children can also be inserted with insertRow() and
    insertColumn(), or appended with appendRow() and appendColumn(). When
    using the append and insert functions, the dimensions of the child table
    will grow as needed.

    An existing row of children can be removed with removeRow() or takeRow();
    correspondingly, a column can be removed with removeColumn() or
    takeColumn().

    An item's children can be sorted by calling sortChildren().

    \section1 Subclassing

    When subclassing UiStandardItem to provide custom items, it is possible to
    define new types for them so that they can be distinguished from the base
    class. The type() function should be reimplemented to return a new type
    value equal to or greater than \l UserType.

    Reimplement data() and setData() if you want to perform custom handling of
    data queries and/or control how an item's data is represented.

    Reimplement clone() if you want UiStandardItemModel to be able to create
    instances of your custom item class on demand (see
    UiStandardItemModel::setItemPrototype()).

    Reimplement read() and write() if you want to control how items are
    represented in their serialized form.

    Reimplement \l{operator<()} if you want to control the semantics of item
    comparison. \l{operator<()} determines the sorted order when sorting items
    with sortChildren() or with UiStandardItemModel::sort().

    \sa UiStandardItemModel, {Item View Convenience Classes}, {Model/View Programming}
*/

/*!
    \enum UiStandardItem::ItemType

    This enum describes the types that are used to describe standard items.

    \value Type     The default type for standard items.
    \value UserType The minimum value for custom types. Values below UserType are
                    reserved by Qt.

    You can define new user types in UiStandardItem subclasses to ensure that
    custom items are treated specially; for example, when items are sorted.

    \sa type()
*/

/*!
    Constructs an item.
*/
UiStandardItem::UiStandardItem()
    : d_ptr(new UiStandardItemPrivate)
{
    Q_D(UiStandardItem);
    d->q_ptr = this;
}

/*!
    Constructs an item with the given \a text.
*/
UiStandardItem::UiStandardItem(const QString &text)
    : d_ptr(new UiStandardItemPrivate)
{
    Q_D(UiStandardItem);
    d->q_ptr = this;
    setText(text);
}

/*!
   Constructs an item with \a rows rows and \a columns columns of child items.
*/
UiStandardItem::UiStandardItem(int rows, int columns)
    : d_ptr(new UiStandardItemPrivate)
{
    Q_D(UiStandardItem);
    d->q_ptr = this;
    setRowCount(rows);
    setColumnCount(columns);
}

/*!
  \internal
*/
UiStandardItem::UiStandardItem(UiStandardItemPrivate &dd)
    : d_ptr(&dd)
{
    Q_D(UiStandardItem);
    d->q_ptr = this;
}

/*!
  Constructs a copy of \a other. Note that model() is
  not copied.

  This function is useful when reimplementing clone().
*/
UiStandardItem::UiStandardItem(const UiStandardItem &other)
    : d_ptr(new UiStandardItemPrivate)
{
    Q_D(UiStandardItem);
    d->q_ptr = this;
    operator=(other);
}

/*!
  Assigns \a other's data and flags to this item. Note that
  type() and model() are not copied.

  This function is useful when reimplementing clone().
*/
UiStandardItem &UiStandardItem::operator=(const UiStandardItem &other)
{
    Q_D(UiStandardItem);
    d->values = other.d_func()->values;
    return *this;
}

/*!
  Destructs the item.
  This causes the item's children to be destructed as well.
*/
UiStandardItem::~UiStandardItem()
{
}

/*!
  Returns the item's parent item, or 0 if the item has no parent.

  \sa child()
*/
UiStandardItem *UiStandardItem::parent() const
{
    Q_D(const UiStandardItem);
    if (!d->model || (d->model->d_func()->root.data() != d->parent))
        return d->parent;
    return 0;
}

/*!
    Sets the item's data for the given \a role to the specified \a value.

    If you subclass UiStandardItem and reimplement this function, your
    reimplementation should call emitDataChanged() if you do not call
    the base implementation of setData(). This will ensure that e.g.
    views using the model are notified of the changes.

    \note The default implementation treats Qt::EditRole and Qt::DisplayRole
    as referring to the same data.

    \sa Qt::ItemDataRole, data(), setFlags()
*/
void UiStandardItem::setData(const QVariant &value, int role)
{
    Q_D(UiStandardItem);
    role = (role == Qt::EditRole) ? Qt::DisplayRole : role;
    QVector<UiStandardItemData>::iterator it;
    for (it = d->values.begin(); it != d->values.end(); ++it) {
        if ((*it).role == role) {
            if (value.isValid()) {
                if ((*it).value.type() == value.type() && (*it).value == value)
                    return;
                (*it).value = value;
            } else {
                d->values.erase(it);
            }
            if (d->model)
                d->model->d_func()->itemChanged(this);
            return;
        }
    }
    d->values.append(UiStandardItemData(role, value));
    if (d->model)
        d->model->d_func()->itemChanged(this);
}

/*!
    Returns the item's data for the given \a role, or an invalid
    QVariant if there is no data for the role.

    \note The default implementation treats Qt::EditRole and Qt::DisplayRole
    as referring to the same data.
*/
QVariant UiStandardItem::data(int role) const
{
    Q_D(const UiStandardItem);
    role = (role == Qt::EditRole) ? Qt::DisplayRole : role;
    QVector<UiStandardItemData>::const_iterator it;
    for (it = d->values.begin(); it != d->values.end(); ++it) {
        if ((*it).role == role)
            return (*it).value;
    }
    return QVariant();
}

/*!
  \since 4.4

  Causes the model associated with this item to emit a
  \l{QAbstractItemModel::dataChanged()}{dataChanged}() signal for this
  item.

  You normally only need to call this function if you have subclassed
  UiStandardItem and reimplemented data() and/or setData().

  \sa setData()
*/
void UiStandardItem::emitDataChanged()
{
    Q_D(UiStandardItem);
    if (d->model)
        d->model->d_func()->itemChanged(this);
}

/*!
  Sets the item flags for the item to \a flags.

  The item flags determine how the user can interact with the item.
  This is often used to disable an item.

  \sa flags(), setData()
*/
void UiStandardItem::setFlags(Qt::ItemFlags flags)
{
    setData((int)flags, Qt::UserRole - 1);
}

/*!
  Returns the item flags for the item.

  The item flags determine how the user can interact with the item.

  By default, items are enabled, editable, selectable, checkable, and can be
  used both as the source of a drag and drop operation and as a drop target.

  \sa setFlags()
*/
Qt::ItemFlags UiStandardItem::flags() const
{
    QVariant v = data(Qt::UserRole - 1);
    if (!v.isValid())
        return (Qt::ItemIsEnabled|Qt::ItemIsEditable);
    return Qt::ItemFlags(v.toInt());
}

/*!
    \fn QString UiStandardItem::text() const

    Returns the item's text. This is the text that's presented to the user
    in a view.

    \sa setText()
*/

/*!
    \fn void UiStandardItem::setText(const QString &text)

    Sets the item's text to the \a text specified.

    \sa text(), setFont(), setForeground()
*/

/*!
    \fn QIcon UiStandardItem::icon() const

    Returns the item's icon.

    \sa setIcon(), {QAbstractItemView::iconSize}{iconSize}
*/

/*!
    \fn void UiStandardItem::setIcon(const QIcon &icon)

    Sets the item's icon to the \a icon specified.
*/

/*!
    \fn QString UiStandardItem::statusTip() const

    Returns the item's status tip.

    \sa setStatusTip(), toolTip(), whatsThis()
*/

/*!
    \fn void UiStandardItem::setStatusTip(const QString &statusTip)

    Sets the item's status tip to the string specified by \a statusTip.

    \sa statusTip(), setToolTip(), setWhatsThis()
*/

/*!
    \fn QString UiStandardItem::toolTip() const

    Returns the item's tooltip.

    \sa setToolTip(), statusTip(), whatsThis()
*/

/*!
    \fn void UiStandardItem::setToolTip(const QString &toolTip)

    Sets the item's tooltip to the string specified by \a toolTip.

    \sa toolTip(), setStatusTip(), setWhatsThis()
*/

/*!
    \fn QString UiStandardItem::whatsThis() const

    Returns the item's "What's This?" help.

    \sa setWhatsThis(), toolTip(), statusTip()
*/

/*!
    \fn void UiStandardItem::setWhatsThis(const QString &whatsThis)

    Sets the item's "What's This?" help to the string specified by \a whatsThis.

    \sa whatsThis(), setStatusTip(), setToolTip()
*/

/*!
    \fn QFont UiStandardItem::font() const

    Returns the font used to render the item's text.

    \sa setFont()
*/

/*!
    \fn void UiStandardItem::setFont(const QFont &font)

    Sets the font used to display the item's text to the given \a font.

    \sa font() setText() setForeground()
*/

/*!
    \fn QBrush UiStandardItem::background() const

    Returns the brush used to render the item's background.

    \sa  foreground() setBackground()
*/

/*!
    \fn void UiStandardItem::setBackground(const QBrush &brush)

    Sets the item's background brush to the specified \a brush.

    \sa background() setForeground()
*/

/*!
    \fn QBrush UiStandardItem::foreground() const

    Returns the brush used to render the item's foreground (e.g. text).

    \sa setForeground() background()
*/

/*!
    \fn void UiStandardItem::setForeground(const QBrush &brush)

    Sets the brush used to display the item's foreground (e.g. text) to the
    given \a brush.

    \sa foreground() setBackground() setFont()
*/

/*!
    \fn int UiStandardItem::textAlignment() const

    Returns the text alignment for the item's text.
*/

/*!
    \fn void UiStandardItem::setTextAlignment(Qt::Alignment alignment)

    Sets the text alignment for the item's text to the \a alignment
    specified.

    \sa textAlignment()
*/

/*!
    \fn QSize UiStandardItem::sizeHint() const

    Returns the size hint set for the item, or an invalid QSize if no
    size hint has been set.

    If no size hint has been set, the item delegate will compute the
    size hint based on the item data.

    \sa setSizeHint()
*/

/*!
    \fn void UiStandardItem::setSizeHint(const QSize &size)

    Sets the size hint for the item to be \a size.
    If no size hint is set, the item delegate will compute the
    size hint based on the item data.

    \sa sizeHint()
*/

/*!
    \fn Qt::CheckState UiStandardItem::checkState() const

    Returns the checked state of the item.

    \sa setCheckState(), isCheckable()
*/

/*!
    \fn void UiStandardItem::setCheckState(Qt::CheckState state)

    Sets the check state of the item to be \a state.

    \sa checkState(), setCheckable()
*/

/*!
    \fn QString UiStandardItem::accessibleText() const

    Returns the item's accessible text.

    The accessible text is used by assistive technologies (i.e. for users who
    cannot use conventional means of interaction).

    \sa setAccessibleText(), accessibleDescription()
*/

/*!
    \fn void UiStandardItem::setAccessibleText(const QString &accessibleText)

    Sets the item's accessible text to the string specified by \a accessibleText.

    The accessible text is used by assistive technologies (i.e. for users who
    cannot use conventional means of interaction).

    \sa accessibleText(), setAccessibleDescription()
*/

/*!
    \fn QString UiStandardItem::accessibleDescription() const

    Returns the item's accessible description.

    The accessible description is used by assistive technologies (i.e. for
    users who cannot use conventional means of interaction).

    \sa setAccessibleDescription(), accessibleText()
*/

/*!
    \fn void UiStandardItem::setAccessibleDescription(const QString &accessibleDescription)

    Sets the item's accessible description to the string specified by \a
    accessibleDescription.

    The accessible description is used by assistive technologies (i.e. for
    users who cannot use conventional means of interaction).

    \sa accessibleDescription(), setAccessibleText()
*/

/*!
  Sets whether the item is enabled. If \a enabled is true, the item is enabled,
  meaning that the user can interact with the item; if \a enabled is false, the
  user cannot interact with the item.

  This flag takes precedence over the other item flags; e.g. if an item is not
  enabled, it cannot be selected by the user, even if the Qt::ItemIsSelectable
  flag has been set.

  \sa isEnabled(), Qt::ItemIsEnabled, setFlags()
*/
void UiStandardItem::setEnabled(bool enabled)
{
   Q_D(UiStandardItem);
   d->changeFlags(enabled, Qt::ItemIsEnabled);
}

/*!
  \fn bool UiStandardItem::isEnabled() const

  Returns whether the item is enabled.

  When an item is enabled, the user can interact with it. The possible
  types of interaction are specified by the other item flags, such as
  isEditable() and isSelectable().

  The default value is true.

  \sa setEnabled(), flags()
*/

/*!
  Sets whether the item is editable. If \a editable is true, the item can be
  edited by the user; otherwise, the user cannot edit the item.

  How the user can edit items in a view is determined by the view's edit
  triggers; see QAbstractItemView::editTriggers.

  \sa isEditable(), setFlags()
*/
void UiStandardItem::setEditable(bool editable)
{
    Q_D(UiStandardItem);
    d->changeFlags(editable, Qt::ItemIsEditable);
}

/*!
  \fn bool UiStandardItem::isEditable() const

  Returns whether the item can be edited by the user.

  When an item is editable (and enabled), the user can edit the item by
  invoking one of the view's edit triggers; see
  QAbstractItemView::editTriggers.

  The default value is true.

  \sa setEditable(), flags()
*/

/*!
  Returns the row where the item is located in its parent's child table, or
  -1 if the item has no parent.

  \sa column(), parent()
*/
int UiStandardItem::row() const
{
    Q_D(const UiStandardItem);
    QPair<int, int> pos = d->position();
    return pos.first;
}

/*!
  Returns the column where the item is located in its parent's child table,
  or -1 if the item has no parent.

  \sa row(), parent()
*/
int UiStandardItem::column() const
{
    Q_D(const UiStandardItem);
    QPair<int, int> pos = d->position();
    return pos.second;
}

/*!
  Returns the QModelIndex associated with this item.

  When you need to invoke item functionality in a QModelIndex-based API (e.g.
  QAbstractItemView), you can call this function to obtain an index that
  corresponds to the item's location in the model.

  If the item is not associated with a model, an invalid QModelIndex is
  returned.

  \sa model(), UiStandardItemModel::itemFromIndex()
*/
QModelIndex UiStandardItem::index() const
{
    Q_D(const UiStandardItem);
    return d->model ? d->model->indexFromItem(this) : QModelIndex();
}

/*!
  Returns the UiStandardItemModel that this item belongs to.

  If the item is not a child of another item that belongs to the model, this
  function returns 0.

  \sa index()
*/
UiStandardItemModel *UiStandardItem::model() const
{
    Q_D(const UiStandardItem);
    return d->model;
}

/*!
    Sets the number of child item rows to \a rows. If this is less than
    rowCount(), the data in the unwanted rows is discarded.

    \sa rowCount(), setColumnCount()
*/
void UiStandardItem::setRowCount(int rows)
{
    int rc = rowCount();
    if (rc == rows)
        return;
    if (rc < rows)
        insertRows(qMax(rc, 0), rows - rc);
    else
        removeRows(qMax(rows, 0), rc - rows);
}

/*!
    Returns the number of child item rows that the item has.

    \sa setRowCount(), columnCount()
*/
int UiStandardItem::rowCount() const
{
    Q_D(const UiStandardItem);
    return d->rowCount();
}

/*!
    Sets the number of child item columns to \a columns. If this is less than
    columnCount(), the data in the unwanted columns is discarded.

    \sa columnCount(), setRowCount()
*/
void UiStandardItem::setColumnCount(int columns)
{
    int cc = columnCount();
    if (cc == columns)
        return;
    if (cc < columns)
        insertColumns(qMax(cc, 0), columns - cc);
    else
        removeColumns(qMax(columns, 0), cc - columns);
}

/*!
    Returns the number of child item columns that the item has.

    \sa setColumnCount(), rowCount()
*/
int UiStandardItem::columnCount() const
{
    Q_D(const UiStandardItem);
    return d->columnCount();
}

/*!
    Inserts a row at \a row containing \a items. If necessary, the column
    count is increased to the size of \a items.

    \sa insertRows(), insertColumn()
*/
void UiStandardItem::insertRow(int row, const QList<UiStandardItem*> &items)
{
    Q_D(UiStandardItem);
    if (row < 0)
        return;
    if (columnCount() < items.count())
        setColumnCount(items.count());
    d->insertRows(row, 1, items);
}

/*!
    Inserts \a items at \a row. The column count wont be changed.

    \sa insertRow(), insertColumn()
*/
void UiStandardItem::insertRows(int row, const QList<UiStandardItem*> &items)
{
    Q_D(UiStandardItem);
    if (row < 0)
        return;
    d->insertRows(row, items);
}

/*!
    Inserts a column at \a column containing \a items. If necessary,
    the row count is increased to the size of \a items.

    \sa insertColumns(), insertRow()
*/
void UiStandardItem::insertColumn(int column, const QList<UiStandardItem*> &items)
{
    Q_D(UiStandardItem);
    if (column < 0)
        return;
    if (rowCount() < items.count())
        setRowCount(items.count());
    d->insertColumns(column, 1, items);
}

/*!
    Inserts \a count rows of child items at row \a row.

    \sa insertRow(), insertColumns()
*/
void UiStandardItem::insertRows(int row, int count)
{
    Q_D(UiStandardItem);
    if (rowCount() < row) {
        count += row - rowCount();
        row = rowCount();
    }
    d->insertRows(row, count, QList<UiStandardItem*>());
}

/*!
    Inserts \a count columns of child items at column \a column.

    \sa insertColumn(), insertRows()
*/
void UiStandardItem::insertColumns(int column, int count)
{
    Q_D(UiStandardItem);
    if (columnCount() < column) {
        count += column - columnCount();
        column = columnCount();
    }
    d->insertColumns(column, count, QList<UiStandardItem*>());
}

/*!
    \fn void UiStandardItem::appendRow(const QList<UiStandardItem*> &items)

    Appends a row containing \a items. If necessary, the column count is
    increased to the size of \a items.

    \sa insertRow()
*/

/*!
    \fn void UiStandardItem::appendRows(const QList<UiStandardItem*> &items)

    Appends rows containing \a items.  The column count will not change.

    \sa insertRow()
*/

/*!
    \fn void UiStandardItem::appendColumn(const QList<UiStandardItem*> &items)

    Appends a column containing \a items. If necessary, the row count is
    increased to the size of \a items.

    \sa insertColumn()
*/

/*!
    \fn bool UiStandardItemModel::insertRow(int row, const QModelIndex &parent)

    Inserts a single row before the given \a row in the child items of the
    \a parent specified. Returns true if the row is inserted; otherwise
    returns false.

    \sa insertRows(), insertColumn(), removeRow()
*/

/*!
    \fn bool UiStandardItemModel::insertColumn(int column, const QModelIndex &parent)

    Inserts a single column before the given \a column in the child items of
    the \a parent specified. Returns true if the column is inserted; otherwise
    returns false.

    \sa insertColumns(), insertRow(), removeColumn()
*/

/*!
    \fn UiStandardItem::insertRow(int row, UiStandardItem *item)
    \overload

    Inserts a row at \a row containing \a item.

    When building a list or a tree that has only one column, this function
    provides a convenient way to insert a single new item.
*/

/*!
    \fn UiStandardItem::appendRow(UiStandardItem *item)
    \overload

    Appends a row containing \a item.

    When building a list or a tree that has only one column, this function
    provides a convenient way to append a single new item.
*/

/*!
    Removes the given \a row. The items that were in the row are deleted.

    \sa takeRow(), removeRows(), removeColumn()
*/
void UiStandardItem::removeRow(int row)
{
    removeRows(row, 1);
}

/*!
    Removes the given \a column. The items that were in the
    column are deleted.

    \sa takeColumn(), removeColumns(), removeRow()
*/
void UiStandardItem::removeColumn(int column)
{
    removeColumns(column, 1);
}

/*!
    Removes \a count rows at row \a row. The items that were in those rows are
    deleted.

    \sa removeRow(), removeColumn()
*/
void UiStandardItem::removeRows(int row, int count)
{
    Q_D(UiStandardItem);
    if ((count < 1) || (row < 0) || ((row + count) > rowCount()))
        return;
    if (d->model)
        d->model->d_func()->rowsAboutToBeRemoved(this, row, row + count - 1);
    int i = d->childIndex(row, 0);
    int n = count * d->columnCount();
    for (int j = i; j < n+i; ++j) {
        UiStandardItem *oldItem = d->children.at(j);
        if (oldItem)
            oldItem->d_func()->setModel(0);
        delete oldItem;
    }
    d->children.remove(qMax(i, 0), n);
    d->rows -= count;
    if (d->model)
        d->model->d_func()->rowsRemoved(this, row, count);
}

/*!
    Removes \a count columns at column \a column. The items that were in those
    columns are deleted.

    \sa removeColumn(), removeRows()
*/
void UiStandardItem::removeColumns(int column, int count)
{
    Q_D(UiStandardItem);
    if ((count < 1) || (column < 0) || ((column + count) > columnCount()))
        return;
    if (d->model)
        d->model->d_func()->columnsAboutToBeRemoved(this, column, column + count - 1);
    for (int row = d->rowCount() - 1; row >= 0; --row) {
        int i = d->childIndex(row, column);
        for (int j=i; j<i+count; ++j) {
            UiStandardItem *oldItem = d->children.at(j);
            if (oldItem)
                oldItem->d_func()->setModel(0);
            delete oldItem;
        }
        d->children.remove(i, count);
    }
    d->columns -= count;
    if (d->model)
        d->model->d_func()->columnsRemoved(this, column, count);
}

/*!
    Returns true if this item has any children; otherwise returns false.

    \sa rowCount(), columnCount(), child()
*/
bool UiStandardItem::hasChildren() const
{
    return (rowCount() > 0) && (columnCount() > 0);
}

/*!
    Sets the child item at (\a row, \a column) to \a item. This item (the parent
    item) takes ownership of \a item. If necessary, the row count and column
    count are increased to fit the item.

    \sa child()
*/
void UiStandardItem::setChild(int row, int column, UiStandardItem *item)
{
    Q_D(UiStandardItem);
    d->setChild(row, column, item, true);
}

/*!
    \fn UiStandardItem::setChild(int row, UiStandardItem *item)
    \overload

    Sets the child at \a row to \a item.
*/

/*!
    Returns the child item at (\a row, \a column) if one has been set; otherwise
    returns 0.

    \sa setChild(), takeChild(), parent()
*/
UiStandardItem *UiStandardItem::child(int row, int column) const
{
    Q_D(const UiStandardItem);
    int index = d->childIndex(row, column);
    if (index == -1)
        return 0;
    return d->children.at(index);
}

/*!
    Removes the child item at (\a row, \a column) without deleting it, and returns
    a pointer to the item. If there was no child at the given location, then
    this function returns 0.

    Note that this function, unlike takeRow() and takeColumn(), does not affect
    the dimensions of the child table.

    \sa child(), takeRow(), takeColumn()
*/
UiStandardItem *UiStandardItem::takeChild(int row, int column)
{
    Q_D(UiStandardItem);
    UiStandardItem *item = 0;
    int index = d->childIndex(row, column);
    if (index != -1) {
        item = d->children.at(index);
        if (item)
            item->d_func()->setParentAndModel(0, 0);
        d->children.replace(index, 0);
    }
    return item;
}

/*!
    Removes \a row without deleting the row items, and returns a list of
    pointers to the removed items. For items in the row that have not been
    set, the corresponding pointers in the list will be 0.

    \sa removeRow(), insertRow(), takeColumn()
*/
QList<UiStandardItem*> UiStandardItem::takeRow(int row)
{
    Q_D(UiStandardItem);
    if ((row < 0) || (row >= rowCount()))
        return QList<UiStandardItem*>();
    if (d->model)
        d->model->d_func()->rowsAboutToBeRemoved(this, row, row);
    QList<UiStandardItem*> items;
    int index = d->childIndex(row, 0);  // Will return -1 if there are no columns
    if (index != -1) {
        int col_count = d->columnCount();
        for (int column = 0; column < col_count; ++column) {
            UiStandardItem *ch = d->children.at(index + column);
            if (ch)
                ch->d_func()->setParentAndModel(0, 0);
            items.append(ch);
        }
        d->children.remove(index, col_count);
    }
    d->rows--;
    if (d->model)
        d->model->d_func()->rowsRemoved(this, row, 1);
    return items;
}

/*!
    Removes \a column without deleting the column items, and returns a list of
    pointers to the removed items. For items in the column that have not been
    set, the corresponding pointers in the list will be 0.

    \sa removeColumn(), insertColumn(), takeRow()
*/
QList<UiStandardItem*> UiStandardItem::takeColumn(int column)
{
    Q_D(UiStandardItem);
    if ((column < 0) || (column >= columnCount()))
        return QList<UiStandardItem*>();
    if (d->model)
        d->model->d_func()->columnsAboutToBeRemoved(this, column, column);
    QList<UiStandardItem*> items;

    for (int row = d->rowCount() - 1; row >= 0; --row) {
        int index = d->childIndex(row, column);
        UiStandardItem *ch = d->children.at(index);
        if (ch)
            ch->d_func()->setParentAndModel(0, 0);
        d->children.remove(index);
        items.prepend(ch);
    }
    d->columns--;
    if (d->model)
        d->model->d_func()->columnsRemoved(this, column, 1);
    return items;
}

/*!
    Returns true if this item is less than \a other; otherwise returns false.

    The default implementation uses the data for the item's sort role (see
    UiStandardItemModel::sortRole) to perform the comparison if the item
    belongs to a model; otherwise, the data for the item's Qt::DisplayRole
    (text()) is used to perform the comparison.

    sortChildren() and UiStandardItemModel::sort() use this function when
    sorting items. If you want custom sorting, you can subclass UiStandardItem
    and reimplement this function.
*/
bool UiStandardItem::operator<(const UiStandardItem &other) const
{
    const int role = model() ? model()->sortRole() : Qt::DisplayRole;
    const QVariant l = data(role), r = other.data(role);
    // this code is copied from QSortFilterProxyModel::lessThan()
    switch (l.userType()) {
    case QVariant::Invalid:
        return (r.type() == QVariant::Invalid);
    case QVariant::Int:
        return l.toInt() < r.toInt();
    case QVariant::UInt:
        return l.toUInt() < r.toUInt();
    case QVariant::LongLong:
        return l.toLongLong() < r.toLongLong();
    case QVariant::ULongLong:
        return l.toULongLong() < r.toULongLong();
    case QMetaType::Float:
        return l.toFloat() < r.toFloat();
    case QVariant::Double:
        return l.toDouble() < r.toDouble();
    case QVariant::Char:
        return l.toChar() < r.toChar();
    case QVariant::Date:
        return l.toDate() < r.toDate();
    case QVariant::Time:
        return l.toTime() < r.toTime();
    case QVariant::DateTime:
        return l.toDateTime() < r.toDateTime();
    case QVariant::String:
    default:
        return l.toString().compare(r.toString()) < 0;
    }
}

/*!
    Sorts the children of the item using the given \a order, by the values in
    the given \a column.

    \note This function is recursive, therefore it sorts the children of the
    item, its grandchildren, etc.

    \sa {operator<()}
*/
void UiStandardItem::sortChildren(int column, Qt::SortOrder order)
{
    Q_D(UiStandardItem);
    if ((column < 0) || (rowCount() == 0))
        return;
    if (d->model)
        emit d->model->layoutAboutToBeChanged();
    d->sortChildren(column, order);
    if (d->model)
        emit d->model->layoutChanged();
}

/*!
    Returns a copy of this item. The item's children are not copied.

    When subclassing UiStandardItem, you can reimplement this function
    to provide UiStandardItemModel with a factory that it can use to
    create new items on demand.

    \sa UiStandardItemModel::setItemPrototype(), operator=()
*/
UiStandardItem *UiStandardItem::clone() const
{
    return new UiStandardItem(*this);
}

/*!
    Returns the type of this item. The type is used to distinguish custom
    items from the base class. When subclassing UiStandardItem, you should
    reimplement this function and return a new value greater than or equal
    to \l UserType.

    \sa UiStandardItem::Type
*/
int UiStandardItem::type() const
{
    return Type;
}

#ifndef QT_NO_DATASTREAM

/*!
    Reads the item from stream \a in. Only the data and flags of the item are
    read, not the child items.

    \sa write()
*/
void UiStandardItem::read(QDataStream &in)
{
    Q_D(UiStandardItem);
    in >> d->values;
    qint32 flags;
    in >> flags;
    setFlags(Qt::ItemFlags(flags));
}

/*!
    Writes the item to stream \a out. Only the data and flags of the item
    are written, not the child items.

    \sa read()
*/
void UiStandardItem::write(QDataStream &out) const
{
    Q_D(const UiStandardItem);
    out << d->values;
    out << flags();
}

/*!
    \relates UiStandardItem
    \since 4.2

    Reads a UiStandardItem from stream \a in into \a item.

    This operator uses UiStandardItem::read().

    \sa {Serializing Qt Data Types}
*/
QDataStream &operator>>(QDataStream &in, UiStandardItem &item)
{
    item.read(in);
    return in;
}

/*!
    \relates UiStandardItem
    \since 4.2

    Writes the UiStandardItem \a item to stream \a out.

    This operator uses UiStandardItem::write().

    \sa {Serializing Qt Data Types}
*/
QDataStream &operator<<(QDataStream &out, const UiStandardItem &item)
{
    item.write(out);
    return out;
}

#endif // QT_NO_DATASTREAM

/*!
    \class UiStandardItemModel
    \brief The UiStandardItemModel class provides a generic model for storing custom data.
    \ingroup model-view
    \inmodule QtWidgets

    UiStandardItemModel can be used as a repository for standard Qt
    data types. It is one of the \l {Model/View Classes} and is part
    of Qt's \l {Model/View Programming}{model/view} framework.

    UiStandardItemModel provides a classic item-based approach to working with
    the model.  The items in a UiStandardItemModel are provided by
    UiStandardItem.

    UiStandardItemModel implements the QAbstractItemModel interface, which
    means that the model can be used to provide data in any view that supports
    that interface (such as QListView, QTableView and QTreeView, and your own
    custom views). For performance and flexibility, you may want to subclass
    QAbstractItemModel to provide support for different kinds of data
    repositories. For example, the QDirModel provides a model interface to the
    underlying file system.

    When you want a list or tree, you typically create an empty
    UiStandardItemModel and use appendRow() to add items to the model, and
    item() to access an item.  If your model represents a table, you typically
    pass the dimensions of the table to the UiStandardItemModel constructor and
    use setItem() to position items into the table. You can also use setRowCount()
    and setColumnCount() to alter the dimensions of the model. To insert items,
    use insertRow() or insertColumn(), and to remove items, use removeRow() or
    removeColumn().

    You can set the header labels of your model with setHorizontalHeaderLabels()
    and setVerticalHeaderLabels().

    You can search for items in the model with findItems(), and sort the model by
    calling sort().

    Call clear() to remove all items from the model.

    An example usage of UiStandardItemModel to create a table:

    \snippet doc/src/snippets/code/src_gui_itemviews_UiStandardItemModel.cpp 0

    An example usage of UiStandardItemModel to create a tree:

    \snippet doc/src/snippets/code/src_gui_itemviews_UiStandardItemModel.cpp 1

    After setting the model on a view, you typically want to react to user
    actions, such as an item being clicked. Since a QAbstractItemView provides
    QModelIndex-based signals and functions, you need a way to obtain the
    UiStandardItem that corresponds to a given QModelIndex, and vice
    versa. itemFromIndex() and indexFromItem() provide this mapping. Typical
    usage of itemFromIndex() includes obtaining the item at the current index
    in a view, and obtaining the item that corresponds to an index carried by
    a QAbstractItemView signal, such as QAbstractItemView::clicked(). First
    you connect the view's signal to a slot in your class:

    \snippet doc/src/snippets/code/src_gui_itemviews_UiStandardItemModel.cpp 2

    When you receive the signal, you call itemFromIndex() on the given model
    index to get a pointer to the item:

    \snippet doc/src/snippets/code/src_gui_itemviews_UiStandardItemModel.cpp 3

    Conversely, you must obtain the QModelIndex of an item when you want to
    invoke a model/view function that takes an index as argument. You can
    obtain the index either by using the model's indexFromItem() function, or,
    equivalently, by calling UiStandardItem::index():

    \snippet doc/src/snippets/code/src_gui_itemviews_UiStandardItemModel.cpp 4

    You are, of course, not required to use the item-based approach; you could
    instead rely entirely on the QAbstractItemModel interface when working with
    the model, or use a combination of the two as appropriate.

    \sa UiStandardItem, {Model/View Programming}, QAbstractItemModel,
    {itemviews/simpletreemodel}{Simple Tree Model example},
    {Item View Convenience Classes}
*/

/*!
    \fn void UiStandardItemModel::itemChanged(UiStandardItem *item)
    \since 4.2

    This signal is emitted whenever the data of \a item has changed.
*/

/*!
    Constructs a new item model with the given \a parent.
*/
UiStandardItemModel::UiStandardItemModel(QObject *parent)
    : QAbstractItemModel(*new UiStandardItemModelPrivate, parent)
{
    Q_D(UiStandardItemModel);
    d->init();
    d->root->d_func()->setModel(this);
}

/*!
    Constructs a new item model that initially has \a rows rows and \a columns
    columns, and that has the given \a parent.
*/
UiStandardItemModel::UiStandardItemModel(int rows, int columns, QObject *parent)
    : QAbstractItemModel(*new UiStandardItemModelPrivate, parent)
{
    Q_D(UiStandardItemModel);
    d->init();
    d->root->insertColumns(0, columns);
    d->root->insertRows(0, rows);
    d->root->d_func()->setModel(this);
}

/*!
  \internal
*/
UiStandardItemModel::UiStandardItemModel(UiStandardItemModelPrivate &dd, QObject *parent)
    : QAbstractItemModel(dd, parent)
{
    Q_D(UiStandardItemModel);
    d->init();
}

/*!
    Destructs the model. The model destroys all its items.
*/
UiStandardItemModel::~UiStandardItemModel()
{
}

/*!
    Sets the item role names to \a roleNames.
*/
void UiStandardItemModel::setItemRoleNames(const QHash<int,QByteArray> &roleNames)
{
    Q_D(UiStandardItemModel);
    d->roleNames = roleNames;
}

/*!
    Removes all items (including header items) from the model and sets the
    number of rows and columns to zero.

    \sa removeColumns(), removeRows()
*/
void UiStandardItemModel::clear()
{
    Q_D(UiStandardItemModel);
    beginResetModel();
    d->root.reset(new UiStandardItem);
    d->root->d_func()->setModel(this);
    endResetModel();
}

/*!
    \since 4.2

    Returns a pointer to the UiStandardItem associated with the given \a index.

    Calling this function is typically the initial step when processing
    QModelIndex-based signals from a view, such as
    QAbstractItemView::activated(). In your slot, you call itemFromIndex(),
    with the QModelIndex carried by the signal as argument, to obtain a
    pointer to the corresponding UiStandardItem.

    Note that this function will lazily create an item for the index (using
    itemPrototype()), and set it in the parent item's child table, if no item
    already exists at that index.

    If \a index is an invalid index, this function returns 0.

    \sa indexFromItem()
*/
UiStandardItem *UiStandardItemModel::itemFromIndex(const QModelIndex &index) const
{
    Q_D(const UiStandardItemModel);
    if ((index.row() < 0) || (index.column() < 0) || (index.model() != this))
        return 0;
    UiStandardItem *parent = static_cast<UiStandardItem*>(index.internalPointer());
    if (parent == 0)
        return 0;
    UiStandardItem *item = parent->child(index.row(), index.column());
    // lazy part
    if (item == 0) {
        item = d->createItem();
        parent->d_func()->setChild(index.row(), index.column(), item);
    }
    return item;
}

/*!
    \since 4.2

    Returns the QModelIndex associated with the given \a item.

    Use this function when you want to perform an operation that requires the
    QModelIndex of the item, such as
    QAbstractItemView::scrollTo(). UiStandardItem::index() is provided as
    convenience; it is equivalent to calling this function.

    \sa itemFromIndex(), UiStandardItem::index()
*/
QModelIndex UiStandardItemModel::indexFromItem(const UiStandardItem *item) const
{
    if (item && item->d_func()->parent) {
        QPair<int, int> pos = item->d_func()->position();
        return createIndex(pos.first, pos.second, item->d_func()->parent);
    }
    return QModelIndex();
}

/*!
    \since 4.2

    Sets the number of rows in this model to \a rows. If
    this is less than rowCount(), the data in the unwanted rows
    is discarded.

    \sa setColumnCount()
*/
void UiStandardItemModel::setRowCount(int rows)
{
    Q_D(UiStandardItemModel);
    d->root->setRowCount(rows);
}

/*!
    \since 4.2

    Sets the number of columns in this model to \a columns. If
    this is less than columnCount(), the data in the unwanted columns
    is discarded.

    \sa setRowCount()
*/
void UiStandardItemModel::setColumnCount(int columns)
{
    Q_D(UiStandardItemModel);
    d->root->setColumnCount(columns);
}

/*!
    \since 4.2

    Sets the item for the given \a row and \a column to \a item. The model
    takes ownership of the item. If necessary, the row count and column count
    are increased to fit the item. The previous item at the given location (if
    there was one) is deleted.

    \sa item()
*/
void UiStandardItemModel::setItem(int row, int column, UiStandardItem *item)
{
    Q_D(UiStandardItemModel);
    d->root->d_func()->setChild(row, column, item, true);
}

/*!
  \fn UiStandardItemModel::setItem(int row, UiStandardItem *item)
  \overload
*/

/*!
    \since 4.2

    Returns the item for the given \a row and \a column if one has been set;
    otherwise returns 0.

    \sa setItem(), takeItem(), itemFromIndex()
*/
UiStandardItem *UiStandardItemModel::item(int row, int column) const
{
    Q_D(const UiStandardItemModel);
    return d->root->child(row, column);
}

/*!
    \since 4.2

    Returns the model's invisible root item.

    The invisible root item provides access to the model's top-level items
    through the UiStandardItem API, making it possible to write functions that
    can treat top-level items and their children in a uniform way; for
    example, recursive functions involving a tree model.

    \note Calling \l{QAbstractItemModel::index()}{index()} on the UiStandardItem object
    retrieved from this function is not valid.
*/
UiStandardItem *UiStandardItemModel::invisibleRootItem() const
{
    Q_D(const UiStandardItemModel);
    return d->root.data();
}

/*!
    \since 4.2

    Sets the item prototype for the model to the specified \a item. The model
    takes ownership of the prototype.

    The item prototype acts as a UiStandardItem factory, by relying on the
    UiStandardItem::clone() function.  To provide your own prototype, subclass
    UiStandardItem, reimplement UiStandardItem::clone() and set the prototype to
    be an instance of your custom class. Whenever UiStandardItemModel needs to
    create an item on demand (for instance, when a view or item delegate calls
    setData())), the new items will be instances of your custom class.

    \sa itemPrototype(), UiStandardItem::clone()
*/
void UiStandardItemModel::setItemPrototype(const UiStandardItem *item)
{
    Q_D(UiStandardItemModel);
    if (d->itemPrototype != item) {
        delete d->itemPrototype;
        d->itemPrototype = item;
    }
}

/*!
    \since 4.2

    Returns the item prototype used by the model. The model uses the item
    prototype as an item factory when it needs to construct new items on
    demand (for instance, when a view or item delegate calls setData()).

    \sa setItemPrototype()
*/
const UiStandardItem *UiStandardItemModel::itemPrototype() const
{
    Q_D(const UiStandardItemModel);
    return d->itemPrototype;
}

/*!
    \since 4.2

    Returns a list of items that match the given \a text, using the given \a
    flags, in the given \a column.
*/
QList<UiStandardItem*> UiStandardItemModel::findItems(const QString &text,
                                                    Qt::MatchFlags flags, int column) const
{
    QModelIndexList indexes = match(index(0, column, QModelIndex()),
                                    Qt::DisplayRole, text, -1, flags);
    QList<UiStandardItem*> items;
    for (int i = 0; i < indexes.size(); ++i)
        items.append(itemFromIndex(indexes.at(i)));
    return items;
}

/*!
    \since 4.2

    Appends a row containing \a items. If necessary, the column count is
    increased to the size of \a items.

    \sa insertRow(), appendColumn()
*/
void UiStandardItemModel::appendRow(const QList<UiStandardItem*> &items)
{
    invisibleRootItem()->appendRow(items);
}

/*!
    \since 4.2

    Appends a column containing \a items. If necessary, the row count is
    increased to the size of \a items.

    \sa insertColumn(), appendRow()
*/
void UiStandardItemModel::appendColumn(const QList<UiStandardItem*> &items)
{
    invisibleRootItem()->appendColumn(items);
}

/*!
    \since 4.2
    \fn UiStandardItemModel::appendRow(UiStandardItem *item)
    \overload

    When building a list or a tree that has only one column, this function
    provides a convenient way to append a single new \a item.
*/

/*!
    \since 4.2

    Inserts a row at \a row containing \a items. If necessary, the column
    count is increased to the size of \a items.

    \sa takeRow(), appendRow(), insertColumn()
*/
void UiStandardItemModel::insertRow(int row, const QList<UiStandardItem*> &items)
{
    invisibleRootItem()->insertRow(row, items);
}

/*!
    \since 4.2

    \fn void UiStandardItemModel::insertRow(int row, UiStandardItem *item)
    \overload

    Inserts a row at \a row containing \a item.

    When building a list or a tree that has only one column, this function
    provides a convenient way to append a single new item.
*/

/*!
    \since 4.2

    Inserts a column at \a column containing \a items. If necessary, the row
    count is increased to the size of \a items.

    \sa takeColumn(), appendColumn(), insertRow()
*/
void UiStandardItemModel::insertColumn(int column, const QList<UiStandardItem*> &items)
{
    invisibleRootItem()->insertColumn(column, items);
}

/*!
    \since 4.2

    Removes the item at (\a row, \a column) without deleting it. The model
    releases ownership of the item.

    \sa item(), takeRow(), takeColumn()
*/
UiStandardItem *UiStandardItemModel::takeItem(int row, int column)
{
    Q_D(UiStandardItemModel);
    return d->root->takeChild(row, column);
}

/*!
    \since 4.2

    Removes the given \a row without deleting the row items, and returns a
    list of pointers to the removed items. The model releases ownership of the
    items. For items in the row that have not been set, the corresponding
    pointers in the list will be 0.

    \sa takeColumn()
*/
QList<UiStandardItem*> UiStandardItemModel::takeRow(int row)
{
    Q_D(UiStandardItemModel);
    return d->root->takeRow(row);
}

/*!
    \since 4.2

    Removes the given \a column without deleting the column items, and returns
    a list of pointers to the removed items. The model releases ownership of
    the items. For items in the column that have not been set, the
    corresponding pointers in the list will be 0.

    \sa takeRow()
*/
QList<UiStandardItem*> UiStandardItemModel::takeColumn(int column)
{
    Q_D(UiStandardItemModel);
    return d->root->takeColumn(column);
}

/*!
    \since 4.2
    \property UiStandardItemModel::sortRole
    \brief the item role that is used to query the model's data when sorting items

    The default value is Qt::DisplayRole.

    \sa sort(), UiStandardItem::sortChildren()
*/
int UiStandardItemModel::sortRole() const
{
    Q_D(const UiStandardItemModel);
    return d->sortRole;
}

void UiStandardItemModel::setSortRole(int role)
{
    Q_D(UiStandardItemModel);
    d->sortRole = role;
}

/*!
  \reimp
*/
int UiStandardItemModel::columnCount(const QModelIndex &parent) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(parent);
    return item ? item->columnCount() : 0;
}

/*!
  \reimp
*/
QVariant UiStandardItemModel::data(const QModelIndex &index, int role) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(index);
    return item ? item->data(role) : QVariant();
}

/*!
  \reimp
*/
Qt::ItemFlags UiStandardItemModel::flags(const QModelIndex &index) const
{
    Q_D(const UiStandardItemModel);
    if (!d->indexValid(index))
        return d->root->flags();
    UiStandardItem *item = d->itemFromIndex(index);
    if (item)
        return item->flags();
    return Qt::ItemIsEnabled|Qt::ItemIsEditable;
}

/*!
  \reimp
*/
bool UiStandardItemModel::hasChildren(const QModelIndex &parent) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(parent);
    return item ? item->hasChildren() : false;
}

/*!
  \reimp
*/
QModelIndex UiStandardItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *parentItem = d->itemFromIndex(parent);
    if ((parentItem == 0)
        || (row < 0)
        || (column < 0)
        || (row >= parentItem->rowCount())
        || (column >= parentItem->columnCount())) {
        return QModelIndex();
    }
    return createIndex(row, column, parentItem);
}

/*!
  \reimp
*/
bool UiStandardItemModel::insertColumns(int column, int count, const QModelIndex &parent)
{
    Q_D(UiStandardItemModel);
    UiStandardItem *item = parent.isValid() ? itemFromIndex(parent) : d->root.data();
    if (item == 0)
        return false;
    return item->d_func()->insertColumns(column, count, QList<UiStandardItem*>());
}

/*!
  \reimp
*/
bool UiStandardItemModel::insertRows(int row, int count, const QModelIndex &parent)
{
    Q_D(UiStandardItemModel);
    UiStandardItem *item = parent.isValid() ? itemFromIndex(parent) : d->root.data();
    if (item == 0)
        return false;
    return item->d_func()->insertRows(row, count, QList<UiStandardItem*>());
}

/*!
  \reimp
*/
QMap<int, QVariant> UiStandardItemModel::itemData(const QModelIndex &index) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(index);
    return item ? item->d_func()->itemData() : QMap<int, QVariant>();
}

/*!
  \reimp
*/
QModelIndex UiStandardItemModel::parent(const QModelIndex &child) const
{
    Q_D(const UiStandardItemModel);
    if (!d->indexValid(child))
        return QModelIndex();
    UiStandardItem *parentItem = static_cast<UiStandardItem*>(child.internalPointer());
    return indexFromItem(parentItem);
}

/*!
  \reimp
*/
bool UiStandardItemModel::removeColumns(int column, int count, const QModelIndex &parent)
{
    Q_D(UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(parent);
    if ((item == 0) || (count < 1) || (column < 0) || ((column + count) > item->columnCount()))
        return false;
    item->removeColumns(column, count);
    return true;
}

/*!
  \reimp
*/
bool UiStandardItemModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_D(UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(parent);
    if ((item == 0) || (count < 1) || (row < 0) || ((row + count) > item->rowCount()))
        return false;
    item->removeRows(row, count);
    return true;
}

/*!
  \reimp
*/
int UiStandardItemModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const UiStandardItemModel);
    UiStandardItem *item = d->itemFromIndex(parent);
    return item ? item->rowCount() : 0;
}

/*!
  \reimp
*/
bool UiStandardItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;
    UiStandardItem *item = itemFromIndex(index);
    if (item == 0)
        return false;
    item->setData(value, role);
    return true;
}

/*!
  \reimp
*/
bool UiStandardItemModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles)
{
    UiStandardItem *item = itemFromIndex(index);
    if (item == 0)
        return false;
    item->d_func()->setItemData(roles);
    return true;
}

/*!
  \reimp
*/
void UiStandardItemModel::sort(int column, Qt::SortOrder order)
{
    Q_D(UiStandardItemModel);
    d->root->sortChildren(column, order);
}

/*!
  \fn QObject *UiStandardItemModel::parent() const
  \internal
*/


/*!
  \reimp
*/
QStringList UiStandardItemModel::mimeTypes() const
{
    return QAbstractItemModel::mimeTypes() <<  QLatin1String("application/x-UiStandardItemModeldatalist");
}

/*!
  \reimp
*/
QMimeData *UiStandardItemModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *data = QAbstractItemModel::mimeData(indexes);
    if (!data)
        return 0;

    QString format = QLatin1String("application/x-UiStandardItemModeldatalist");
    if (!mimeTypes().contains(format))
        return data;
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    QSet<UiStandardItem*> itemsSet;
    QStack<UiStandardItem*> stack;
    itemsSet.reserve(indexes.count());
    stack.reserve(indexes.count());
    for (int i = 0; i < indexes.count(); ++i) {
        UiStandardItem *item = itemFromIndex(indexes.at(i));
        itemsSet << item;
        stack.push(item);
    }

    //remove duplicates childrens
    {
        QSet<UiStandardItem *> seen;
        while (!stack.isEmpty()) {
            UiStandardItem *itm = stack.pop();
            if (seen.contains(itm))
                continue;
            seen.insert(itm);

            const QVector<UiStandardItem*> &childList = itm->d_func()->children;
            for (int i = 0; i < childList.count(); ++i) {
                UiStandardItem *chi = childList.at(i);
                if (chi) {
                    QSet<UiStandardItem *>::iterator it = itemsSet.find(chi);
                    if (it != itemsSet.end()) {
                        itemsSet.erase(it);
                    }
                    stack.push(chi);
                }
            }
        }
    }

    stack.reserve(itemsSet.count());
    foreach (UiStandardItem *item, itemsSet) {
        stack.push(item);
    }

    //stream everything recursively
    while (!stack.isEmpty()) {
        UiStandardItem *item = stack.pop();
        if (itemsSet.contains(item)) { //if the item is selection 'top-level', strem its position
            stream << item->row() << item->column();
        }
        if (item) {
            stream << *item << item->columnCount() << item->d_ptr->children.count();
            stack += item->d_ptr->children;
        } else {
            UiStandardItem dummy;
            stream << dummy << 0 << 0;
        }
    }

    data->setData(format, encoded);
    return data;
}

QT_END_NAMESPACE_UIHELPERS

#include "moc_uistandarditemmodel.cpp"

#endif // QT_NO_STANDARDITEMMODEL
