#ifndef SCORE_ON_THE_GO_BPM
#define SCORE_ON_THE_GO_BPM

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

#endif