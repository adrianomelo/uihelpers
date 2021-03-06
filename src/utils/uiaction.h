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

#ifndef QACTION_H
#define QACTION_H

#include "uihelpersglobal.h"
// #include <QtGui/qkeysequence.h> // XXX: Won't be removed
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtCore/QEvent>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE_UIHELPERS


#ifndef QT_NO_ACTION

class UiAction;
class UiActionGroup;
class UiActionPrivate;

class UIHELPERS_EXPORT UiActionEvent : public QEvent
{
    UiAction *act, *bef;
public:
    UiActionEvent(int type, UiAction *action, UiAction *before = 0);
    ~UiActionEvent();

    inline UiAction *action() const { return act; }
    inline UiAction *before() const { return bef; }
};

class UIHELPERS_EXPORT UiAction : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(UiAction)

    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY changed)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked DESIGNABLE isCheckable NOTIFY toggled)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QString iconText READ iconText WRITE setIconText NOTIFY changed)
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip NOTIFY changed)
    Q_PROPERTY(QString statusTip READ statusTip WRITE setStatusTip NOTIFY changed)
    Q_PROPERTY(QString whatsThis READ whatsThis WRITE setWhatsThis NOTIFY changed)
#ifndef QT_NO_SHORTCUT
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY changed)
    Q_PROPERTY(Qt::ShortcutContext shortcutContext READ shortcutContext WRITE setShortcutContext NOTIFY changed)
    Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat NOTIFY changed)
#endif
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)

public:
    explicit UiAction(QObject* parent);
    UiAction(const QString &text, QObject* parent);

    ~UiAction();

    void setActionGroup(UiActionGroup *group);
    UiActionGroup *actionGroup() const;

    void setText(const QString &text);
    QString text() const;

    void setIconText(const QString &text);
    QString iconText() const;

    void setToolTip(const QString &tip);
    QString toolTip() const;

    void setStatusTip(const QString &statusTip);
    QString statusTip() const;

    void setWhatsThis(const QString &what);
    QString whatsThis() const;

#ifndef QT_NO_SHORTCUT
    void setShortcut(const QKeySequence &shortcut);
    QKeySequence shortcut() const;

    void setShortcuts(const QList<QKeySequence> &shortcuts);
    void setShortcuts(QKeySequence::StandardKey);
    QList<QKeySequence> shortcuts() const;

    void setShortcutContext(Qt::ShortcutContext context);
    Qt::ShortcutContext shortcutContext() const;

    void setAutoRepeat(bool);
    bool autoRepeat() const;
#endif

    void setCheckable(bool);
    bool isCheckable() const;

    QVariant data() const;
    void setData(const QVariant &var);

    bool isChecked() const;

    bool isEnabled() const;

    bool isVisible() const;

    enum ActionEvent { Trigger };
    void activate(ActionEvent event);

protected:
    bool event(QEvent *);
    UiAction(UiActionPrivate &dd, QObject *parent);

public Q_SLOTS:
    void trigger() { activate(Trigger); }
    void setChecked(bool);
    void toggle();
    void setEnabled(bool);
    inline void setDisabled(bool b) { setEnabled(!b); }
    void setVisible(bool);

Q_SIGNALS:
    void changed();
    void triggered(bool checked = false);
    void toggled(bool);

private:
    Q_DISABLE_COPY(UiAction)

    friend class UiActionGroup;
#ifdef Q_OS_MAC
    friend void qt_mac_clear_status_text(UiAction *action);
#endif
};

QT_BEGIN_INCLUDE_NAMESPACE_UIHELPERS
#include <UiHelpers/uiactiongroup.h>
QT_END_INCLUDE_NAMESPACE_UIHELPERS

#endif // QT_NO_ACTION

QT_END_NAMESPACE_UIHELPERS

QT_END_HEADER

#endif // QACTION_H
