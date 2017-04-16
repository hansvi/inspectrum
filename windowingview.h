/*
 *  Copyright (C) 2017, Hans Van Ingelgom <hans@hansvi.be>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QWidget>

class WindowingView : public QWidget
{
    Q_OBJECT

public:
    WindowingView(QWidget *parent = 0);
    int heightForWidth(int w) const;
    QSize sizeHint() const;

public slots:
    void setTimeResolution(int resolution);
    void setBeta(int beta);

protected:
    void paintEvent(QPaintEvent *event);

private:
    int beta;
    int timeResolution;
};
