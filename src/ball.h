#ifndef SCORE_ON_THE_GO_BALL
#define SCORE_ON_THE_GO_BALL

#include "color_divisor.h"

// Represents a ball in gameplay
class Ball
{
    public:
        minibeat at; // Minibeat at which it hits the paddle.
        float speed; // Speed at which it hits the paddle.
        float x;     // X-position of the ball at which it hits the paddle.

        // Creates a ball with a minibeat.
        Ball(minibeat at, float speed, float x);

        // Creates a ball with a double, converted to a minibeat.
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
};

#endif