#ifndef SCORE_ON_THE_GO_BALL
#define SCORE_ON_THE_GO_BALL

#include "color_divisor.h"
#include <variant>
#include <vector>

// Type of a normal ball.
struct BallTypeNormal {};

// Point relative to the start of a hold ball.
struct BallTypeTailPoint
{
    float x;            // X-position of the tail point.
    minibeat minibeats; // Minibeats relative to the tailed ball (e.g. 500 + 7500 (hold) -> 8000)
};

// Type of a node of a hold ball.
struct BallTypeHoldNode {};

// Type of a hold ball.
struct BallTypeHold
{
    std::vector<BallTypeTailPoint> points;
};

// Represents the type of a ball.
using BallType = std::variant<
    BallTypeNormal,
    BallTypeHold,
    BallTypeHoldNode
>;

// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Represents a ball in gameplay
class Ball
{
    public:
        minibeat at;   // Minibeat at which it hits the paddle.
        float speed;   // Speed at which it hits the paddle.
        float x;       // X-position of the ball at which it hits the paddle.
        BallType type; // Type of the ball.

        // Creates a normal ball with a minibeat.
        Ball(minibeat at, float speed, float x);

        // Creates a normal ball with a double, converted to a minibeat.
        Ball(double at, float speed, float x);

        // Converts beat floating number to its nearest minibeat.
        static minibeat toMinibeats(double beat);

        // Converts a minibeat to its beat floating number.
        static double toBeats(minibeat minibeat);

        // Get color divisor of the ball.
        ColorDivisor getColorDivisor()
        {
            return ColorDivisor::getColorDivisor(at);
        }

        bool operator<(const Ball& other) const
        {
            return at < other.at;
        }

        bool operator>(const Ball& other) const
        {
            return at > other.at;
        }

        bool operator<=(const Ball& other) const
        {
            return at <= other.at;
        }

        bool operator>=(const Ball& other) const
        {
            return at >= other.at;
        }

        bool operator<(const int& other) const
        {
            return at < other;
        }

        bool operator>(const int& other) const
        {
            return at > other;
        }

        bool operator<=(const int& other) const
        {
            return at <= other;
        }

        bool operator>=(const int& other) const
        {
            return at >= other;
        }
};

#endif