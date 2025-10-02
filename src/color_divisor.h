#ifndef SCORE_ON_THE_GO_COLOR_DIVISOR
#define SCORE_ON_THE_GO_COLOR_DIVISOR

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>

// Unsigned type for minibeat (Note: 48 minibeats = 1 beat)
using minibeat = uint32_t;

// Signed type for minibeat (Note: 48 minibeats = 1 beat)
using sminibeat = int32_t;

#define MINIBEATS_PER_BEAT 48

// Specifies a color beat divisor.
class ColorDivisor
{
    public:
        enum Value : uint8_t
        {
            DIVISOR_INVALID =  0,
            DIVISOR_4TH     =  1,
            DIVISOR_8TH     =  2,
            DIVISOR_12TH    =  3,
            DIVISOR_16TH    =  4,
            DIVISOR_24TH    =  5,
            DIVISOR_32ND    =  6,
            DIVISOR_48TH    =  7,
            DIVISOR_64TH    =  8,
            DIVISOR_192ND   =  9,
        };

        ColorDivisor() = default;
        constexpr ColorDivisor(Value colorDivisor) : value(colorDivisor) { }

        constexpr operator Value() const { return value; }
        explicit operator bool() const { return value != DIVISOR_INVALID; }

        constexpr operator int() const { return (uint8_t) value; }

        constexpr bool operator==(ColorDivisor a) const { return value == a.value; }
        constexpr bool operator!=(ColorDivisor a) const { return value != a.value; }

        // Gets the color from a minibeat.
        static ColorDivisor getColorDivisor(minibeat miniBeats)
        {
            // divisor can be used as a boolean; if invalid it is false.
            for (ColorDivisor divisor = ColorDivisor::DIVISOR_4TH; divisor; divisor = divisor.getNext())
            {
                if (miniBeats % divisor.getWorth() == 0) return divisor;
            }
            return ColorDivisor::DIVISOR_192ND;
        }
        
        // Gets the protraying color of a divisor.
        constexpr SDL_Color getProtrayingColor()
        {
            const SDL_Color colors[] = {
                (SDL_Color) { 0x00u, 0x00u, 0x00u, 0xFFu }, // Black (INVALID)
                (SDL_Color) { 0xFFu, 0x00u, 0x00u, 0xFFu }, // Red
                (SDL_Color) { 0x3Fu, 0x00u, 0xFFu, 0xFFu }, // Blue
                (SDL_Color) { 0xFFu, 0x00u, 0xFFu, 0xFFu }, // Magenta
                (SDL_Color) { 0xFFu, 0xFFu, 0x00u, 0xFFu }, // Yellow
                (SDL_Color) { 0x7Fu, 0x00u, 0xFFu, 0xFFu }, // Purple
                (SDL_Color) { 0xFFu, 0x7Fu, 0x00u, 0xFFu }, // Orange
                (SDL_Color) { 0x00u, 0xFFu, 0xFFu, 0xFFu }, // Cyan
                (SDL_Color) { 0xFFu, 0xFFu, 0xFFu, 0xFFu }, // White
                (SDL_Color) { 0xFFu, 0xFFu, 0xFFu, 0xFFu }, // White
            };
            return colors[getColor() + 1];
        }

        // Gets the text color for displaying an editor ball.
        constexpr SDL_Color getTextColor()
        {
            SDL_Color baseColor = getProtrayingColor();
            // Divide RGB by 2 (>> 1)
            baseColor.r >>= 1;
            baseColor.g >>= 1;
            baseColor.b >>= 1;
            return baseColor;
        }

        constexpr ColorDivisor getNext() const {
            if (value >= DIVISOR_192ND || value <= DIVISOR_INVALID) return DIVISOR_INVALID;
            return ColorDivisor((Value) (value + 1));
        }

        constexpr ColorDivisor getPrevious() const {
            if (value > DIVISOR_192ND || value <= DIVISOR_4TH) return DIVISOR_INVALID;
            return ColorDivisor((Value) (value - 1));
        }

        // Get its worth in minibeats (48 minibeats = 1 beat)
        constexpr minibeat getWorth() const {
            minibeat WORTH[] = { 0, 48, 24, 16, 12, 8, 6, 4, 3, 1 };
            return WORTH[value];
        }

        // Get the `th` number (`4`th, `8`th, etc.)
        constexpr unsigned int getTh() const {
            unsigned int TH[] = { 0, 4, 8, 12, 16, 24, 32, 48, 64, 192 };
            return TH[value];
        }

        // Get the `th` string (4th, 8th, etc.)
        std::string getThString() const {
            unsigned int ordinal = getTh();
            std::string str = std::to_string(ordinal);
            unsigned int belowcent = ordinal % 100U;
            if (belowcent < 11 || belowcent > 13)
            {
                if (belowcent % 10 == 1)
                    return str + "st";
                else if (belowcent % 10 == 2)
                    return str + "nd";
                else if (belowcent % 10 == 3)
                    return str + "rd";
            }
            return str + "th";
        }

        // Get the color index of the divisor.
        constexpr int getColor() const {
            int COLOR[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 7 };
            return COLOR[value];
        }

    private:
        Value value;
};

inline minibeat operator ""_mb(unsigned long long value)
{
    return static_cast<minibeat>(value);
}

inline sminibeat operator ""_smb(unsigned long long value)
{
    return static_cast<sminibeat>(value);
}


#endif