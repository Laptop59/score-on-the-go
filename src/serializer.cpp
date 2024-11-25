#include "serializer.h"
#include <variant>
#include <cmath>

Serializer::Serializer()
{
    this->line   = 0;
}

std::string Serializer::saveBallfile(
    std::vector<Ball>& balls,
    std::vector<BpmChange>& bpmChanges
)
{
    auto ballIter = balls.begin();
    auto bpmChangesIter = bpmChanges.begin();
    std::string output = "";
    while (true)
    {
        // Specifies next in line.
        // `0`: None
        // `1`: Ball
        // `2`: BPM Change
        uint8_t next = 0;
        double leastBeat = INFINITY;
        // Prioritize BPM changes over balls.
        if (bpmChangesIter != bpmChanges.end())
        {
            if (bpmChangesIter->beat < leastBeat)
            {
                leastBeat = bpmChangesIter->beat;
                next = 2;
            }
        }
        // Now ball.
        if (ballIter != balls.end())
        {
            if (ballIter->at < leastBeat)
            {
                leastBeat = Ball::toBeats(ballIter->at);
                next = 1;
            }
        }
        // Finally use them.
        if (!next) break; // Break out of the loop.
        switch (next)
        {
            case 1:
                // Ball.
                output += std::to_string(Ball::toBeats(ballIter->at));
                output += ':';
                output += std::to_string(ballIter->x);
                if (ballIter->speed != 1)
                {
                    output += ':';
                    output += std::to_string(ballIter->speed);
                }
                output += '\n';
                ++ballIter;
                break;
            case 2:
                // BPM change.
                output += std::to_string(bpmChangesIter->beat);
                output += ':';
                output += std::to_string(bpmChangesIter->bpm);
                output += ":bpm\n";
                ++bpmChangesIter;
                break;
        }
    }
    if (!output.empty())
    {
        // Trim last \n character.
        if (output.at(output.size() - 1) == '\n')
            output.erase(output.size() - 1, 1);
    }
    return output;
}

SerializerResult Serializer::readBallfile(char *contents, size_t byteCount)
{
    // Start parsing.
    this->line   = 1;
    this->errors.clear();
    this->lines.clear();
    this->bpmChanges.clear();

    std::vector<Ball> balls;

    size_t i = 0;
    char ch;
    std::vector<char> charsInLine;
    while (i < byteCount)
    {
        ch = contents[i++];
        if (ch == '\n' && !charsInLine.empty())
        {
            this->lines.push_back(charsInLine);
            charsInLine.clear();
        }
        else if (ch == '\r' || ch == ' ')
            ;
        else
        {
            charsInLine.push_back(ch);
        }
    }
    if (!charsInLine.empty())
    {
        this->lines.push_back(charsInLine);
        charsInLine.clear();
    }

    // Read each line.
    for (auto line = this->lines.begin(); line < this->lines.end(); ++line)
    {
        std::vector<std::string> segments; // For creating notes.
        std::string currentSegment;
        for (auto ch = line->begin(); ch < line->end(); ++ch)
        {
            // ':' is our delimeter.
            if (*ch == ':')
            {
                segments.push_back(currentSegment);
                currentSegment.clear();
            }
            else
                currentSegment += *ch;
        }
        if (!currentSegment.empty())
            segments.push_back(currentSegment);

        // Check the number of segments.
        if (segments.size() == 1)
        {
            this->errors.push_back((SerializerError) {
                this->line,
                std::string("Invalid ball declaration. There must at least be beat:x.")
            });
        }
        else if (segments.size() > 1 && segments.size() <= 3)
        {
            double beat, x;
            double speed = 1.0f;
            if (!checkIsDouble(segments.at(0), beat))
            {
                this->errors.push_back((SerializerError) {
                    this->line,
                    std::string("Invalid beat number. Got: ") + segments.at(0)
                });
            }
            if (!checkIsDouble(segments.at(1), x))
            {
                this->errors.push_back((SerializerError) {
                    this->line,
                    std::string("Invalid abscicca (x-position) number. Got: ") + segments.at(1)
                });
            }
            if (segments.size() >= 3)
            {
                if (segments.at(2).compare(bpmString) == 0)
                {
                    // Add BPM change.
                    this->bpmChanges.push_back((BpmChange) {
                        beat,
                        x
                    });
                    continue;
                }
                else if (!checkIsDouble(segments.at(2), speed))
                {
                    this->errors.push_back((SerializerError) {
                        this->line,
                        std::string("Invalid speed number. (Note: If not specified, it defaults to 1.0) Got: ") + segments.at(2)
                    });
                }
            }
            balls.push_back(*new Ball(beat, speed, x));
        }
        else
        {
            this->errors.push_back((SerializerError) {
                this->line,
                std::string("Too many segments in one line.")
            });
        }

        this->line++;
    }

    if (errors.empty())
    {
        SerializerSuccess success {
            balls,
            this->bpmChanges
        };
        // Return errors.
        SerializerResult result {
            std::in_place_type<SerializerSuccess>,
            success
        };
        return result;
    }
    else
    {
        SerializerFailure failure {
            this->errors
        };
        // Return errors.
        SerializerResult result {
            std::in_place_type<SerializerFailure>,
            failure
        };
        return result;
    }
}