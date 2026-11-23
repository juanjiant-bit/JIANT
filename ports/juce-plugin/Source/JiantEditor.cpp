// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "JiantEditor.h"
#include "JiantLook.h"
#include "jiant_core.h"
#include <climits>
#include <cmath>

namespace {
using R = juce::Rectangle<float>;
// the layout, in the editor's 1100 x 700 units
const R kTop {0, 0, 1100, 48};
const R kTracks {16, 64, 252, 372};                      // six rows
constexpr float kRowH = 62;
const R kScreenBox {284, 64, 480, 480};
const float kBtnY[2] = {556, 598};
constexpr float kBtnH = 34, kBtnGap = 6;
const char* const kRow1[7] = {"HOME", "EDIT", "FX", "ENV", "LFO", "SCL", "ARP"};
const char* const kRow2[7] = {"SEQ", "GLO", "SAVE", "PLAY", "REC", "OCT-", "OCT+"};
const int kRow1Bit[7] = {JIANT_B_HOME, JIANT_B_EDIT, JIANT_B_FX, JIANT_B_ENV, JIANT_B_LFO, JIANT_B_SCL, JIANT_B_ARP};
const int kRow2Bit[7] = {JIANT_B_SEQ, JIANT_B_GLO, JIANT_B_SAVE, JIANT_B_PLAY, JIANT_B_REC, JIANT_B_OCTDN, JIANT_B_OCTUP};
// the knobs: 0 MASTER, 1 SELECT, 2 PRESETS, 3 ALGORITHM (small, left), 4..7 the page knobs (big, right)
const char* const kKnobNames[8] = {"MASTER", "SELECT", "PRESETS", "ALGO", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
const int kKnobRole[8] = {-1, JIANT_E_SELECT, JIANT_E_PRESET, JIANT_E_ALGO, JIANT_E_K1, JIANT_E_K2, JIANT_E_K3, JIANT_E_K4};
juce::Point<float> knobCentre(int i)
{
    if (i < 4) {
        static const int order[4] = {1, 2, 3, 0};       // SELECT PRESETS ALGO MASTER, left to right
        int at = 0;
        while (order[at] != i) ++at;
        return {16 + 32 + at * 63.0f, 478};
    }
    return {780 + 38 + (i - 4) * 76.0f, 112};
}
float knobRadius(int i) { return i < 4 ? 21.0f : 29.0f; }
const R kSound {776, 172, 312, 452};
const R kEngRow {780, 198, 304, 28};
constexpr float kPresetY = 234, kPresetH = 22;
constexpr int kPresetRows = 7;
juce::Point<float> editCentre(int i) { return {780 + 38 + (i % 4) * 76.0f, 452 + (i / 4) * 92.0f}; }
const R kAudition {16, 560, 252, 64};
const R kSamplesBtn {872, 12, 112, 26};
constexpr int kPlayGreen = 14;
constexpr int kAuditionFirst = 7;                        // C4 (key 0 is F3)

R buttonRect(int bit)
{
    const float w = (kScreenBox.getWidth() - 6 * kBtnGap) / 7;
    for (int i = 0; i < 7; ++i) {
        if (kRow1Bit[i] == bit) return {kScreenBox.getX() + i * (w + kBtnGap), kBtnY[0], w, kBtnH};
        if (kRow2Bit[i] == bit) return {kScreenBox.getX() + i * (w + kBtnGap), kBtnY[1], w, kBtnH};
    }
    return {};
}
const char* buttonName(int bit)
{
    for (int i = 0; i < 7; ++i) {
        if (kRow1Bit[i] == bit) return kRow1[i];
        if (kRow2Bit[i] == bit) return kRow2[i];
    }
    return "";
}
R trackRect(int t) { return {kTracks.getX(), kTracks.getY() + t * kRowH, kTracks.getWidth(), kRowH - 6}; }
R muteRect(int t)
{
    auto r = trackRect(t);
    return {r.getRight() - 34, r.getY() + 8, 26, 22};
}
R presetRect(int row) { return {780, kPresetY + row * kPresetH, 304, kPresetH - 2}; }
R auditionKey(int i) { return {kAudition.getX() + i * (kAudition.getWidth() / 13), kAudition.getY(), kAudition.getWidth() / 13 - 2, kAudition.getHeight()}; }
bool blackKey(int i) { static const bool b[13] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0}; return b[i]; }
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
    static const int bits[12] = {JIANT_B_FX, JIANT_B_SCL, JIANT_B_ENV, JIANT_B_LFO, JIANT_B_EDIT, JIANT_B_GLO,
                                 JIANT_B_HOME, JIANT_B_SAVE, JIANT_B_ARP, JIANT_B_SEQ, JIANT_B_PLAY, JIANT_B_REC};
    for (int i = 0; row[i]; ++i)
        if (row[i] == c) return bits[i];
    if (c == '[') return JIANT_B_OCTDN;
    if (c == ']') return JIANT_B_OCTUP;
    return -1;
}
int v14(const uint8_t* p) { return int(p[0] | p[1] << 7) - 8192; }
juce::String cstr(const std::vector<uint8_t>& r, size_t& at)
{
    juce::String s;
    while (at < r.size() && r[at]) s += juce::juce_wchar(r[at++]);
    ++at;
    return s;
}
int64_t nowMs() { return juce::Time::currentTimeMillis(); }
}  // namespace

JiantEditor::JiantEditor(JiantProcessor& p) : AudioProcessorEditor(p), proc_(p), samples_(p)
{
    addChildComponent(samples_);
    samples_.onClose = [this] { showSamples(false); };
    std::fill(std::begin(pending_), std::end(pending_), INT_MIN);
    float master = proc_.params.getRawParameterValue("volume")->load();
    angle_[0] = -135.0f + 270.0f * master / 100.0f;
    setResizable(true, true);
    setResizeLimits(kW * 6 / 10, kH * 6 / 10, kW * 2, kH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio(double(kW) / kH);
    setSize(kW, kH);
    setWantsKeyboardFocus(true);
    refreshNow();
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
    samples_.setBounds(R(776, 172, 312, 452).transformedBy(juce::AffineTransform::scale(scale_)).toNearestInt());
}

juce::Point<float> JiantEditor::toPanel(juce::Point<float> p) const { return p / scale_; }

void JiantEditor::showSamples(bool on)
{
    samples_.setVisible(on);
    repaint();
}

void JiantEditor::refreshNow()
{
    link_ = proc_.link();
    loadSound();
    repaint();
}

void JiantEditor::loadSound()
{
    const auto& info = proc_.info();
    if (!info.ok || !link_.seq)
        return;
    const int eng = link_.t[juce::jlimit(0, JiantProcessor::kTracks - 1, link_.selected)].engine;
    if (eng == sound_.engine)
        return;
    Sound s;
    s.engine = eng;
    auto names = proc_.request({10, uint8_t(eng)});
    if (names.size() > 3 && names[1] == eng) {
        size_t at = 3;
        for (int i = 0; i < names[2] && at < names.size(); ++i) s.presets.add(cstr(names, at));
    }
    for (int i = 0; i < 8; ++i) {
        auto d = proc_.request({5, 0, uint8_t(info.pE0 + i)});
        if (d.size() < 11)
            continue;
        Desc& e = s.edit[i];
        e.fmt = d[3];
        e.min = v14(&d[4]);
        e.max = v14(&d[6]);
        size_t at = 10;
        e.label = cstr(d, at);
        e.unit = cstr(d, at);
        if (e.fmt == 8)
            while (at < d.size()) e.names.add(cstr(d, at));
    }
    sound_ = s;
    const int preset = link_.t[link_.selected].preset;
    if (preset < presetTop_ || preset >= presetTop_ + kPresetRows)
        presetTop_ = juce::jmax(0, preset - kPresetRows / 2);
}

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
    if (++tick_ % 3 == 0) {                              // the tracks and the sound, ~10 a second
        auto l = proc_.link();
        if (l.seq != link_.seq) {
            link_ = l;
            loadSound();
            for (int i = 0; i < 8; ++i)
                if (pending_[i] != INT_MIN && nowMs() - pendingAt_[i] > 600)
                    pending_[i] = INT_MIN;
            dirty = true;
        }
        if (tick_ % 15 == 0)
            dirty = true;                                // (the CPU readout)
    }
    if (dirty)
        repaint();
}

// --------------------------------------------------------------------------------------------------------- paint

void JiantEditor::paint(juce::Graphics& g)
{
    g.fillAll(jlook::bg);
    g.addTransform(juce::AffineTransform::scale(scale_));
    jlook::paintBody(g, {0, 0, float(kW), float(kH)});
    paintTop(g);
    paintTracks(g);
    paintAudition(g);
    paintScreen(g);
    paintButtons(g);
    paintKnobs(g);
    if (!samples_.isVisible())
        paintSound(g);
}

void JiantEditor::paintTop(juce::Graphics& g)
{
    g.setColour(jlook::cream);
    g.setFont(jlook::dot(26.0f));
    g.drawText("JIANT FM", R(20, 9, 150, 32), juce::Justification::centredLeft);
    R badge(170, 13, 104, 24);
    jlook::brackets(g, badge, jlook::org, 6.0f, 1.5f);
    g.setFont(jlook::mono(12.0f));
    g.setColour(proc_.owner() ? jlook::ink : jlook::red);
    g.drawText(proc_.owner() ? "JIANT " + juce::String(proc_.copy() + 1) + "/" + juce::String(JiantProcessor::copies())
                             : juce::String("SILENT"),
               badge, juce::Justification::centred);
    const bool play = proc_.playing();
    g.setColour(play ? jlook::grn : jlook::dim);
    juce::Path tri;
    if (play)
        tri.addTriangle(300, 18, 300, 32, 312, 25);
    else
        tri.addRectangle(300, 19, 12, 12);
    g.fillPath(tri);
    g.setFont(jlook::mono(12.0f));
    g.drawText(juce::String(play ? "PLAYING  " : "STOPPED  ") + juce::String(proc_.bpm()) + " BPM", R(320, 13, 180, 24),
               juce::Justification::centredLeft);
    const bool host = int(proc_.params.getRawParameterValue("sync")->load()) == 1;
    g.setColour(jlook::faint);
    g.drawText(host ? "SYNC HOST" : "SYNC INTERNAL", R(500, 13, 120, 24), juce::Justification::centredLeft);
    jlook::button(g, kSamplesBtn, "SAMPLES", samples_.isVisible(), false, jlook::org, 14.0f);
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(12.0f));
    g.drawText("CPU " + juce::String(proc_.cpuPercent()) + "%", R(992, 13, 92, 24), juce::Justification::centredRight);
}

void JiantEditor::paintTracks(juce::Graphics& g)
{
    const auto& info = proc_.info();
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(10.5f));
    g.drawText("TRACKS", R(kTracks.getX(), 50, 200, 14), juce::Justification::centredLeft);
    const int n = link_.seq ? link_.nTrk : JiantProcessor::kTracks;
    for (int t = 0; t < JiantProcessor::kTracks; ++t) {
        auto r = trackRect(t);
        const auto& tr = link_.t[t];
        const bool sel = link_.seq && t == link_.selected, on = t < n;
        const juce::Colour c = jlook::track(t);
        g.setColour(sel ? jlook::surf : jlook::panel);
        g.fillRect(r);
        g.setColour(jlook::line);
        g.drawRect(r, 1.0f);
        g.setColour(tr.mute ? c.withAlpha(0.3f) : c);
        g.fillRect(r.getX(), r.getY(), 4.0f, r.getHeight());
        if (sel)
            jlook::brackets(g, r.expanded(2), jlook::org, 9.0f, 1.5f);
        g.setFont(jlook::dot(22.0f));
        g.setColour(c);
        g.drawText(juce::String(t + 1), R(r.getX() + 10, r.getY() + 4, 22, 30), juce::Justification::centredLeft);
        juce::String eng = on && info.ok && tr.engine < info.engines.size() ? info.engines[tr.engine] : juce::String("-");
        g.setFont(jlook::dot(16.0f));
        g.setColour(tr.mute ? jlook::faint : jlook::ink);
        g.drawText(eng, R(r.getX() + 36, r.getY() + 5, 150, 20), juce::Justification::centredLeft, true);
        juce::String pre = sel && sound_.engine == tr.engine && tr.preset < sound_.presets.size()
                               ? sound_.presets[tr.preset] : "PRESET " + juce::String(tr.preset + 1);
        g.setFont(jlook::mono(10.5f));
        g.setColour(jlook::dim);
        g.drawText(tr.mute ? "MUTED" : pre, R(r.getX() + 36, r.getY() + 25, 170, 14), juce::Justification::centredLeft, true);
        // the level, as heat
        R bar(r.getX() + 36, r.getBottom() - 11, r.getWidth() - 48, 4);
        g.setColour(jlook::line);
        g.fillRect(bar);
        g.setColour(tr.mute ? jlook::faint : c);
        g.fillRect(bar.withWidth(bar.getWidth() * juce::jlimit(0, 127, tr.level) / 127.0f));
        if (tr.armed) {
            g.setColour(jlook::red);
            g.fillEllipse(r.getRight() - 50, r.getY() + 14, 8, 8);
        }
        auto m = muteRect(t);
        jlook::button(g, m, "M", tr.mute != 0, false, jlook::vio, 13.0f);
    }
}

void JiantEditor::paintAudition(juce::Graphics& g)
{
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(10.5f));
    g.drawText("AUDITION  C4-C5", R(kAudition.getX(), kAudition.getY() - 16, 200, 14), juce::Justification::centredLeft);
    uint32_t held = sentKeys_ != ~0u ? sentKeys_ : 0;
    for (int i = 0; i < 13; ++i) {
        auto r = auditionKey(i);
        const int k = kAuditionFirst + i;
        const bool down = (held >> k) & 1u, lit = (litKeys_ >> k) & 1u;
        g.setColour(down ? jlook::org : blackKey(i) ? juce::Colour(0xff07060c) : jlook::panel);
        g.fillRect(r);
        g.setColour(blackKey(i) ? jlook::vio.withAlpha(0.45f) : jlook::line);
        g.drawRect(r, 1.0f);
        g.setColour(down ? jlook::cream : lit ? jlook::org : blackKey(i) ? jlook::vio.withAlpha(0.6f) : jlook::lineHi);
        g.fillRect(r.getCentreX() - 1.5f, r.getY() + 8, 3.0f, r.getHeight() - 16);
    }
}

void JiantEditor::paintScreen(juce::Graphics& g)
{
    auto box = kScreenBox;
    g.setColour(juce::Colours::black);
    g.fillRect(box);
    jlook::brackets(g, box.expanded(6), jlook::org, 16.0f, 2.0f);
    // an integer scale of 240 in the box's pixels, centred: every screen pixel the same size
    const float px = box.getWidth() * scale_;
    const int k = juce::jmax(1, int(px / 240.0f));
    const float side = float(240 * k) / scale_;
    R sr(box.getCentreX() - side / 2, box.getCentreY() - side / 2, side, side);
    g.saveState();
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(screen_, sr, juce::RectanglePlacement::stretchToFit);
    g.restoreState();
    if (!proc_.owner()) {
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRect(box);
        g.setColour(jlook::cream);
        g.setFont(jlook::dot(20.0f));
        g.drawFittedText("All " + juce::String(JiantProcessor::copies()) + " JIANTs are playing:\nthis one stays silent.",
                         box.toNearestInt(), juce::Justification::centred, 3);
    }
    g.setColour(jlook::faint);
    g.setFont(jlook::mono(10.0f));
    g.drawText("WHEEL / DRAG ON THE SCREEN: SELECT   SHIFT: TRACK   KEYS Z..M  Q..O   1..= BUTTONS",
               R(kScreenBox.getX(), 640, kScreenBox.getWidth(), 16), juce::Justification::centred);
}

void JiantEditor::paintButtons(juce::Graphics& g)
{
    for (int row = 0; row < 2; ++row)
        for (int i = 0; i < 7; ++i) {
            const int bit = row ? kRow2Bit[i] : kRow1Bit[i];
            auto rc = buttonRect(bit);
            const bool green = bit == JIANT_B_PLAY && (lit_ & (1u << kPlayGreen));
            const bool lit = ((lit_ >> bit) & 1u) || green;
            const bool held = ((sentButtons_ != ~0u ? sentButtons_ : 0) >> bit) & 1u;
            const bool latched = (latched_ >> bit) & 1u;
            juce::Colour on = bit == JIANT_B_REC ? jlook::red : green ? jlook::grn : latched ? jlook::cyan : jlook::org;
            jlook::button(g, rc, buttonName(bit), lit || latched, held, on, 15.0f);
        }
}

void JiantEditor::paintKnobs(juce::Graphics& g)
{
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(10.5f));
    g.drawText("PAGE KNOBS", R(780, 50, 200, 14), juce::Justification::centredLeft);
    const float master = proc_.params.getRawParameterValue("volume")->load() / 100.0f;
    for (int i = 0; i < 8; ++i) {
        auto c = knobCentre(i);
        const float r = knobRadius(i);
        if (i == 0) {
            jlook::ring(g, c, r, master, jlook::heat(master), false, 4.0f);
        } else {                                         // an encoder: endless, its point where it was turned to
            jlook::ring(g, c, r, 0.0f, jlook::vio, false, i < 4 ? 3.0f : 4.0f);
            const float a = angle_[i] * juce::MathConstants<float>::pi / 180.0f;
            g.setColour(i < 4 ? jlook::vio : jlook::heat(float(i - 4) / 3.0f));
            const float rr = r - (i < 4 ? 8.0f : 11.0f);
            g.fillEllipse(c.x + rr * std::sin(a) - 3, c.y - rr * std::cos(a) - 3, 6, 6);
        }
        g.setColour(jlook::ink);
        g.setFont(jlook::dot(i < 4 ? 11.0f : 13.0f));
        g.drawText(kKnobNames[i], R(c.x - 40, c.y + r + 3, 80, 16), juce::Justification::centred);
    }
}

void JiantEditor::paintSound(juce::Graphics& g)
{
    const auto& info = proc_.info();
    const int sel = juce::jlimit(0, JiantProcessor::kTracks - 1, link_.selected);
    const auto& tr = link_.t[sel];
    g.setColour(jlook::body);
    g.fillRect(kSound);
    g.setColour(jlook::line);
    g.drawRect(kSound, 1.0f);
    jlook::brackets(g, kSound, jlook::org.withAlpha(0.8f), 10.0f, 1.5f);
    g.setColour(jlook::track(sel));
    g.setFont(jlook::dot(15.0f));
    g.drawText("TRACK " + juce::String(sel + 1) + "  SOUND", R(786, 176, 240, 20), juce::Justification::centredLeft);
    // the engine, < >
    jlook::button(g, {780, kEngRow.getY(), 28, 28}, "<", false, false, jlook::vio, 15.0f);
    jlook::button(g, {1056, kEngRow.getY(), 28, 28}, ">", false, false, jlook::vio, 15.0f);
    g.setColour(jlook::cream);
    g.setFont(jlook::dot(18.0f));
    juce::String eng = info.ok && tr.engine < info.engines.size() ? info.engines[tr.engine] : juce::String("-");
    g.drawText(eng, R(812, kEngRow.getY(), 240, 28), juce::Justification::centred);
    // its presets
    for (int row = 0; row < kPresetRows; ++row) {
        const int p = presetTop_ + row;
        if (p >= sound_.presets.size())
            break;
        auto r = presetRect(row);
        const bool cur = p == tr.preset && sound_.engine == tr.engine;
        g.setColour(cur ? jlook::org.withAlpha(0.16f) : (row & 1) ? jlook::surf : jlook::panel);
        g.fillRect(r);
        if (cur) {
            g.setColour(jlook::org);
            g.drawRect(r, 1.0f);
        }
        g.setFont(jlook::mono(10.0f));
        g.setColour(jlook::faint);
        g.drawText(juce::String(p + 1).paddedLeft('0', 2), r.withWidth(28).translated(6, 0), juce::Justification::centredLeft);
        g.setFont(jlook::dot(13.0f));
        g.setColour(cur ? jlook::cream : jlook::ink);
        g.drawText(sound_.presets[p], r.withTrimmedLeft(34), juce::Justification::centredLeft, true);
    }
    if (sound_.presets.size() > kPresetRows) {
        g.setColour(jlook::faint);
        g.setFont(jlook::mono(9.5f));
        g.drawText(juce::String(presetTop_ + 1) + "-" + juce::String(juce::jmin(sound_.presets.size(), presetTop_ + kPresetRows)) +
                       " OF " + juce::String(sound_.presets.size()) + "  (WHEEL)",
                   R(780, kPresetY + kPresetRows * kPresetH, 300, 14), juce::Justification::centredRight);
    }
    // EDIT: the engine's eight parameters
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(10.5f));
    g.drawText("EDIT", R(786, 404, 100, 14), juce::Justification::centredLeft);
    for (int i = 0; i < 8; ++i) {
        const Desc& d = sound_.edit[i];
        auto c = editCentre(i);
        const int v = pending_[i] != INT_MIN ? pending_[i] : info.ok ? tr.p[size_t(info.pE0 + i)] : 0;
        const bool live = d.label.isNotEmpty() && d.label != "-" && d.max > d.min;
        const float v01 = live ? float(v - d.min) / float(d.max - d.min) : 0.0f;
        jlook::ring(g, c, 24, v01, live ? jlook::heat(v01) : jlook::faint, live && d.min < 0, 4.0f);
        g.setColour(live ? jlook::ink : jlook::faint);
        g.setFont(jlook::dot(12.0f));
        g.drawText(live ? d.label : juce::String("-"), R(c.x - 38, c.y + 27, 76, 14), juce::Justification::centred, true);
        juce::String val = !live ? juce::String() : d.fmt == 8 && v - d.min >= 0 && v - d.min < d.names.size()
                                       ? d.names[v - d.min] : juce::String(v) + (d.unit.isNotEmpty() ? " " + d.unit : "");
        g.setColour(jlook::dim);
        g.setFont(jlook::mono(9.5f));
        g.drawText(val, R(c.x - 38, c.y + 41, 76, 13), juce::Justification::centred, true);
        g.setColour(jlook::cream);
        g.setFont(jlook::mono(9.5f));
    }
}

// ------------------------------------------------------------------------------------------------------ input

JiantEditor::Hit JiantEditor::hitTest(juce::Point<float> p) const
{
    if (kSamplesBtn.contains(p)) return {Samples, 0};
    for (int i = 0; i < 8; ++i)
        if (p.getDistanceFrom(knobCentre(i)) <= knobRadius(i) + 8)
            return {Knob, i};
    for (int bit = 0; bit < JIANT_NBUTTONS; ++bit)
        if (buttonRect(bit).expanded(2).contains(p))
            return {Button, bit};
    if (kScreenBox.contains(p)) return {Screen, 0};
    for (int t = 0; t < JiantProcessor::kTracks; ++t) {
        if (muteRect(t).contains(p)) return {Mute, t};
        if (trackRect(t).contains(p)) return {TrackRow, t};
    }
    for (int i = 0; i < 13; ++i)
        if (auditionKey(i).contains(p)) return {Key, kAuditionFirst + i};
    if (!samples_.isVisible()) {
        if (R(780, kEngRow.getY(), 28, 28).contains(p)) return {EngPrev, 0};
        if (R(1056, kEngRow.getY(), 28, 28).contains(p)) return {EngNext, 0};
        for (int row = 0; row < kPresetRows; ++row)
            if (presetRect(row).contains(p) && presetTop_ + row < sound_.presets.size())
                return {Preset, presetTop_ + row};
        for (int i = 0; i < 8; ++i)
            if (p.getDistanceFrom(editCentre(i)) <= 30)
                return {Edit, i};
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
        JiantProcessor::Command c {JiantProcessor::Command::Encoder, 0, uint8_t(kKnobRole[knob]), n};
        proc_.post(c);
        angle_[knob] = std::fmod(angle_[knob] + n * 18.0f, 360.0f);
    }
    repaint();
}

void JiantEditor::editTurn(int i, int n)
{
    const auto& info = proc_.info();
    const Desc& d = sound_.edit[i];
    if (!n || !info.ok || d.max <= d.min || d.label == "-")
        return;
    const int sel = juce::jlimit(0, JiantProcessor::kTracks - 1, link_.selected);
    const int cur = pending_[i] != INT_MIN ? pending_[i] : link_.t[sel].p[size_t(info.pE0 + i)];
    const int v = juce::jlimit(d.min, d.max, cur + n);
    if (v == cur)
        return;
    const int x = v + 8192;
    proc_.postRequest({3, 0, uint8_t(info.pE0 + i), uint8_t(x & 127), uint8_t(x >> 7)});   // SET, the selected track
    pending_[i] = v;
    pendingAt_[i] = nowMs();
    repaint();
}

void JiantEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    Hit h = hitTest(toPanel(e.position));
    const int src = e.source.getIndex();
    const auto& info = proc_.info();
    switch (h.kind) {
    case Samples: showSamples(!samples_.isVisible()); break;
    case Knob: drags_[src] = {Knob, h.index, e.position.y, 0}; break;
    case Edit: drags_[src] = {Edit, h.index, e.position.y, 0}; break;
    case Screen: drags_[src] = {Screen, e.mods.isShiftDown() ? 3 : 1, e.position.y, 0}; break;
    case Button: {
        uint32_t bit = 1u << h.index;
        if (e.mods.isShiftDown() || e.mods.isPopupMenu())
            latched_ ^= bit;
        else if (latched_ & bit)
            latched_ &= ~bit;
        else
            heldButtons_[src] = h.index;
        syncButtons();
        break;
    }
    case Key:
        heldKeys_[src] = h.index;
        syncKeys();
        break;
    case TrackRow:
        proc_.postRequest({27, uint8_t(h.index)});       // TRACK: select it
        link_.selected = h.index;
        loadSound();
        repaint();
        break;
    case Mute: {
        const int x = (link_.t[h.index].mute ? 0 : 1) + 8192;
        proc_.postRequest({31, uint8_t(h.index), 40, uint8_t(x & 127), uint8_t(x >> 7)});   // TRACK_PARAM P_MUTE
        link_.t[h.index].mute = !link_.t[h.index].mute;
        repaint();
        break;
    }
    case EngPrev:
    case EngNext: {
        if (!info.ok)
            break;
        const int n = info.engines.size(), dir = h.kind == EngNext ? 1 : -1;
        int e2 = link_.t[link_.selected].engine;
        for (int i = 0; i < n; ++i) {                    // the next engine with a name (retired ones are "-")
            e2 = (e2 + dir + n) % n;
            if (info.engines[e2] != "-") break;
        }
        proc_.postRequest({8, uint8_t(e2), 0});          // PRESET: its first sound
        presetTop_ = 0;
        break;
    }
    case Preset:
        proc_.postRequest({8, uint8_t(sound_.engine), uint8_t(h.index)});
        link_.t[link_.selected].preset = h.index;
        repaint();
        break;
    default: break;
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
    float step = 8.0f;
    if (d.kind == Knob && d.index == 0) step = 3.0f;
    if (d.kind == Edit) {
        const Desc& ds = sound_.edit[d.index];
        step = ds.max - ds.min > 40 ? 2.0f : 9.0f;
    }
    if (d.kind == Screen) step = 14.0f;
    int n = int(d.acc / step);
    if (n) {
        d.acc -= n * step;
        if (d.kind == Edit) editTurn(d.index, n);
        else turn(d.index, d.kind == Screen ? -n : n);   // (the screen: dragged up, the list goes up)
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
    static float acc = 0;
    acc += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 10.0f;
    int n = int(acc);
    if (!n)
        return;
    acc -= float(n);
    if (h.kind == Knob) turn(h.index, n);
    else if (h.kind == Screen) turn(e.mods.isShiftDown() ? 3 : 1, -n);
    else if (h.kind == Edit) editTurn(h.index, n);
    else if (h.kind == Preset || (!samples_.isVisible() && R(780, kPresetY, 304, kPresetRows * kPresetH).contains(toPanel(e.position)))) {
        presetTop_ = juce::jlimit(0, juce::jmax(0, sound_.presets.size() - kPresetRows), presetTop_ - n);
        repaint();
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

bool JiantEditor::isInterestedInFileDrag(const juce::StringArray& files) { return samples_.isInterestedInFileDrag(files); }

void JiantEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    showSamples(true);
    if (samples_.getBounds().contains(x, y))
        samples_.filesDropped(files, x - samples_.getX(), y - samples_.getY());
    else
        samples_.load(samples_.selected(), files);
}
