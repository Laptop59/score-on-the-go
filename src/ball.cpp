#include "ball.h"
#include <cmath>

Ball::Ball(minibeat at, float speed, float x)
{
    this->at = at;
    this->speed = speed;
    this->x = x;
    BallType ballType(BallTypeNormal {});
    this->type = ballType;
}

Ball::Ball(double at, float speed, float x)
{
    this->at = toMinibeats(at);
    this->speed = speed;
    this->x = x;
    BallType ballType(BallTypeNormal {});
    this->type = ballType;
}

double Ball::toBeats(minibeat minibeat)
{
    return ((double) minibeat) / MINIBEATS_PER_BEAT;
}

minibeat Ball::toMinibeats(double beat)
{
    if (beat <= 0) return 0;
    return std::round(beat * MINIBEATS_PER_BEAT);
}