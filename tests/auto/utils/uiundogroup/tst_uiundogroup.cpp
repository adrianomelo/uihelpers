/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the UiHelpers playground module of the Qt Toolkit.
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
#include <QtTest/QtTest>
#include <UiHelpers/UiUndoGroup>
#include <UiHelpers/UiUndoStack>

QT_USE_NAMESPACE_UIHELPERS

// Temporarily disabling IRIX due to build issuues with GCC
#if !defined(__sgi) || defined(__sgi) && !defined(__GNUC__)

/******************************************************************************
** Commands
*/

class InsertCommand : public UiUndoCommand
{
public:
    InsertCommand(QString *str, int idx, const QString &text,
                    UiUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:
    QString *m_str;
    int m_idx;
    QString m_text;
};

class RemoveCommand : public UiUndoCommand
{
public:
    RemoveCommand(QString *str, int idx, int len, UiUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:
    QString *m_str;
    int m_idx;
    QString m_text;
};

class AppendCommand : public UiUndoCommand
{
public:
    AppendCommand(QString *str, const QString &text, UiUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();
    virtual int id() const;
    virtual bool mergeWith(const UiUndoCommand *other);

    bool merged;

private:
    QString *m_str;
    QString m_text;
};

InsertCommand::InsertCommand(QString *str, int idx, const QString &text,
                            UiUndoCommand *parent)
    : UiUndoCommand(parent)
{
    QVERIFY(str->length() >= idx);

    setText("insert");

    m_str = str;
    m_idx = idx;
    m_text = text;
}

void InsertCommand::redo()
{
    QVERIFY(m_str->length() >= m_idx);

    m_str->insert(m_idx, m_text);
}

void InsertCommand::undo()
{
    QCOMPARE(m_str->mid(m_idx, m_text.length()), m_text);

    m_str->remove(m_idx, m_text.length());
}

RemoveCommand::RemoveCommand(QString *str, int idx, int len, UiUndoCommand *parent)
    : UiUndoCommand(parent)
{
    QVERIFY(str->length() >= idx + len);

    setText("remove");

    m_str = str;
    m_idx = idx;
    m_text = m_str->mid(m_idx, len);
}

void RemoveCommand::redo()
{
    QCOMPARE(m_str->mid(m_idx, m_text.length()), m_text);

    m_str->remove(m_idx, m_text.length());
}

void RemoveCommand::undo()
{
    QVERIFY(m_str->length() >= m_idx);

    m_str->insert(m_idx, m_text);
}

AppendCommand::AppendCommand(QString *str, const QString &text, UiUndoCommand *parent)
    : UiUndoCommand(parent)
{
    setText("append");

    m_str = str;
    m_text = text;
    merged = false;
}

void AppendCommand::redo()
{
    m_str->append(m_text);
}

void AppendCommand::undo()
{
    QCOMPARE(m_str->mid(m_str->length() - m_text.length()), m_text);

    m_str->truncate(m_str->length() - m_text.length());
}

int AppendCommand::id() const
{
    return 1;
}

bool AppendCommand::mergeWith(const UiUndoCommand *other)
{
    if (other->id() != id())
        return false;
    m_text += static_cast<const AppendCommand*>(other)->m_text;
    merged = true;
    return true;
}

/******************************************************************************
** tst_UiUndoGroup
*/

class tst_UiUndoGroup : public QObject
{
    Q_OBJECT
public:
    tst_UiUndoGroup();

private slots:
    void setActive();
    void addRemoveStack();
    void deleteStack();
    void checkSignals();
    void addStackAndDie();
};

tst_UiUndoGroup::tst_UiUndoGroup()
{
}

void tst_UiUndoGroup::setActive()
{
    UiUndoGroup group;
    UiUndoStack stack1(&group), stack2(&group);

    QCOMPARE(group.activeStack(), (UiUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), false);

    UiUndoStack stack3;
    QCOMPARE(stack3.isActive(), true);

    group.addStack(&stack3);
    QCOMPARE(stack3.isActive(), false);

    stack1.setActive();
    QCOMPARE(group.activeStack(), &stack1);
    QCOMPARE(stack1.isActive(), true);
    QCOMPARE(stack2.isActive(), false);
    QCOMPARE(stack3.isActive(), false);

    group.setActiveStack(&stack2);
    QCOMPARE(group.activeStack(), &stack2);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);

    group.removeStack(&stack2);
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);

    group.removeStack(&stack2);
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);
}

void tst_UiUndoGroup::addRemoveStack()
{
    UiUndoGroup group;

    UiUndoStack stack1(&group);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << &stack1);

    UiUndoStack stack2;
    group.addStack(&stack2);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << &stack1 << &stack2);

    group.addStack(&stack1);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << &stack1 << &stack2);

    group.removeStack(&stack1);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << &stack2);

    group.removeStack(&stack1);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << &stack2);

    group.removeStack(&stack2);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>());
}

void tst_UiUndoGroup::deleteStack()
{
    UiUndoGroup group;

    UiUndoStack *stack1 = new UiUndoStack(&group);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << stack1);
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);

    stack1->setActive();
    QCOMPARE(group.activeStack(), stack1);

    UiUndoStack *stack2 = new UiUndoStack(&group);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << stack1 << stack2);
    QCOMPARE(group.activeStack(), stack1);

    UiUndoStack *stack3 = new UiUndoStack(&group);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << stack1 << stack2 << stack3);
    QCOMPARE(group.activeStack(), stack1);

    delete stack2;
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << stack1 << stack3);
    QCOMPARE(group.activeStack(), stack1);

    delete stack1;
    QCOMPARE(group.stacks(), QList<UiUndoStack*>() << stack3);
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);

    stack3->setActive(false);
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);

    stack3->setActive(true);
    QCOMPARE(group.activeStack(), stack3);

    group.removeStack(stack3);
    QCOMPARE(group.stacks(), QList<UiUndoStack*>());
    QCOMPARE(group.activeStack(), (UiUndoStack*)0);

    delete stack3;
}

#define CHECK_STATE(_activeStack, _clean, _canUndo, _undoText, _canRedo, _redoText, \
                    _cleanChanged, _indexChanged, _undoChanged, _redoChanged) \
    QCOMPARE(group.activeStack(), (UiUndoStack*)_activeStack); \
    QCOMPARE(group.isClean(), _clean); \
    QCOMPARE(group.canUndo(), _canUndo); \
    QCOMPARE(group.undoText(), QString(_undoText)); \
    QCOMPARE(group.canRedo(), _canRedo); \
    QCOMPARE(group.redoText(), QString(_redoText)); \
    if (_indexChanged) { \
        QCOMPARE(indexChangedSpy.count(), 1); \
        indexChangedSpy.clear(); \
    } else { \
        QCOMPARE(indexChangedSpy.count(), 0); \
    } \
    if (_cleanChanged) { \
        QCOMPARE(cleanChangedSpy.count(), 1); \
        QCOMPARE(cleanChangedSpy.at(0).at(0).toBool(), _clean); \
        cleanChangedSpy.clear(); \
    } else { \
        QCOMPARE(cleanChangedSpy.count(), 0); \
    } \
    if (_undoChanged) { \
        QCOMPARE(canUndoChangedSpy.count(), 1); \
        QCOMPARE(canUndoChangedSpy.at(0).at(0).toBool(), _canUndo); \
        QCOMPARE(undoTextChangedSpy.count(), 1); \
        QCOMPARE(undoTextChangedSpy.at(0).at(0).toString(), QString(_undoText)); \
        canUndoChangedSpy.clear(); \
        undoTextChangedSpy.clear(); \
    } else { \
        QCOMPARE(canUndoChangedSpy.count(), 0); \
        QCOMPARE(undoTextChangedSpy.count(), 0); \
    } \
    if (_redoChanged) { \
        QCOMPARE(canRedoChangedSpy.count(), 1); \
        QCOMPARE(canRedoChangedSpy.at(0).at(0).toBool(), _canRedo); \
        QCOMPARE(redoTextChangedSpy.count(), 1); \
        QCOMPARE(redoTextChangedSpy.at(0).at(0).toString(), QString(_redoText)); \
        canRedoChangedSpy.clear(); \
        redoTextChangedSpy.clear(); \
    } else { \
        QCOMPARE(canRedoChangedSpy.count(), 0); \
        QCOMPARE(redoTextChangedSpy.count(), 0); \
    }

void tst_UiUndoGroup::checkSignals()
{
    UiUndoGroup group;
    QSignalSpy indexChangedSpy(&group, SIGNAL(indexChanged(int)));
    QSignalSpy cleanChangedSpy(&group, SIGNAL(cleanChanged(bool)));
    QSignalSpy canUndoChangedSpy(&group, SIGNAL(canUndoChanged(bool)));
    QSignalSpy undoTextChangedSpy(&group, SIGNAL(undoTextChanged(QString)));
    QSignalSpy canRedoChangedSpy(&group, SIGNAL(canRedoChanged(bool)));
    QSignalSpy redoTextChangedSpy(&group, SIGNAL(redoTextChanged(QString)));

    QString str;

    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.undo();
    CHECK_STATE(0,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.redo();
    CHECK_STATE(0,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    UiUndoStack *stack1 = new UiUndoStack(&group);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack1->push(new AppendCommand(&str, "foo"));
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack1->setActive();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->push(new InsertCommand(&str, 0, "bar"));
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "insert",   // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                false,      // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.redo();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->setActive(false);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    UiUndoStack *stack2 = new UiUndoStack(&group);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack2->setActive();
    CHECK_STATE(stack2,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->setActive();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    delete stack1;
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged
}

void tst_UiUndoGroup::addStackAndDie()
{
    // Test that UiUndoStack doesn't keep a reference to UiUndoGroup after the
    // group is deleted.
    UiUndoStack *stack = new UiUndoStack;
    UiUndoGroup *group = new UiUndoGroup;
    group->addStack(stack);
    delete group;
    stack->setActive(true);
    delete stack;
}

#else
class tst_UiUndoGroup : public QObject
{
    Q_OBJECT
public:
    tst_UiUndoGroup() {}

private slots:
    void setActive() { QSKIP( "Not tested on irix-g++"); }
    void addRemoveStack() { QSKIP( "Not tested on irix-g++"); }
    void deleteStack() { QSKIP( "Not tested on irix-g++"); }
    void checkSignals() { QSKIP( "Not tested on irix-g++"); }
    void addStackAndDie() { QSKIP( "Not tested on irix-g++"); }
};
#endif

QTEST_MAIN(tst_UiUndoGroup)

#include "tst_uiundogroup.moc"

