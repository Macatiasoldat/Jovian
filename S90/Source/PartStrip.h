#pragma once
// Composants d'édition d'une partie (son, canal, zones, niveau, pan, envois) pour les pages Scène et Multi.
#include "Scenes.h"
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>

namespace s90ui
{
inline juce::String noteName (int n)
{
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 2);   // convention Yamaha / Cubase / GarageBand : Do central = C3 = 60
}

// Petite boîte numérique « façon écran LCD » : glisser (ou molette) pour changer, double-clic pour revenir à la valeur par défaut.
class NumBox : public juce::Component
{
public:
    std::function<juce::String (double)> format;
    std::function<void (double)> onChange;

    void setRange (double mn, double mx, double st, double def)
    {
        minV = mn; maxV = mx; step = st; defV = def;
        value = juce::jlimit (mn, mx, value);
    }
    void setValue (double v, bool notify = false)
    {
        v = juce::jlimit (minV, maxV, std::round (v / step) * step);
        if (v == value) { repaint(); return; }
        value = v;
        repaint();
        if (notify && onChange) onChange (value);
    }
    double getValue() const { return value; }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff06202c));
        g.fillRoundedRectangle (r, 4.f);
        // barre de remplissage discrète
        const double frac = (maxV > minV) ? (value - minV) / (maxV - minV) : 0.0;
        g.setColour (juce::Colour (0xff12485c).withAlpha (isEnabled() ? 1.f : 0.4f));
        g.fillRoundedRectangle (r.withWidth ((float) (r.getWidth() * frac)).reduced (0.f, 1.f), 3.f);
        g.setColour (juce::Colour (hover ? 0xff2f9fbf : 0xff0e3a4c));
        g.drawRoundedRectangle (r, 4.f, 1.f);
        g.setColour (juce::Colour (0xff7fe6ff));
        g.setFont (juce::FontOptions (12.f, juce::Font::bold));
        g.drawText (format ? format (value) : juce::String (value), getLocalBounds(), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { startValue = value; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const double steps = (maxV - minV) / step;
        const double pxPerStep = steps > 40 ? 1.5 : 6.0;     // plages longues (notes, niveau) : fin ; plages courtes : large
        const double d = (double) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
        setValue (startValue + std::round (d / pxPerStep) * step, true);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const double d = (w.deltaY != 0 ? w.deltaY : w.deltaX);
        if (d != 0) setValue (value + (d > 0 ? step : -step), true);
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (defV, true); }

private:
    double minV = 0, maxV = 1, step = 1, defV = 0, value = 0, startValue = 0;
    bool hover = false;
};

// Une partie : ligne 1 = ON, son, niveau, pan ; ligne 2 = canal, zone de clavier, zone de vélocité, transposition, envois
class PartStrip : public juce::Component
{
public:
    std::function<void (const s90::PartCfg&)> onChange;

    PartStrip()
    {
        onBtn.setButtonText ("ON");
        onBtn.setClickingTogglesState (true);
        onBtn.onClick = [this] { cfg.on = onBtn.getToggleState(); push(); };
        addAndMakeVisible (onBtn);

        for (int c = 0; c < s90::kNumCats; ++c)
        {
            voiceBox.addSectionHeading (s90::kCatNames[c]);
            for (int i = 0; i < 10; ++i)
                voiceBox.addItem (juce::String (c * 10 + i + 1).paddedLeft ('0', 3) + "  " + s90::bank()[(size_t) (c * 10 + i)].name, c * 10 + i + 1);
        }
        voiceBox.onChange = [this] { cfg.patch = juce::jlimit (0, s90::kNumPatches - 1, voiceBox.getSelectedId() - 1); push(); };
        addAndMakeVisible (voiceBox);

        auto setup = [this] (NumBox& b, double mn, double mx, double st, double def, std::function<juce::String (double)> fmt,
                             std::function<void (double)> set)
        {
            b.setRange (mn, mx, st, def);
            b.format = std::move (fmt);
            b.onChange = [this, set] (double v) { set (v); push(); };
            addAndMakeVisible (b);
        };
        setup (level, 0, 1, 0.01, 0.8, [] (double v) { return "LVL " + juce::String (juce::roundToInt (v * 100)); }, [this] (double v) { cfg.level = (float) v; });
        setup (pan, -1, 1, 0.02, 0, [] (double v) { const int p = juce::roundToInt (v * 50); return "PAN " + (p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p * 2) : "R" + juce::String (p * 2))); },
               [this] (double v) { cfg.pan = (float) v; });
        setup (ch, 0, 16, 1, 0, [] (double v) { return v < 0.5 ? juce::String ("CH OMNI") : "CH " + juce::String ((int) v); }, [this] (double v) { cfg.channel = (int) v; });
        setup (lo, 0, 127, 1, 0, [] (double v) { return "LO " + noteName ((int) v); },
               [this] (double v) { cfg.lo = (int) v; if (cfg.hi < cfg.lo) { cfg.hi = cfg.lo; hi.setValue (cfg.hi); } });
        setup (hi, 0, 127, 1, 127, [] (double v) { return "HI " + noteName ((int) v); },
               [this] (double v) { cfg.hi = (int) v; if (cfg.lo > cfg.hi) { cfg.lo = cfg.hi; lo.setValue (cfg.lo); } });
        setup (vlo, 1, 127, 1, 1, [] (double v) { return "VEL " + juce::String ((int) v) + "+"; },
               [this] (double v) { cfg.vlo = (int) v; if (cfg.vhi < cfg.vlo) { cfg.vhi = cfg.vlo; vhi.setValue (cfg.vhi); } });
        setup (vhi, 1, 127, 1, 127, [] (double v) { return "TO " + juce::String ((int) v); },
               [this] (double v) { cfg.vhi = (int) v; if (cfg.vlo > cfg.vhi) { cfg.vlo = cfg.vhi; vlo.setValue (cfg.vlo); } });
        setup (tr, -36, 36, 1, 0, [] (double v) { const int t = (int) v; return "TRANS " + juce::String (t > 0 ? "+" : "") + juce::String (t); }, [this] (double v) { cfg.transpose = (int) v; });
        setup (rev, 0, 2, 0.05, 1, [] (double v) { return "REV " + juce::String (juce::roundToInt (v * 100)) + "%"; }, [this] (double v) { cfg.rev = (float) v; });
        setup (cho, 0, 2, 0.05, 1, [] (double v) { return "CHO " + juce::String (juce::roundToInt (v * 100)) + "%"; }, [this] (double v) { cfg.cho = (float) v; });
        setChannelVisible (false);
    }

    void setChannelVisible (bool v) { ch.setVisible (v); chVisible = v; resized(); }

    void setCfg (const s90::PartCfg& c)
    {
        cfg = c;
        onBtn.setToggleState (c.on, juce::dontSendNotification);
        voiceBox.setSelectedId (c.patch + 1, juce::dontSendNotification);
        level.setValue (c.level); pan.setValue (c.pan); ch.setValue (c.channel);
        lo.setValue (c.lo); hi.setValue (c.hi); vlo.setValue (c.vlo); vhi.setValue (c.vhi);
        tr.setValue (c.transpose); rev.setValue (c.rev); cho.setValue (c.cho);
        const float a = c.on ? 1.f : 0.55f;
        for (juce::Component* k : { (juce::Component*) &voiceBox, (juce::Component*) &level, (juce::Component*) &pan, (juce::Component*) &ch,
                                    (juce::Component*) &lo, (juce::Component*) &hi, (juce::Component*) &vlo, (juce::Component*) &vhi,
                                    (juce::Component*) &tr, (juce::Component*) &rev, (juce::Component*) &cho })
            k->setAlpha (a);
    }

    void setTitle (const juce::String& t) { onBtn.setButtonText (t.isEmpty() ? juce::String ("ON") : t); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff23252b));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 6.f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 5);
        const int lineH = (r.getHeight() - 4) / 2;
        auto l1 = r.removeFromTop (lineH);
        r.removeFromTop (4);
        auto l2 = r;

        onBtn.setBounds (l1.removeFromLeft (46));
        l1.removeFromLeft (6);
        const int boxW = 84;
        pan.setBounds (l1.removeFromRight (boxW));
        l1.removeFromRight (4);
        level.setBounds (l1.removeFromRight (boxW));
        l1.removeFromRight (6);
        voiceBox.setBounds (l1);

        NumBox* row2[] = { &ch, &lo, &hi, &vlo, &vhi, &tr, &rev, &cho };
        int count = 0;
        for (auto* b : row2) if (b != &ch || chVisible) ++count;
        const int gap = 4;
        const int w = (l2.getWidth() - gap * (count - 1)) / juce::jmax (1, count);
        for (auto* b : row2)
        {
            if (b == &ch && ! chVisible) continue;
            b->setBounds (l2.removeFromLeft (w));
            l2.removeFromLeft (gap);
        }
    }

private:
    void push() { if (onChange) onChange (cfg); }

    s90::PartCfg cfg;
    bool chVisible = false;
    juce::TextButton onBtn;
    juce::ComboBox voiceBox;
    NumBox level, pan, ch, lo, hi, vlo, vhi, tr, rev, cho;
};
} // namespace s90ui
