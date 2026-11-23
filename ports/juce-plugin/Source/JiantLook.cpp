// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "JiantLook.h"
#include "BinaryData.h"

namespace jlook {

juce::Typeface::Ptr dotTypeface()
{
    static juce::Typeface::Ptr t =
        juce::Typeface::createSystemTypefaceFor(BinaryData::DotGothic16ascii_ttf, BinaryData::DotGothic16ascii_ttfSize);
    return t;
}
juce::Typeface::Ptr monoTypeface()
{
    static juce::Typeface::Ptr t =
        juce::Typeface::createSystemTypefaceFor(BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);
    return t;
}

void paintBody(juce::Graphics& g, juce::Rectangle<float> r, float grid)
{
    g.setColour(bg);
    g.fillRect(r);
    g.setColour(juce::Colour(0x12efe6c8));
    for (float y = r.getY() + grid * 0.5f; y < r.getBottom(); y += grid)
        for (float x = r.getX() + grid * 0.5f; x < r.getRight(); x += grid)
            g.fillRect(x, y, 1.0f, 1.0f);
    juce::ColourGradient heatLine(heat(0), r.getX(), 0, heat(1), r.getRight(), 0, false);
    for (int i = 1; i < 5; ++i)
        heatLine.addColour(i / 5.0, heat(i / 5.0f));
    g.setGradientFill(heatLine);
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 3.0f);
}

void brackets(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float len, float w)
{
    g.setColour(c);
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom();
    g.fillRect(x0, y0, len, w); g.fillRect(x0, y0, w, len);
    g.fillRect(x1 - len, y0, len, w); g.fillRect(x1 - w, y0, w, len);
    g.fillRect(x0, y1 - w, len, w); g.fillRect(x0, y1 - len, w, len);
    g.fillRect(x1 - len, y1 - w, len, w); g.fillRect(x1 - w, y1 - len, w, len);
}

void ring(juce::Graphics& g, juce::Point<float> c, float radius, float v, juce::Colour arc, bool bipolar, float thick)
{
    const float a0 = -0.75f * juce::MathConstants<float>::pi, a1 = 0.75f * juce::MathConstants<float>::pi;
    v = juce::jlimit(0.0f, 1.0f, v);
    juce::Path track;
    track.addCentredArc(c.x, c.y, radius, radius, 0, a0, a1, true);
    g.setColour(line.withAlpha(0.35f));
    g.strokePath(track, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float from = bipolar ? 0.0f : a0, to = a0 + (a1 - a0) * v;
    if (std::abs(to - from) > 1e-3f) {
        juce::Path val;
        val.addCentredArc(c.x, c.y, radius, radius, 0, std::min(from, to), std::max(from, to), true);
        g.setColour(arc);
        g.strokePath(val, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // the value's point, cream on the arc's end
    const float px = c.x + radius * std::sin(to), py = c.y - radius * std::cos(to);
    g.setColour(cream);
    g.fillEllipse(px - thick * 0.6f, py - thick * 0.6f, thick * 1.2f, thick * 1.2f);
    g.setColour(panel);
    g.fillEllipse(c.x - radius + thick * 1.6f, c.y - radius + thick * 1.6f, 2 * (radius - thick * 1.6f),
                  2 * (radius - thick * 1.6f));
    g.setColour(line);
    g.drawEllipse(c.x - radius + thick * 1.6f, c.y - radius + thick * 1.6f, 2 * (radius - thick * 1.6f),
                  2 * (radius - thick * 1.6f), 1.0f);
}

void button(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool held, juce::Colour on,
            float font)
{
    g.setColour(lit ? on.interpolatedWith(bg, 0.82f) : panel);
    g.fillRect(r);
    g.setColour(held ? cream : lit ? on : on.withAlpha(0.45f));
    g.drawRect(r, held ? 2.0f : 1.25f);
    g.setColour(lit ? cream : ink);
    g.setFont(dot(font));
    g.drawFittedText(text, r.toNearestInt().reduced(2, 0), juce::Justification::centred, 1, 0.8f);
}

}  // namespace jlook
