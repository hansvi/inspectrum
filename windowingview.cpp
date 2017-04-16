/*
 *  Copyright (C) 2015-2016, Mike Walters <mike@flomp.net>
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

#include "windowingview.h"
#include <QPainter>
#include <QPalette>
#include "fft.h"

WindowingView::WindowingView(QWidget *parent) : QWidget(parent)
{
    beta = 0;
    timeResolution = 0;

    QPalette palette = this->palette();
    palette.setColor(QPalette::Background, Qt::black);
    setAutoFillBackground(true);
    setPalette(palette);
}

int WindowingView::heightForWidth(int w) const
{
    return w * 3 / 2;
}

void WindowingView::setTimeResolution(int resolution)
{
    timeResolution = resolution;
    update();
}

void WindowingView::setBeta(int beta)
{
    this->beta = beta;
    update();
}

void WindowingView::paintEvent(QPaintEvent *event)
{
    int n = width();
    int maxy = height() - 1;
    float window[n];
    calcFFTWindow(window, n, timeResolution, beta);
    QPainter p(this);
    QRect rect(0,0, width(), height());
    p.fillRect(rect, Qt::black);
    p.setPen(Qt::white);
    for(int i = 1; i < n; i++) {
        p.drawLine(i - 1, maxy * (1.0 - window[i - 1]), i, maxy*(1.0 - window[i]));
    }
    
}
QSize WindowingView::sizeHint() const
{
    return QSize(120,80);
}
