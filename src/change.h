#ifndef SCORE_ON_THE_GO_CHANGE
#define SCORE_ON_THE_GO_CHANGE

// Defines a BPM change.
struct BpmChange
{
    double beat;
    double bpm;

    bool operator<(BpmChange other)
    {
        return beat < other.beat;
    }

    bool operator>(BpmChange other)
    {
        return beat > other.beat;
    }

    bool operator<=(BpmChange other)
    {
        return beat <= other.beat;
    }

    bool operator>=(BpmChange other)
    {
        return beat >= other.beat;
    }
};

// Defines a Paddle Width change.
struct PaddleWidthChange
{
    double beat;
    float width;

    bool operator<(PaddleWidthChange other)
    {
        return beat < other.beat;
    }

    bool operator>(PaddleWidthChange other)
    {
        return beat > other.beat;
    }

    bool operator<=(PaddleWidthChange other)
    {
        return beat <= other.beat;
    }

    bool operator>=(PaddleWidthChange other)
    {
        return beat >= other.beat;
    }
};

// Defines a Paddle Speed change.
struct PaddleSpeedChange
{
    double beat;
    float speed;

    bool operator<(PaddleSpeedChange other)
    {
        return beat < other.beat;
    }

    bool operator>(PaddleSpeedChange other)
    {
        return beat > other.beat;
    }

    bool operator<=(PaddleSpeedChange other)
    {
        return beat <= other.beat;
    }

    bool operator>=(PaddleSpeedChange other)
    {
        return beat >= other.beat;
    }
};

// Defines a Dual change.
struct PaddleDualChange
{
    double beat;
    bool enabled;

    bool operator<(PaddleDualChange other)
    {
        return beat < other.beat;
    }

    bool operator>(PaddleDualChange other)
    {
        return beat > other.beat;
    }

    bool operator<=(PaddleDualChange other)
    {
        return beat <= other.beat;
    }

    bool operator>=(PaddleDualChange other)
    {
        return beat >= other.beat;
    }
};

// Defines a Command.
struct Command
{
    double beat;
    std::string action;

    bool operator<(Command other)
    {
        return beat < other.beat;
    }

    bool operator>(Command other)
    {
        return beat > other.beat;
    }

    bool operator<=(Command other)
    {
        return beat <= other.beat;
    }

    bool operator>=(Command other)
    {
        return beat >= other.beat;
    }
};

#endif