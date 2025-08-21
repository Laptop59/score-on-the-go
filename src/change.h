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

#endif