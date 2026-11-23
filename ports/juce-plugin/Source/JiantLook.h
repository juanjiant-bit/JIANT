// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT's look for the plugin's editor, as the site's FM-1 panel (web/emu/devices.html) and the firmware's JIANT skin
// draw it: a near-black body with a faint dot grid, thin lines, values as heat (indigo violet pink red orange amber
// cream), what is active in orange, what is selected in violet; DotGothic16 for names, IBM Plex Mono for values (both
// OFL, embedded: ports/juce-plugin/fonts).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace jlook {

const juce::Colour bg(0xff050409), body(0xff0b0a11), panel(0xff100f17), surf(0xff15141d), line(0x29efe6c8),
    lineHi(0x55efe6c8), ink(0xffddd6ea), dim(0xffa39cba), faint(0xff655d78), org(0xffff7a1a), vio(0xffb870ff),
    cyan(0xff3fe0e6), grn(0xff8be06a), red(0xffff4b4b), cream(0xfffff3ce), yellow(0xffffe14d);

// the heat gradient, 0..1 (the site's --heat): the six tracks take its stops
inline juce::Colour heat(float t)
{
    static const juce::uint32 stops[6] = {0xff9d97ff, 0xffd39bff, 0xffff9fd2, 0xffff9a8a, 0xffffc07a, 0xfffff3a8};
    t = juce::jlimit(0.0f, 1.0f, t) * 5.0f;
    int i = juce::jmin(4, int(t));
    return juce::Colour(stops[i]).interpolatedWith(juce::Colour(stops[i + 1]), t - float(i));
}
inline juce::Colour track(int k) { return heat(float(k) / 5.0f); }

juce::Typeface::Ptr dotTypeface();
juce::Typeface::Ptr monoTypeface();
inline juce::FontOptions dot(float h) { return juce::FontOptions(dotTypeface()).withHeight(h); }
inline juce::FontOptions mono(float h) { return juce::FontOptions(monoTypeface()).withHeight(h); }

// the body: its colour, the dot grid, the heat line along the top
void paintBody(juce::Graphics& g, juce::Rectangle<float> r, float grid = 14.0f);
// HUD corner brackets around r (the site's .stage corners)
void brackets(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float len = 12.0f, float w = 1.5f);
// a ring with its value as an arc (from -135 degrees), the arc in heat or c
void ring(juce::Graphics& g, juce::Point<float> c, float radius, float value01, juce::Colour arc, bool bipolar = false,
          float thick = 4.0f);
// a button: dark, a thin edge; lit: orange edge and a warm fill
void button(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool held = false,
            juce::Colour on = org, float font = 13.0f);

}  // namespace jlook
