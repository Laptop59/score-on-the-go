#ifndef SCORE_ON_THE_GO_CHANGE
#define SCORE_ON_THE_GO_CHANGE

// Defines a BPM change.
struct BpmChange
{
    double beat;
    double bpm;

    friend bool operator<(const BpmChange lhs, const BpmChange rhs)
    {
        return lhs.beat < rhs.beat;
    }

    friend bool operator>(const BpmChange lhs, const BpmChange rhs)
    {
        return lhs.beat > rhs.beat;
    }

    friend bool operator<=(const BpmChange lhs, const BpmChange rhs)
    {
        return lhs.beat <= rhs.beat;
    }

    friend bool operator>=(const BpmChange lhs, const BpmChange rhs)
    {
        return lhs.beat >= rhs.beat;
    }
};

// Defines a Paddle Width change.
struct PaddleWidthChange
{
    double beat;
    float width;

    friend bool operator<(const PaddleWidthChange lhs, const PaddleWidthChange rhs)
    {
        return lhs.beat < rhs.beat;
    }

    friend bool operator>(const PaddleWidthChange lhs, const PaddleWidthChange rhs)
    {
        return lhs.beat > rhs.beat;
    }

    friend bool operator<=(const PaddleWidthChange lhs, const PaddleWidthChange rhs)
    {
        return lhs.beat <= rhs.beat;
    }

    friend bool operator>=(const PaddleWidthChange lhs, const PaddleWidthChange rhs)
    {
        return lhs.beat >= rhs.beat;
    }
};

// Defines a Paddle Speed change.
struct PaddleSpeedChange
{
    double beat;
    float speed;

    friend bool operator<(const PaddleSpeedChange lhs, const PaddleSpeedChange rhs)
    {
        return lhs.beat < rhs.beat;
    }

    friend bool operator>(const PaddleSpeedChange lhs, const PaddleSpeedChange rhs)
    {
        return lhs.beat > rhs.beat;
    }

    friend bool operator<=(const PaddleSpeedChange lhs, const PaddleSpeedChange rhs)
    {
        return lhs.beat <= rhs.beat;
    }

    friend bool operator>=(const PaddleSpeedChange lhs, const PaddleSpeedChange rhs)
    {
        return lhs.beat >= rhs.beat;
    }
};

// Defines a Dual change.
struct PaddleDualChange
{
    double beat;
    bool enabled;

    friend bool operator<(const PaddleDualChange lhs, const PaddleDualChange rhs)
    {
        return lhs.beat < rhs.beat;
    }

    friend bool operator>(const PaddleDualChange lhs, const PaddleDualChange rhs)
    {
        return lhs.beat > rhs.beat;
    }

    friend bool operator<=(const PaddleDualChange lhs, const PaddleDualChange rhs)
    {
        return lhs.beat <= rhs.beat;
    }

    friend bool operator>=(const PaddleDualChange lhs, const PaddleDualChange rhs)
    {
        return lhs.beat >= rhs.beat;
    }
};

// Defines a Command.
struct Command
{
    double beat;
    std::string action;

    friend bool operator<(const Command lhs, const Command rhs)
    {
        return lhs.beat < rhs.beat;
    }

    friend bool operator>(const Command lhs, const Command rhs)
    {
        return lhs.beat > rhs.beat;
    }

    friend bool operator<=(const Command lhs, const Command rhs)
    {
        return lhs.beat <= rhs.beat;
    }

    friend bool operator>=(const Command lhs, const Command rhs)
    {
        return lhs.beat >= rhs.beat;
    }
};

#endif