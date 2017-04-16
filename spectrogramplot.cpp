/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
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

#include "spectrogramplot.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>
#include <QRect>
#include <liquid/liquid.h>

#include <cstdlib>
#include "util.h"


SpectrogramPlot::SpectrogramPlot(std::shared_ptr<SampleSource<std::complex<float>>> src) : Plot(src), inputSource(src), fftSize(512), tuner(fftSize, this)
{
    zoomLevel = 1;
    powerMax = 0.0f;
    powerMin = -50.0f;
    timeResolution = 0.0;
    sampleRate = 0;
    beta = 4.0;
    frequencyScaleEnabled = false;
    setFFTSize(fftSize);

    for (int i = 0; i < 256; i++) {
        float p = (float)i / 256;
        colormap[i] = QColor::fromHsvF(p * 0.83f, 1.0, 1.0 - p).rgba();
    }

    tunerTransform = std::make_shared<TunerTransform>(src);
    connect(&tuner, &Tuner::tunerMoved, this, &SpectrogramPlot::tunerMoved);
}

void SpectrogramPlot::invalidateEvent()
{
    pixmapCache.clear();
    fftCache.clear();
    emit repaint();
}

void SpectrogramPlot::paintFront(QPainter &painter, QRect &rect, range_t<off_t> sampleRange)
{
    if (tunerEnabled())
        tuner.paintFront(painter, rect, sampleRange);

    if (frequencyScaleEnabled)
        paintFrequencyScale(painter, rect);
}

void SpectrogramPlot::paintFrequencyScale(QPainter &painter, QRect &rect)
{
    // At which pixel is F_+sampleRate/2
    int y = rect.y();

    int plotHeight = rect.height();

    double bwPerPixel = (double)sampleRate / plotHeight;
    int tickHeight = 50;

    int bwPerTick = 10 * pow(10, floor(log(bwPerPixel * tickHeight) / log(10)));

    if (bwPerTick < 1) {
        return;
    }

    painter.save();

    QPen pen(Qt::white, 1, Qt::SolidLine);
    painter.setPen(pen);
    QFontMetrics fm(painter.font());


    int tick = 0;

    while (tick <= sampleRate / 2) {

        int tickpy = plotHeight / 2 - tick / bwPerPixel + y;
        int tickny = plotHeight / 2 + tick / bwPerPixel + y;

        painter.drawLine(0, tickny, 30, tickny);
        painter.drawLine(0, tickpy, 30, tickpy);

        if (tick != 0) {
            char buf[128];

            if (bwPerTick % 1000000 == 0) {
                snprintf(buf, sizeof(buf), "-%d MHz", (int)tick / 1000000);
            } else if(bwPerTick % 1000 == 0) {
                snprintf(buf, sizeof(buf), "-%d kHz", tick / 1000);
            } else {
                snprintf(buf, sizeof(buf), "-%d Hz", tick);
            }

            painter.drawText(5, tickny - 5, buf);

            buf[0] = ' ';
            painter.drawText(5, tickpy + 15, buf);
        }

        tick += bwPerTick;
    }

    // Draw small ticks
    bwPerTick /= 10;

    if (bwPerTick >= 1 ) {
        tick = 0;
        while (tick <= sampleRate / 2) {

            int tickpy = plotHeight / 2 - tick / bwPerPixel + y;
            int tickny = plotHeight / 2 + tick / bwPerPixel + y;

            painter.drawLine(0, tickny, 3, tickny);
            painter.drawLine(0, tickpy, 3, tickpy);

            tick += bwPerTick;
        }
    }
    painter.restore();
}

void SpectrogramPlot::paintMid(QPainter &painter, QRect &rect, range_t<off_t> sampleRange)
{
    if (!inputSource || inputSource->count() == 0)
        return;

    off_t sampleOffset = sampleRange.minimum % (getStride() * linesPerTile());
    off_t tileID = sampleRange.minimum - sampleOffset;
    int xoffset = sampleOffset / getStride();

    // Paint first (possibly partial) tile
    painter.drawPixmap(QRect(rect.left(), rect.y(), linesPerTile() - xoffset, fftSize), *getPixmapTile(tileID), QRect(xoffset, 0, linesPerTile() - xoffset, fftSize));
    tileID += getStride() * linesPerTile();

    // Paint remaining tiles
    for (int x = linesPerTile() - xoffset; x < rect.right(); x += linesPerTile()) {
        // TODO: don't draw past rect.right()
        // TODO: handle partial final tile
        painter.drawPixmap(QRect(x, rect.y(), linesPerTile(), fftSize), *getPixmapTile(tileID));
        tileID += getStride() * linesPerTile();
    }
}

QPixmap* SpectrogramPlot::getPixmapTile(off_t tile)
{
    QPixmap *obj = pixmapCache.object(TileCacheKey(fftSize, zoomLevel, tile));
    if (obj != 0)
        return obj;

    float *fftTile = getFFTTile(tile);
    obj = new QPixmap(linesPerTile(), fftSize);
    QImage image(linesPerTile(), fftSize, QImage::Format_RGB32);
    float powerRange = -1.0f / std::abs(int(powerMin - powerMax));
    for (int y = 0; y < fftSize; y++) {
        auto scanLine = (QRgb*)image.scanLine(fftSize - y - 1);
        for (int x = 0; x < linesPerTile(); x++) {
            float *fftLine = &fftTile[x * fftSize];
            float normPower = (fftLine[y] - powerMax) * powerRange;
            normPower = clamp(normPower, 0.0f, 1.0f);

            scanLine[x] = colormap[(uint8_t)(normPower * (256 - 1))];
        }
    }
    obj->convertFromImage(image);
    pixmapCache.insert(TileCacheKey(fftSize, zoomLevel, tile), obj);
    return obj;
}

float* SpectrogramPlot::getFFTTile(off_t tile)
{
    std::array<float, tileSize>* obj = fftCache.object(TileCacheKey(fftSize, zoomLevel, tile));
    if (obj != nullptr)
        return obj->data();

    std::array<float, tileSize>* destStorage = new std::array<float, tileSize>;
    float *ptr = destStorage->data();
    off_t sample = tile;
    while ((ptr - destStorage->data()) < tileSize) {
        getLine(ptr, sample);
        sample += getStride();
        ptr += fftSize;
    }
    fftCache.insert(TileCacheKey(fftSize, zoomLevel, tile), destStorage);
    return destStorage->data();
}

void SpectrogramPlot::getLine(float *dest, off_t sample)
{
    if (inputSource && fft) {
        auto buffer = inputSource->getSamples(sample, fftSize);
        if (buffer == nullptr)
            return;

        for (int i = 0; i < fftSize; i++) {
            buffer[i] *= window[i];
        }

        fft->process(buffer.get(), buffer.get());
        const float invFFTSize = 1.0f / fftSize;
        const float logMultiplier = 10.0f / log2f(10.0f);
        for (int i = 0; i < fftSize; i++) {
            // Start from the middle of the FFTW array and wrap
            // to rearrange the data
            int k = i ^ (fftSize >> 1);
            auto s = buffer[k] * invFFTSize;
            float power = s.real() * s.real() + s.imag() * s.imag();
            float logPower = log2f(power) * logMultiplier;
            *dest = logPower;
            dest++;
        }
    }
}

int SpectrogramPlot::getStride()
{
    return fftSize / zoomLevel;
}

float SpectrogramPlot::getTunerPhaseInc()
{
    auto freq = 0.5f - tuner.centre() / (float)fftSize;
    return freq * Tau;
}

std::vector<float> SpectrogramPlot::getTunerTaps()
{
    float cutoff = tuner.deviation() / (float)fftSize;
    float gain = pow(10.0f, powerMax / -10.0f);
    auto atten = 60.0f;
    auto len = estimate_req_filter_len(0.05f, atten);
    auto taps = std::vector<float>(len);
    liquid_firdes_kaiser(len, cutoff, atten, 0.0f, taps.data());
    std::transform(taps.begin(), taps.end(), taps.begin(),
                   std::bind1st(std::multiplies<float>(), gain));
    return taps;
}

int SpectrogramPlot::linesPerTile()
{
    return tileSize / fftSize;
}

bool SpectrogramPlot::mouseEvent(QEvent::Type type, QMouseEvent event)
{
    if (tunerEnabled())
        return tuner.mouseEvent(type, event);

    return false;
}

std::shared_ptr<AbstractSampleSource> SpectrogramPlot::output()
{
    return tunerTransform;
}

void SpectrogramPlot::setFFTSize(int size)
{
    float sizeScale = float(size) / float(fftSize);
    
    fftSize = size;
    fft.reset(new FFT(fftSize));

    window.reset(new float[fftSize]);
    calcFFTWindow(window.get(), fftSize, timeResolution, beta);

    setHeight(fftSize);
    auto dev = tuner.deviation();
    auto centre = tuner.centre();
    tuner.setHeight(fftSize);
    tuner.setDeviation( dev * sizeScale );
    tuner.setCentre( centre * sizeScale );
}

void SpectrogramPlot::setPowerMax(int power)
{
    powerMax = power;
    pixmapCache.clear();
    tunerMoved();
}

void SpectrogramPlot::setPowerMin(int power)
{
    powerMin = power;
    pixmapCache.clear();
}

void SpectrogramPlot::setTimeResolution(int resolution)
{
    timeResolution = resolution;
    setFFTSize(fftSize);
    invalidateEvent();
}

void SpectrogramPlot::setBeta(float beta)
{
    this->beta = beta;
    setFFTSize(fftSize);
    invalidateEvent();
}
void SpectrogramPlot::setZoomLevel(int zoom)
{
    zoomLevel = zoom;
}

void SpectrogramPlot::setSampleRate(off_t rate)
{
    sampleRate = rate;
}

void SpectrogramPlot::enableScales(bool enabled)
{
   frequencyScaleEnabled = enabled;
}

bool SpectrogramPlot::tunerEnabled()
{
    return (tunerTransform->subscriberCount() > 0);
}

void SpectrogramPlot::tunerMoved()
{
    tunerTransform->setFrequency(getTunerPhaseInc());
    tunerTransform->setTaps(getTunerTaps());
    tunerTransform->setRelativeBandwith(tuner.deviation() * 2.0 / getStride());

    // TODO: for invalidating traceplot cache, this shouldn't really go here
    QPixmapCache::clear();

    emit repaint();
}

uint qHash(const TileCacheKey &key, uint seed)
{
    return key.fftSize ^ key.zoomLevel ^ key.sample ^ seed;
}
