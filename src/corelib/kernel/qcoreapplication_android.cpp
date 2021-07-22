/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcoreapplication.h"
#include "qcoreapplication_p.h"

#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qfuture.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#include <QtCore/qlist.h>
#include <QtCore/qmutex.h>
#include <QtCore/qpromise.h>
#include <QtCore/qscopedpointer.h>

QT_BEGIN_NAMESPACE

static const char qtNativeClassName[] = "org/qtproject/qt/android/QtNative";

QApplicationPermission::PermissionResult resultFromAndroid(jint value)
{
    return value == 0 ? QApplicationPermission::Authorized : QApplicationPermission::Denied;
}

using PendingPermissionRequestsHash
            = QHash<int, QSharedPointer<QPromise<QApplicationPermission::PermissionResult>>>;
Q_GLOBAL_STATIC(PendingPermissionRequestsHash, g_pendingPermissionRequests);
static QBasicMutex g_pendingPermissionRequestsMutex;

static int nextRequestCode()
{
    static QBasicAtomicInt counter = Q_BASIC_ATOMIC_INITIALIZER(0);
    return counter.fetchAndAddRelaxed(1);
}

static QStringList nativeStringsFromPermission(QApplicationPermission::PermissionType permission)
{
    static const auto precisePerm = QStringLiteral("android.permission.ACCESS_FINE_LOCATION");
    static const auto coarsePerm = QStringLiteral("android.permission.ACCESS_COARSE_LOCATION");
    static const auto backgroundPerm =
            QStringLiteral("android.permission.ACCESS_BACKGROUND_LOCATION");

    switch (permission) {
    case QApplicationPermission::Location:
        return {coarsePerm};
    case QApplicationPermission::PreciseLocation:
        return {precisePerm};
    case QApplicationPermission::BackgroundLocation:
        // Keep the background permission first to be able to use .first()
        // in checkPermission because it takes single permission
        if (QtAndroidPrivate::androidSdkVersion() >= 29)
            return {backgroundPerm, coarsePerm};
        return {coarsePerm};
    case QApplicationPermission::PreciseBackgroundLocation:
        // Keep the background permission first to be able to use .first()
        // in checkPermission because it takes single permission
        if (QtAndroidPrivate::androidSdkVersion() >= 29)
            return {backgroundPerm, precisePerm};
        return {precisePerm};
    case QApplicationPermission::Camera:
        return {QStringLiteral("android.permission.CAMERA")};
    case QApplicationPermission::Microphone:
        return {QStringLiteral("android.permission.RECORD_AUDIO")};
    case QApplicationPermission::Bluetooth:
        return { QStringLiteral("android.permission.BLUETOOTH") };
    case QApplicationPermission::BodySensors:
        return {QStringLiteral("android.permission.BODY_SENSORS")};
    case QApplicationPermission::PhysicalActivity:
        return {QStringLiteral("android.permission.ACTIVITY_RECOGNITION")};
    case QApplicationPermission::Contacts:
        return {QStringLiteral("android.permission.READ_CONTACTS"),
                QStringLiteral("android.permission.WRITE_CONTACTS")};
    case QApplicationPermission::Storage:
        return {QStringLiteral("android.permission.READ_EXTERNAL_STORAGE"),
                QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE")};
    case QApplicationPermission::Calendar:
        return {QStringLiteral("android.permission.READ_CALENDAR"),
                QStringLiteral("android.permission.WRITE_CALENDAR")};
    }

    return {};
}

/*!
    \internal

    This function is called when the result of the permission request is available.
    Once a permission is requested, the result is braodcast by the OS and listened
    to by QtActivity which passes it to C++ through a native JNI method call.
 */
static void sendRequestPermissionsResult(JNIEnv *env, jobject *obj, jint requestCode,
                                         jobjectArray permissions, jintArray grantResults)
{
    Q_UNUSED(obj);

    QMutexLocker locker(&g_pendingPermissionRequestsMutex);
    auto it = g_pendingPermissionRequests->constFind(requestCode);
    if (it == g_pendingPermissionRequests->constEnd()) {
        qWarning() << "Found no valid pending permission request for request code" << requestCode;
        return;
    }

    auto request = *it;
    g_pendingPermissionRequests->erase(it);
    locker.unlock();

    const int size = env->GetArrayLength(permissions);
    std::unique_ptr<jint[]> results(new jint[size]);
    env->GetIntArrayRegion(grantResults, 0, size, results.get());

    for (int i = 0 ; i < size; ++i) {
        QApplicationPermission::PermissionResult result = resultFromAndroid(results[i]);
        request->addResult(result, i);
    }

    request->finish();
}

QFuture<QApplicationPermission::PermissionResult>
requestPermissionsInternal(const QStringList &permissions)
{
    QSharedPointer<QPromise<QApplicationPermission::PermissionResult>> promise;
    promise.reset(new QPromise<QApplicationPermission::PermissionResult>());
    QFuture<QApplicationPermission::PermissionResult> future = promise->future();
    promise->start();

    // No mechanism to request permission for SDK version below 23, because
    // permissions defined in the manifest are granted at install time.
    if (QtAndroidPrivate::androidSdkVersion() < 23) {
        for (int i = 0; i < permissions.size(); ++i)
            promise->addResult(QCoreApplication::checkPermission(permissions.at(i)).result(), i);
        promise->finish();
        return future;
    }

    const int requestCode = nextRequestCode();
    QMutexLocker locker(&g_pendingPermissionRequestsMutex);
    g_pendingPermissionRequests->insert(requestCode, promise);

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([permissions, requestCode] {
        QJniEnvironment env;
        jclass clazz = env.findClass("java/lang/String");
        auto array = env->NewObjectArray(permissions.size(), clazz, nullptr);
        int index = 0;

        for (auto &perm : permissions)
            env->SetObjectArrayElement(array, index++, QJniObject::fromString(perm).object());

        QJniObject(QtAndroidPrivate::activity()).callMethod<void>("requestPermissions",
                                                                  "([Ljava/lang/String;I)V",
                                                                  array,
                                                                  requestCode);
        env->DeleteLocalRef(array);
    });

    return future;
}

QFuture<QApplicationPermission::PermissionResult>
QCoreApplicationPrivate::requestPermission(const QString &permission)
{
    // avoid the uneccessary call and response to an empty permission string
    if (permission.size() > 0)
        return requestPermissionsInternal({permission});

    QPromise<QApplicationPermission::PermissionResult> promise;
    QFuture<QApplicationPermission::PermissionResult> future = promise.future();
    promise.start();
    promise.addResult(QApplicationPermission::Denied);
    promise.finish();
    return future;
}

static bool isBackgroundLocationApi29(QApplicationPermission::PermissionType permission)
{
    return QNativeInterface::QAndroidApplication::sdkVersion() >= 29
            && (permission == QApplicationPermission::BackgroundLocation
                || permission == QApplicationPermission::PreciseBackgroundLocation);
}

QFuture<QApplicationPermission::PermissionResult>
QCoreApplicationPrivate::requestPermission(QApplicationPermission::PermissionType permission)
{
    QSharedPointer<QPromise<QApplicationPermission::PermissionResult>> promise;
    promise.reset(new QPromise<QApplicationPermission::PermissionResult>());
    QFuture<QApplicationPermission::PermissionResult> future = promise->future();
    promise->start();
    const auto nativePermissions = nativeStringsFromPermission(permission);

    if (nativePermissions.size() > 0) {
        requestPermissionsInternal(nativePermissions).then(
                    [promise, permission](QFuture<QApplicationPermission::PermissionResult> future) {
            auto AuthorizedCount = future.results().count(QApplicationPermission::Authorized);
            if (AuthorizedCount > 0) {
                if (isBackgroundLocationApi29(permission))
                    promise->addResult(future.resultAt(0), 0);
                else
                    promise->addResult(QApplicationPermission::Authorized, 0);
            } else {
                promise->addResult(QApplicationPermission::Denied, 0);
            }
            promise->finish();
        });

        return future;
    }

    promise->addResult(QApplicationPermission::Denied);
    promise->finish();
    return future;
}

QFuture<QApplicationPermission::PermissionResult>
QCoreApplicationPrivate::checkPermission(const QString &permission)
{
    QPromise<QApplicationPermission::PermissionResult> promise;
    QFuture<QApplicationPermission::PermissionResult> future = promise.future();
    promise.start();

    if (permission.size() > 0) {
        auto res = QJniObject::callStaticMethod<jint>(qtNativeClassName,
                                                      "checkSelfPermission",
                                                      "(Ljava/lang/String;)I",
                                                      QJniObject::fromString(permission).object());
        promise.addResult(resultFromAndroid(res));
    } else {
        promise.addResult(QApplicationPermission::Denied);
    }

    promise.finish();
    return future;
}

QFuture<QApplicationPermission::PermissionResult>
QCoreApplicationPrivate::checkPermission(QApplicationPermission::PermissionType permission)
{
    const auto nativePermissions = nativeStringsFromPermission(permission);

    if (nativePermissions.size() > 0)
        return checkPermission(nativePermissions.first());

    QPromise<QApplicationPermission::PermissionResult> promise;
    QFuture<QApplicationPermission::PermissionResult> future = promise.future();
    promise.start();
    promise.addResult(QApplicationPermission::Denied);
    promise.finish();
    return future;
}

bool QtAndroidPrivate::registerPermissionNatives()
{
    if (QtAndroidPrivate::androidSdkVersion() < 23)
        return true;

    JNINativeMethod methods[] = {
        {"sendRequestPermissionsResult", "(I[Ljava/lang/String;[I)V",
         reinterpret_cast<void *>(sendRequestPermissionsResult)
        }};

    QJniEnvironment env;
    return env.registerNativeMethods(qtNativeClassName, methods, 1);
}

QT_END_NAMESPACE
