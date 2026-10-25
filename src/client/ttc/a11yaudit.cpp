// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/a11yaudit.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QAccessible>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QWidget>

namespace ttc {

namespace {

QString describe(QWidget* w)
{
    QString name = w->objectName();
    return QStringLiteral("%1%2").arg(QString::fromLatin1(w->metaObject()->className()),
                                      name.isEmpty() ? QString() : QStringLiteral(" '%1'").arg(name));
}

bool isAuditable(QWidget* w)
{
    // Only what is on screen now: a closed combo-box popup or a hidden
    // settings page is not something the user can reach.
    if (!w->isVisible() || !w->isEnabled())
        return false;
    if (!(w->focusPolicy() & Qt::TabFocus))
        return false;
    // Scroll bars and the viewports of scroll areas are not separate stops
    // for screen-reader users.
    if (qobject_cast<QScrollBar*>(w))
        return false;
    if (auto* area = qobject_cast<QAbstractScrollArea*>(w->parentWidget()))
        if (area->viewport() == w)
            return false;
    return true;
}

QString cleanLabel(QString text)
{
    text.remove(QLatin1Char('&'));
    text = text.trimmed();
    if (text.endsWith(QLatin1Char(':')))
        text.chop(1);
    return text.trimmed();
}

void nameWidget(QWidget* field, const QString& name)
{
    if (field->accessibleName().isEmpty())
        field->setAccessibleName(name);
    if (auto* spin = qobject_cast<QAbstractSpinBox*>(field))
        for (QLineEdit* inner : spin->findChildren<QLineEdit*>())
            if (inner->accessibleName().isEmpty())
                inner->setAccessibleName(name);
}

} // namespace

void nameFormFields(QWidget* root)
{
    if (!root)
        return;
    const auto forms = root->findChildren<QFormLayout*>();
    for (QFormLayout* form : forms)
        for (int row = 0; row < form->rowCount(); ++row)
        {
            QLayoutItem* labelItem = form->itemAt(row, QFormLayout::LabelRole);
            QLayoutItem* fieldItem = form->itemAt(row, QFormLayout::FieldRole);
            auto* label = labelItem ? qobject_cast<QLabel*>(labelItem->widget()) : nullptr;
            QWidget* field = fieldItem ? fieldItem->widget() : nullptr;
            if (label && field)
                nameWidget(field, cleanLabel(label->text()));
        }
}

QStringList auditAccessibility(QWidget* root)
{
    QStringList problems;
    if (!root)
        return problems;
    QList<QWidget*> widgets = root->findChildren<QWidget*>();
    widgets.prepend(root);
    for (QWidget* w : std::as_const(widgets))
    {
        if (!isAuditable(w))
            continue;
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(w);
        if (!iface)
        {
            problems << QStringLiteral("%1: no accessibility interface").arg(describe(w));
            continue;
        }
        const QString name = iface->text(QAccessible::Name).trimmed();
        if (name.isEmpty())
            problems << QStringLiteral("%1 (%2): focusable but has no accessible name")
                            .arg(describe(w), iface->text(QAccessible::Value).left(40));
        if (auto* b = qobject_cast<QAbstractButton*>(w))
            if (b->text().trimmed().isEmpty() && b->accessibleName().trimmed().isEmpty())
                problems << QStringLiteral("%1: button without text").arg(describe(w));
    }
    return problems;
}

} // namespace ttc
