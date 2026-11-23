// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "JiantEditor.h"
#include "JiantLook.h"
#include "jiant_core.h"
#include <cmath>

namespace {
// the browser emulator's WIDE layout (web/emu/index.html LAYOUTS.wide), in its 1000 x 624 units
const float kScreen[3] = {292, 44, 256};
const float kKnobs[8][3] = {{105, 100, 24}, {215, 100, 24}, {105, 205, 24}, {215, 205, 24},
                            {610, 100, 22}, {714, 100, 22}, {818, 100, 22}, {922, 100, 22}};
const char* const kKnobNames[8] = {"MASTER", "SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
const int kKnobRole[8] = {-1, JIANT_E_SELECT, JIANT_E_PRESET, JIANT_E_ALGO, JIANT_E_K1, JIANT_E_K2, JIANT_E_K3, JIANT_E_K4};
const float kTrays[2][4] = {{50, 250, 220, 50}, {568, 160, 400, 140}};
const float kOct[2][4] = {{60, 258, 90, 34}, {170, 258, 90, 34}};
const float kGrid[6] = {582, 174, 54, 48, 63, 62};
const float kKeybed[4] = {36, 332, 928, 256};
const char* const kRow1[6] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO"};
const char* const kRow2[6] = {"HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"};
const int kRow1Bit[6] = {JIANT_B_FX, JIANT_B_SCL, JIANT_B_ENV, JIANT_B_LFO, JIANT_B_EDIT, JIANT_B_GLO};
const int kRow2Bit[6] = {JIANT_B_HOME, JIANT_B_SAVE, JIANT_B_ARP, JIANT_B_SEQ, JIANT_B_PLAY, JIANT_B_REC};
const int kWhite[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
const int kBlack[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
const int kBlackAfter[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
constexpr int kPlayGreen = 14;

const juce::Colour cBody(0xff1d1d1f), cTray(0xff141415), cPart(0xff2a2a2d), cEdge(0xff3a3a3e), cLabel(0xffd6d6d8),
    cLit(0xfffff4d6), cGreen(0xff5fe08a), cDim(0xff3a3934), cSlit(0xff3a3a3c), cSlitLit(0xffececee), cRec(0xffff453a);

// the buttons' rectangles, by bit
juce::Rectangle<float> buttonRect(int bit)
{
    if (bit == JIANT_B_OCTDN) return {kOct[0][0], kOct[0][1], kOct[0][2], kOct[0][3]};
    if (bit == JIANT_B_OCTUP) return {kOct[1][0], kOct[1][1], kOct[1][2], kOct[1][3]};
    for (int i = 0; i < 6; ++i) {
        if (kRow1Bit[i] == bit) return {kGrid[0] + i * kGrid[4], kGrid[1], kGrid[2], kGrid[3]};
        if (kRow2Bit[i] == bit) return {kGrid[0] + i * kGrid[4], kGrid[1] + kGrid[5], kGrid[2], kGrid[3]};
    }
    return {};
}
const char* buttonName(int bit)
{
    if (bit == JIANT_B_OCTDN) return "OCT-";
    if (bit == JIANT_B_OCTUP) return "OCT+";
    for (int i = 0; i < 6; ++i) {
        if (kRow1Bit[i] == bit) return kRow1[i];
        if (kRow2Bit[i] == bit) return kRow2[i];
    }
    return "";
}
// a key's pill, its centre and size
struct KeyGeom { float cx, cy, w, h; };
KeyGeom keyGeom(int k)
{
    const float x0 = kKeybed[0], y0 = kKeybed[1], w = kKeybed[2], h = kKeybed[3];
    const float pitch = (w - 24) / 16, kw = pitch * 0.78f, kh = h * 0.425f;
    for (int i = 0; i < 16; ++i)
        if (kWhite[i] == k) return {x0 + 12 + (i + 0.5f) * pitch, y0 + h * 0.75f, kw, kh};
    for (int j = 0; j < 11; ++j)
        if (kBlack[j] == k) return {x0 + 12 + (kBlackAfter[j] + 1) * pitch, y0 + h * 0.25f, kw, kh};
    return {0, 0, 0, 0};
}
// the computer keyboard (as the emulator's): Z..' (lower row and its black keys), Q..O the keys from B4 up
int keyForChar(juce::juce_wchar c)
{
    static const char* white = "zxcvbnm,./";
    static const int whiteK[10] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16};
    static const char* black = "sdfhjl;'";
    static const int blackK[8] = {1, 3, 5, 8, 10, 13, 15, 17};
    static const char* upper = "qwertyuio";
    c = juce::CharacterFunctions::toLowerCase(c);
    for (int i = 0; white[i]; ++i) if (white[i] == c) return whiteK[i];
    for (int i = 0; black[i]; ++i) if (black[i] == c) return blackK[i];
    for (int i = 0; upper[i]; ++i) if (upper[i] == c) return 18 + i;
    return -1;
}
int buttonForChar(juce::juce_wchar c)
{
    static const char* row = "1234567890-=";
    for (int i = 0; row[i]; ++i)
        if (row[i] == c) return i < 6 ? kRow1Bit[i] : kRow2Bit[i - 6];
    if (c == '[') return JIANT_B_OCTDN;
    if (c == ']') return JIANT_B_OCTUP;
    return -1;
}
}  // namespace

JiantEditor::JiantEditor(JiantProcessor& p) : AudioProcessorEditor(p), proc_(p), samples_(p)
{
    addChildComponent(samples_);
    samples_.onClose = [this] { samples_.setVisible(false); repaint(); };
    float master = proc_.params.getRawParameterValue("volume")->load();
    angle_[0] = -135.0f + 270.0f * master / 100.0f;
    setResizable(true, true);
    setResizeLimits(kW / 2, kH / 2, kW * 2, kH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio(double(kW) / kH);
    setSize(kW, kH);
    setWantsKeyboardFocus(true);
    startTimerHz(30);
}

JiantEditor::~JiantEditor()
{
    proc_.post({JiantProcessor::Command::Buttons, 0});
    proc_.post({JiantProcessor::Command::Keys, 0});
}

void JiantEditor::resized()
{
    scale_ = float(getWidth()) / kW;
    samples_.setBounds(juce::Rectangle<float>(kW - 340, 12, 328, kH - 24).transformedBy(juce::AffineTransform::scale(scale_)).toNearestInt());
}

bool JiantEditor::isInterestedInFileDrag(const juce::StringArray& files) { return samples_.isInterestedInFileDrag(files); }

void JiantEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    samples_.setVisible(true);
    if (samples_.getBounds().contains(x, y))
        samples_.filesDropped(files, x - samples_.getX(), y - samples_.getY());
    else
        samples_.load(samples_.selected(), files);
}

juce::Point<float> JiantEditor::toPanel(juce::Point<float> p) const { return p / scale_; }

void JiantEditor::timerCallback()
{
    bool dirty = false;
    if (proc_.copyScreen(frame_.data(), seen_)) {
        juce::Image::BitmapData px(screen_, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 240; ++x) {
                uint16_t v = frame_[size_t(y * 240 + x)];
                v = uint16_t(v >> 8 | v << 8);
                uint8_t r = uint8_t((v >> 11) << 3), g = uint8_t(((v >> 5) & 63) << 2), b = uint8_t((v & 31) << 3);
                px.setPixelColour(x, y, juce::Colour(uint8_t(r | r >> 5), uint8_t(g | g >> 6), uint8_t(b | b >> 5)));
            }
        dirty = true;
    }
    uint32_t lit = proc_.litButtons(), litK = proc_.litKeys();
    if (lit != lit_ || litK != litKeys_) {
        lit_ = lit;
        litKeys_ = litK;
        dirty = true;
    }
    if (dirty)
        repaint();
}

void JiantEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff8e8e93));
    g.addTransform(juce::AffineTransform::scale(scale_));
    g.setColour(cBody);
    g.fillRoundedRectangle(4, 4, kW - 8, kH - 8, 40);
    g.setColour(cTray);
    for (auto& t : kTrays)
        g.fillRoundedRectangle(t[0], t[1], t[2], t[3], 14);
    // the screen
    g.setColour(juce::Colour(0xff0b0b0c));
    g.fillRoundedRectangle(kScreen[0], kScreen[1], kScreen[2], kScreen[2], 18);
    const float inset = kScreen[2] / 32;
    juce::Rectangle<float> sr(kScreen[0] + inset, kScreen[1] + inset, kScreen[2] - 2 * inset, kScreen[2] - 2 * inset);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(screen_, sr, juce::RectanglePlacement::stretchToFit);
    if (!proc_.owner()) {
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRect(sr);
        g.setColour(cLabel);
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawFittedText("All " + juce::String(JiantProcessor::copies()) + " JIANTs are playing:\nthis one stays silent.", sr.toNearestInt(), juce::Justification::centred, 3);
    }
    // the knobs
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    for (int i = 0; i < 8; ++i) {
        const float x = kKnobs[i][0], y = kKnobs[i][1], r = kKnobs[i][2];
        g.setColour(cTray);
        g.fillEllipse(x - r - 5, y - r - 5, 2 * (r + 5), 2 * (r + 5));
        g.setColour(cPart);
        g.fillEllipse(x - r, y - r, 2 * r, 2 * r);
        g.setColour(cEdge);
        g.drawEllipse(x - r, y - r, 2 * r, 2 * r, 2);
        juce::Line<float> ind(x, y - r + 5, x, y - r + (i == 0 ? 14.0f : 10.0f));
        ind.applyTransform(juce::AffineTransform::rotation(angle_[i] * juce::MathConstants<float>::pi / 180.0f, x, y));
        g.setColour(cLabel);
        g.drawLine(ind, 3);
        g.drawText(kKnobNames[i], juce::Rectangle<float>(x - 60, y - r - 30, 120, 18), juce::Justification::centred);
    }
    // the buttons
    for (int bit = 0; bit < JIANT_NBUTTONS; ++bit) {
        auto rc = buttonRect(bit);
        bool green = bit == JIANT_B_PLAY && (lit_ & (1u << kPlayGreen));
        bool lit = (lit_ & (1u << bit)) && !green;
        bool held = ((sentButtons_ != ~0u ? sentButtons_ : 0) >> bit) & 1u;
        g.setColour(green ? cGreen : lit ? cLit : cPart);
        g.fillRoundedRectangle(rc, 8);
        g.setColour((latched_ >> bit) & 1u ? juce::Colour(0xff79b8ff) : held ? cLit : cEdge);
        g.drawRoundedRectangle(rc, 8, held || ((latched_ >> bit) & 1u) ? 3.0f : 1.5f);
        g.setColour(bit == JIANT_B_REC ? cRec : (green || lit) ? cBody : cLabel);
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        if (bit == JIANT_B_PLAY) {
            g.drawText("PLAY", rc.withY(rc.getY() + rc.getHeight() * 0.18f).withHeight(16), juce::Justification::centred);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText("STOP", rc.withY(rc.getY() + rc.getHeight() * 0.56f).withHeight(13), juce::Justification::centred);
        } else {
            g.drawText(buttonName(bit), rc, juce::Justification::centred);
        }
    }
    // the keys: two rows of pills, a slit down each that lights
    g.setColour(cTray);
    g.fillRoundedRectangle(kKeybed[0], kKeybed[1], kKeybed[2], kKeybed[3], 18);
    uint32_t heldKeys = sentKeys_ != ~0u ? sentKeys_ : 0;
    for (int k = 0; k < JIANT_KEYS; ++k) {
        KeyGeom kg = keyGeom(k);
        juce::Rectangle<float> rc(kg.cx - kg.w / 2, kg.cy - kg.h / 2, kg.w, kg.h);
        g.setColour(juce::Colour(0xff0c0c0d));
        g.fillRoundedRectangle(rc, kg.w / 2);
        g.setColour((heldKeys >> k) & 1u ? cLit : cEdge);
        g.drawRoundedRectangle(rc, kg.w / 2, (heldKeys >> k) & 1u ? 3.0f : 1.5f);
        float sw = std::max(3.0f, kg.w * 0.14f);
        g.setColour((litKeys_ >> k) & 1u ? cSlitLit : cSlit);
        g.fillRoundedRectangle(kg.cx - sw / 2, rc.getY() + kg.w * 0.4f, sw, kg.h - kg.w * 0.8f, sw / 2);
    }
    g.setColour(cLabel.withAlpha(0.5f));
    g.setFont(juce::FontOptions(11.0f));
    jlook::button(g, samplesButton_, "SAMPLES", samples_.isVisible());
    g.setColour(cLabel.withAlpha(0.5f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("JIANT " + juce::String(proc_.copy() + 1) + " OF " + juce::String(JiantProcessor::copies()) + "  -  CPU " + juce::String(proc_.cpuPercent()) + " %", juce::Rectangle<float>(36, 596, 928, 20),
               juce::Justification::centredRight);
}

// ------------------------------------------------------------------------------------------------------ input

JiantEditor::Hit JiantEditor::hitTest(juce::Point<float> p) const
{
    for (int i = 0; i < 8; ++i)
        if (p.getDistanceFrom({kKnobs[i][0], kKnobs[i][1]}) <= kKnobs[i][2] + 12)
            return {Knob, i};
    for (int bit = 0; bit < JIANT_NBUTTONS; ++bit)
        if (buttonRect(bit).expanded(3).contains(p))
            return {Button, bit};
    for (int k = 0; k < JIANT_KEYS; ++k) {
        KeyGeom kg = keyGeom(k);
        const float pitch = (kKeybed[2] - 24) / 16;
        if (juce::Rectangle<float>(kg.cx - pitch / 2, kg.cy - kKeybed[3] * 0.24f, pitch, kKeybed[3] * 0.48f).contains(p))
            return {Key, k};
    }
    return {};
}

void JiantEditor::syncButtons()
{
    uint32_t m = latched_;
    for (auto& [src, bit] : heldButtons_) m |= 1u << bit;
    for (auto& [code, bit] : keyButtons_) m |= 1u << bit;
    if (m != sentButtons_) {
        sentButtons_ = m;
        proc_.post({JiantProcessor::Command::Buttons, m});
        repaint();
    }
}

void JiantEditor::syncKeys()
{
    uint32_t m = 0;
    for (auto& [src, k] : heldKeys_) m |= 1u << k;
    for (auto& [code, k] : keyKeys_) m |= 1u << k;
    if (m != sentKeys_) {
        sentKeys_ = m;
        proc_.post({JiantProcessor::Command::Keys, m});
        repaint();
    }
}

void JiantEditor::turn(int knob, int n)
{
    if (!n)
        return;
    if (knob == 0) {                                     // MASTER: the Volume parameter (the host sees it move)
        auto* p = proc_.params.getParameter("volume");
        float v = juce::jlimit(0.0f, 100.0f, proc_.params.getRawParameterValue("volume")->load() + n * 1.6f);
        p->setValueNotifyingHost(p->convertTo0to1(v));
        angle_[0] = -135.0f + 270.0f * v / 100.0f;
    } else {
        proc_.post({JiantProcessor::Command::Encoder, 0, uint8_t(kKnobRole[knob]), n});
        angle_[knob] = std::fmod(angle_[knob] + n * 18.0f, 360.0f);
    }
    repaint();
}

void JiantEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (samplesButton_.contains(toPanel(e.position))) {
        samples_.setVisible(!samples_.isVisible());
        repaint();
        return;
    }
    Hit h = hitTest(toPanel(e.position));
    const int src = e.source.getIndex();
    if (h.kind == Knob)
        drags_[src] = {h.index, e.position.y, 0};
    else if (h.kind == Button) {
        uint32_t bit = 1u << h.index;
        if (e.mods.isShiftDown() || e.mods.isPopupMenu())
            latched_ ^= bit;
        else if (latched_ & bit)
            latched_ &= ~bit;
        else
            heldButtons_[src] = h.index;
        syncButtons();
    } else if (h.kind == Key) {
        heldKeys_[src] = h.index;
        syncKeys();
    }
}

void JiantEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto it = drags_.find(e.source.getIndex());
    if (it == drags_.end())
        return;
    Drag& d = it->second;
    d.acc += (d.y - e.position.y) / scale_;
    d.y = e.position.y;
    const float step = d.knob == 0 ? 3.0f : 8.0f;
    int n = int(d.acc / step);
    if (n) {
        d.acc -= n * step;
        turn(d.knob, n);
    }
}

void JiantEditor::mouseUp(const juce::MouseEvent& e)
{
    const int src = e.source.getIndex();
    drags_.erase(src);
    if (heldButtons_.erase(src))
        syncButtons();
    if (heldKeys_.erase(src))
        syncKeys();
}

void JiantEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    Hit h = hitTest(toPanel(e.position));
    if (h.kind != Knob)
        return;
    static float acc = 0;
    acc += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 10.0f;
    int n = int(acc);
    if (n) {
        acc -= float(n);
        turn(h.index, n);
    }
}

bool JiantEditor::keyPressed(const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::upKey) { turn(1, 1); return true; }
    if (code == juce::KeyPress::downKey) { turn(1, -1); return true; }
    if (code == juce::KeyPress::rightKey) { turn(3, 1); return true; }
    if (code == juce::KeyPress::leftKey) { turn(3, -1); return true; }
    const juce::juce_wchar c = key.getTextCharacter();
    if (int k = keyForChar(c); k >= 0) {
        if (!keyKeys_.count(code)) { keyKeys_[code] = k; syncKeys(); }
        return true;
    }
    if (int b = buttonForChar(c); b >= 0) {
        if (!keyButtons_.count(code)) { keyButtons_[code] = b; syncButtons(); }
        return true;
    }
    return false;
}

bool JiantEditor::keyStateChanged(bool isKeyDown)
{
    if (isKeyDown)
        return false;
    bool changed = false;
    for (auto it = keyKeys_.begin(); it != keyKeys_.end();)
        if (!juce::KeyPress::isKeyCurrentlyDown(it->first)) { it = keyKeys_.erase(it); changed = true; } else ++it;
    for (auto it = keyButtons_.begin(); it != keyButtons_.end();)
        if (!juce::KeyPress::isKeyCurrentlyDown(it->first)) { it = keyButtons_.erase(it); changed = true; } else ++it;
    if (changed) { syncKeys(); syncButtons(); }
    return changed;
}

void JiantEditor::focusLost(FocusChangeType)
{
    keyKeys_.clear();
    keyButtons_.clear();
    syncKeys();
    syncButtons();
}
