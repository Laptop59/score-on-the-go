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

namespace ChangeColors
{
    // The editor color for lines of BPM changes.
    const SDL_Color BPM_LINE = SDL_Color {0x9Fu, 0x9Fu, 0x4Fu, 0xFF};
    // The editor color for text of BPM changes.
    const SDL_Color BPM_TEXT = SDL_Color {0xFFu, 0xFFu, 0x7Fu, 0xFF};

    // The editor color for lines of Paddle Width changes.
    const SDL_Color PADDLE_WIDTH_LINE = SDL_Color {0x2Fu, 0xFFu, 0xFFu, 0xFF};
    // The editor color for text of Paddle Width changes.
    const SDL_Color PADDLE_WIDTH_TEXT = SDL_Color {0x5Fu, 0xFFu, 0xFFu, 0xFF};

    // The editor color for lines of Paddle Speed changes.
    const SDL_Color PADDLE_SPEED_LINE = SDL_Color {0x4Fu, 0x6Fu, 0xAFu, 0xFF};
    // The editor color for text of Paddle Speed changes.
    const SDL_Color PADDLE_SPEED_TEXT = SDL_Color {0x7Fu, 0x9Fu, 0xEFu, 0xFF};

    // The editor color for lines of Paddle Dual changes.
    const SDL_Color PADDLE_DUAL_LINE = SDL_Color {0x6Fu, 0x1Fu, 0x6Fu, 0xFF};
    // The editor color for text of Paddle Dual changes.
    const SDL_Color PADDLE_DUAL_TEXT = SDL_Color {0xCFu, 0x5Fu, 0xCFu, 0xFF};

    // The editor color for lines of BG command changes.
    const SDL_Color BG_COMMAND_LINE = SDL_Color {0x3Fu, 0xFFu, 0x3Fu, 0xFF};
    // The editor color for text of BG command changes.
    const SDL_Color BG_COMMAND_TEXT = SDL_Color {0x5Fu, 0xFFu, 0x5Fu, 0xFF};
}

#endif