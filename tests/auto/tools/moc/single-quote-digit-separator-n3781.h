/****************************************************************************
**
** Copyright (C) 2013 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Stephen Kelly <stephen.kelly@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SINGLE_QUOTE_DIGIT_SEPARATOR_N3781_H
#define SINGLE_QUOTE_DIGIT_SEPARATOR_N3781_H

#include <QObject>

class KDAB : public QObject
{
    Q_OBJECT
public:
    // C++1y allows use of single quote as a digit separator, useful for large
    // numbers. http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3781.pdf
    // Ensure that moc does not get confused with this.
    enum Salaries {
        Steve
#ifdef Q_MOC_RUN
        = 1'234'567
#endif
    };
    Q_ENUMS(Salaries)
};
#endif // SINGLE_QUOTE_DIGIT_SEPARATOR_N3781_H
