/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "nodeinstancesignalspy.h"
#include "objectnodeinstance.h"

#include <QMetaProperty>
#include <QMetaObject>
#include <QDebug>
#include <QSharedPointer>
#include <private/qdeclarativemetatype_p.h>

namespace QmlDesigner {
namespace Internal {

NodeInstanceSignalSpy::NodeInstanceSignalSpy() :
    QObject()
{
    blockSignals(true);
}

void NodeInstanceSignalSpy::setObjectNodeInstance(const ObjectNodeInstance::Pointer &nodeInstance)
{
    methodeOffset = QObject::staticMetaObject.methodCount() + 1;
    registerObject(nodeInstance->object());
    m_objectNodeInstance = nodeInstance;

}

void NodeInstanceSignalSpy::registerObject(QObject *spiedObject, const PropertyName &prefix)
{
    if (m_registeredObjectList.contains(spiedObject)) // prevent cycles
        return;

    m_registeredObjectList.append(spiedObject);
    for (int index = QObject::staticMetaObject.propertyOffset();
         index < spiedObject->metaObject()->propertyCount();
         index++) {
             QMetaProperty metaProperty = spiedObject->metaObject()->property(index);

             // handle dot properties and connect the signals to the object
             if (metaProperty.isReadable()
                 && !metaProperty.isWritable()
                 && QDeclarativeMetaType::isQObject(metaProperty.userType())) {
                  QObject *propertyObject = QDeclarativeMetaType::toQObject(metaProperty.read(spiedObject));
                  if (propertyObject)
                      registerObject(propertyObject, prefix + metaProperty.name() + '.');
             } else if (metaProperty.hasNotifySignal()) {
                 QMetaMethod metaMethod = metaProperty.notifySignal();
                 bool isConnecting = QMetaObject::connect(spiedObject, metaMethod.methodIndex(), this, methodeOffset, Qt::DirectConnection);
                 Q_ASSERT(isConnecting);
                 Q_UNUSED(isConnecting);
                 m_indexPropertyHash.insert(methodeOffset, prefix + metaProperty.name());
                 methodeOffset++;
             }

             // search recursive in objects
             if (metaProperty.isReadable()
                 && metaProperty.isWritable()
                 && QDeclarativeMetaType::isQObject(metaProperty.userType())) {
                 QObject *propertyObject = QDeclarativeMetaType::toQObject(metaProperty.read(spiedObject));
                 if (propertyObject)
                     registerObject(propertyObject, prefix + metaProperty.name() + '/');
             }

             // search recursive in objects list
             if (metaProperty.isReadable()
                 && QDeclarativeMetaType::isList(metaProperty.userType())) {
                 QDeclarativeListReference list(spiedObject, metaProperty.name());
                 if (list.canCount() && list.canAt()) {
                     for (int i = 0; i < list.count(); i++) {
                         QObject *propertyObject = list.at(i);
                         if (propertyObject)
                             registerObject(propertyObject, prefix + metaProperty.name() + '/');
                     }
                 }
             }
         }
}

int NodeInstanceSignalSpy::qt_metacall(QMetaObject::Call call, int methodId, void **a)
{
    if (call == QMetaObject::InvokeMetaMethod && methodId > QObject::staticMetaObject.methodCount()) {
        ObjectNodeInstance::Pointer nodeInstance = m_objectNodeInstance.toStrongRef();

        if (nodeInstance && nodeInstance->nodeInstanceServer() && nodeInstance->isValid()) {
            nodeInstance->nodeInstanceServer()->notifyPropertyChange(nodeInstance->instanceId(), m_indexPropertyHash.value(methodId));
        }

    }

    return QObject::qt_metacall(call, methodId, a);
}

} // namespace Internal
} // namespace QmlDesigner
