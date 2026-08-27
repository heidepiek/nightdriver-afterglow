#pragma once

//+--------------------------------------------------------------------------
//
// File: PatternFlipClock.h
//
// NightDriverStrip custom effect
//
// Description:
//
//   Split-flap ("flip clock") style HH:MM clock for HUB75 matrices.
//   Each digit is drawn as a big 7-segment block. When a digit changes,
//   its top half is clipped down to the horizontal center line ("hinge")
//   and back up again, simulating the classic mechanical flap flip.
//   The bottom half updates to the new digit immediately, as it would on
//   a real split-flap display.
//
//   Tuned for 64x32 HUB75 panels but the layout is computed from
//   g()->kMatrixWidth / g()->kMatrixHeight, so it will adapt to other
//   matrix sizes too (very small panels will just look cramped).
//
//---------------------------------------------------------------------------

#include <ctime>
#include <cstdint>
#include <algorithm>
#include <Arduino.h>

#include "effects.h"

class PatternFlipClock : public EffectWithId<PatternFlipClock>
{
  private:

    static constexpr int NUM_DIGITS = 4;

    static constexpr uint8_t SEG_A = 0x01;
    static constexpr uint8_t SEG_B = 0x02;
    static constexpr uint8_t SEG_C = 0x04;
    static constexpr uint8_t SEG_D = 0x08;
    static constexpr uint8_t SEG_E = 0x10;
    static constexpr uint8_t SEG_F = 0x20;
    static constexpr uint8_t SEG_G = 0x40;

    int   _curDigit[NUM_DIGITS]      = { 0, 0, 0, 0 };
    int   _nextDigit[NUM_DIGITS]     = { 0, 0, 0, 0 };
    bool  _flipping[NUM_DIGITS]      = { false, false, false, false };
    float _flipProgress[NUM_DIGITS]  = { 0, 0, 0, 0 };

    static constexpr float FLIP_SPEED = 0.03f;
    static constexpr int SEG_THICKNESS = 2; 
    static constexpr int HINGE_GAP     = 1;

    static uint8_t DigitSegments(int digit)
    {
        switch (digit)
        {
            case 0: return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
            case 1: return SEG_B | SEG_C;
            case 2: return SEG_A | SEG_B | SEG_G | SEG_E | SEG_D;
            case 3: return SEG_A | SEG_B | SEG_G | SEG_C | SEG_D;
            case 4: return SEG_F | SEG_G | SEG_B | SEG_C;
            case 5: return SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
            case 6: return SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D;
            case 7: return SEG_A | SEG_B | SEG_C;
            case 8: return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
            case 9: return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
            default: return 0;
        }
    }

    uint16_t Color(const CRGB & c)
    {
        return g()->to16bit(c);
    }

    void FillRectClipped(int x, int y, int w, int h, uint16_t color, int clipTop, int clipBottom)
    {
        int y1 = std::max(y, clipTop);
        int y2 = std::min(y + h, clipBottom);
        if (w <= 0 || y2 <= y1)
            return;
        g()->fillRect(x, y1, w, y2 - y1, color);
    }

    void DrawSegments(int x, int y, int w, int h, int digit, uint16_t color,
                       int clipTop = INT32_MIN, int clipBottom = INT32_MAX)
    {
        if (digit < 0 || digit > 9)
            return;

        const uint8_t segs = DigitSegments(digit);
        const int t   = SEG_THICKNESS;
        const int gp  = HINGE_GAP;
        const int midY = y + h / 2;
        const int vLen = h / 2 - t - gp;

        if (vLen <= 0)
            return;

        x += 2;
        y += 2;
        w -= 4;
        h -= 4;

        if (segs & SEG_A) FillRectClipped(x + t,     y,               w - 2 * t, t,    color, clipTop, clipBottom);
        if (segs & SEG_D) FillRectClipped(x + t,     y + h - t,       w - 2 * t, t,    color, clipTop, clipBottom);

        if (segs & SEG_F) FillRectClipped(x,         y + t,           t,         vLen, color, clipTop, clipBottom);
        if (segs & SEG_B) FillRectClipped(x + w - t, y + t,           t,         vLen, color, clipTop, clipBottom);

        if (segs & SEG_E) FillRectClipped(x,         midY + gp,       t,         vLen, color, clipTop, clipBottom);
        if (segs & SEG_C) FillRectClipped(x + w - t, midY + gp,       t,         vLen, color, clipTop, clipBottom);

        if (segs & SEG_G) {
            if (clipTop < midY) { 
                FillRectClipped(x + t, midY - t, w - 2 * t, t - 1, color, clipTop, midY - 1);
            }
            if (clipBottom > midY) { 
                FillRectClipped(x + t, midY + 1, w - 2 * t, t - 1, color, midY + 1, clipBottom);
            }
        }
    }

    void DrawHinge(int x, int midY, int w)
    {
        uint16_t frameWhite = Color(CRGB(255, 255, 255));

        // De gitzwarte snede snijdt nu prachtig door het grijze vlak heen!
        g()->drawLine(x + 1, midY, x + w - 2, midY, BLACK16);

        // De mechanische pinnetjes aan de zijkant
        g()->drawPixel(x, midY, frameWhite);
        g()->drawPixel(x + w - 1, midY, frameWhite);
    }

    void DrawFlipDigit(int x, int y, int w, int h, int idx)
    {
        const uint16_t fg       = Color(CRGB(255, 255, 255)); // Helder wit cijfer
        const uint16_t fgDim    = Color(CRGB(40, 40, 40));    // Veel donkerder grijs voor de vallende flap schaduw
        const uint16_t white    = Color(CRGB(255, 255, 255)); // Helder wit buitenkader
        
        // AANGEPAST: Stevige middengrijze achtergrond zodat de flapjes écht grijs worden
        const uint16_t flapBg   = Color(CRGB(90, 90, 90));    

        const int midY = y + h / 2;

        // NIEUW: Vul de binnenkant van het kader met de lichtgrijze kleur
        // (we laten de buitenste rand vrij voor het witte kader)
        g()->fillRect(x + 1, y + 1, w - 2, h - 2, flapBg);

        // Teken het witte buitenkader eromheen
        g()->drawRect(x, y, w, h, white);

        if (!_flipping[idx])
        {
            DrawSegments(x, y, w, h, _curDigit[idx], fg);
            DrawHinge(x, midY, w);
            return;
        }

        // --- NATUURKUNDIGE VERSNELLING (EASING) ---
        float linearP = _flipProgress[idx];
        float p = linearP * linearP; 

        const int halfH = h / 2;

        if (p < 0.5f)
        {
            // FASE 1: Oude bovenkant valt naar beneden
            float stageP = p / 0.5f; 
            const int curH = static_cast<int>(halfH * (1.0f - stageP));
            
            if (curH > 0)
                DrawSegments(x, y, w, h, _curDigit[idx], fg, midY - curH, midY);
                
            // Ondergrond toont alvast het nieuwe cijfer
            DrawSegments(x, y, w, h, _nextDigit[idx], fg, midY, y + h);
        }
        else
        {
            // FASE 2: Flap valt over de onderkant heen richting de bodem
            float stageP = (p - 0.5f) / 0.5f;
            const int curH = static_cast<int>(halfH * stageP);
            
            // Bovenzijde is nu definitief het nieuwe cijfer
            DrawSegments(x, y, w, h, _nextDigit[idx], fg, y, midY);
            
            // Onderzijde achtergrond (nieuwe cijfer)
            DrawSegments(x, y, w, h, _nextDigit[idx], fg, midY, y + h);
            
            // De vallende schaduwflap die eroverheen klettert
            if (curH > 0)
                DrawSegments(x, y, w, h, _curDigit[idx], fgDim, midY, midY + curH);
        }

        DrawHinge(x, midY, w);
    }

    void DrawColon(int x, int y, int h, bool visible)
    {
        if (!visible)
            return;

        const uint16_t color = Color(CRGB(255, 255, 255));
        const int midY = y + h / 2;
        
        g()->fillRect(x, midY - 5, 2, 2, color);
        g()->fillRect(x, y + h - (midY - 5) - 2, 2, 2, color);
    }

  public:

    PatternFlipClock() : EffectWithId<PatternFlipClock>("Flip Clock") {}
    PatternFlipClock(const JsonObjectConst & jsonObject) : EffectWithId<PatternFlipClock>(jsonObject) {}

    bool ShouldShowTitle() const { return false; }
    size_t DesiredFramesPerSecond() const override { return 30; }
    bool RequiresDoubleBuffering() const override { return false; }

    void Draw() override
    {
        g()->fillScreen(BLACK16);

        const int digitW = 13;
        const int digitH = 26;
        const int y0     = 3; 

        const int xPos[NUM_DIGITS] = { 1, 15, 36, 50 };
        const int xColon           = 31;

        const time_t rawNow = time(nullptr);
        const tm * localNow = localtime(&rawNow);

        const int newDigits[NUM_DIGITS] =
        {
            localNow->tm_hour / 10,
            localNow->tm_hour % 10,
            localNow->tm_min / 10,
            localNow->tm_min % 10
        };

        for (int i = 0; i < NUM_DIGITS; i++)
        {
            if (_flipping[i])
            {
                _flipProgress[i] += FLIP_SPEED;
                if (_flipProgress[i] >= 1.0f)
                {
                    _curDigit[i] = _nextDigit[i];
                    _flipping[i] = false;
                }
            }
            else if (newDigits[i] != _curDigit[i])
            {
                _nextDigit[i]     = newDigits[i];
                _flipping[i]      = true;
                _flipProgress[i]  = 0.0f;
            }
        }

        for (int i = 0; i < NUM_DIGITS; i++)
        {
            DrawFlipDigit(xPos[i], y0, digitW, digitH, i);
        }

        bool colonVisible = (millis() % 1000) < 500;
        DrawColon(xColon, y0, digitH, colonVisible);
    }
};
