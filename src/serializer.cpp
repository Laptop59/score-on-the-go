#include "serializer.h"
#include <variant>
#include <cmath>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

Serializer::Serializer()
{
    this->line   = 0;
}

std::vector<BallTypeTailPoint>* Serializer::getPointsFromType(BallType& type)
{
    std::vector<BallTypeTailPoint>* points = nullptr;
    // HOLDs
    if (std::holds_alternative<BallTypeHold>(type))
    {
        BallTypeHold& data = std::get<BallTypeHold>(type);
        points = &data.points;
    }
    // PITS
    if (std::holds_alternative<BallTypePit>(type))
    {
        BallTypePit& data = std::get<BallTypePit>(type);
        points = &data.points;
    }
    return points;
}

std::string Serializer::saveBallfile(
    std::vector<Ball>& balls,
    std::vector<BpmChange>& bpmChanges,
    std::vector<PaddleWidthChange>& paddleWidthChanges,
    std::vector<PaddleSpeedChange>& paddleSpeedChanges,
    std::vector<PaddleDualChange>& paddleDualChanges
)
{
    auto ballIter = balls.begin();
    auto bpmChangesIter = bpmChanges.begin();
    auto paddleWidthChangesIter = paddleWidthChanges.begin();
    auto paddleSpeedChangesIter = paddleSpeedChanges.begin();
    auto paddleDualChangesIter = paddleDualChanges.begin();
    std::string output = "";
    while (true)
    {
        // Specifies next in line.
        // `0`: None
        // `1`: Ball
        // `2`: BPM Change
        // `3`: Paddle width change
        // `4`: Paddle speed change
        // `5`: Paddle dual change
        uint8_t next = 0;
        double leastBeat = INFINITY;
        // Prioritize Paddle Dual changes over the below changes.
        if (paddleDualChangesIter != paddleDualChanges.end())
        {
            if (paddleDualChangesIter->beat < leastBeat)
            {
                leastBeat = paddleDualChangesIter->beat;
                next = 5;
            }
        }
        // Prioritize Paddle Speed changes over the below changes.
        if (paddleSpeedChangesIter != paddleSpeedChanges.end())
        {
            if (paddleSpeedChangesIter->beat < leastBeat)
            {
                leastBeat = paddleSpeedChangesIter->beat;
                next = 4;
            }
        }
        // Prioritize Paddle Width changes over BPM changes.
        if (paddleWidthChangesIter != paddleWidthChanges.end())
        {
            if (paddleWidthChangesIter->beat < leastBeat)
            {
                leastBeat = paddleWidthChangesIter->beat;
                next = 3;
            }
        }
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
            if (Ball::toBeats(ballIter->at) < leastBeat)
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
                {
                    // Ball.
                    char extra = '\0';
                    if (std::holds_alternative<BallTypeHold>(ballIter->type))
                        extra = 'h';
                    else if (std::holds_alternative<BallTypePit>(ballIter->type))
                        extra = 'p';
                    else if (std::holds_alternative<BallTypeMine>(ballIter->type))
                        extra = 'm';
                    else if (std::holds_alternative<BallTypeSquare>(ballIter->type))
                        extra = 's';
                    else if (std::holds_alternative<BallTypeBouncy>(ballIter->type))
                        extra = 'b';
                    if (extra)
                        output += extra;
                    output += std::to_string(Ball::toBeats(ballIter->at));
                    output += ':';
                    output += std::to_string(ballIter->x);
                    std::vector<BallTypeTailPoint>* points =
                    getPointsFromType(ballIter->type);
                    // We only want to omit speed if ball is not hold/pit
                    // (for convenience)
                    if (ballIter->speed != 1 || points != nullptr || std::holds_alternative<BallTypeBouncy>(ballIter->type))
                    {
                        output += ':';
                        output += std::to_string(ballIter->speed);
                    }
                    if (std::holds_alternative<BallTypeBouncy>(ballIter->type)) {
                        BallTypeBouncy data = std::get<BallTypeBouncy>(ballIter->type);
                        output += ":";
                        output += std::to_string(data.respawns);
                        output += ":";
                        output += std::to_string(data.interval);
                    }
                    if (points != nullptr)
                    {    
                        for (auto it = points->begin(); it != points->end(); ++it)
                        {
                            output += ":";
                            output += std::to_string(Ball::toBeats(it->minibeats));
                            output += ":";
                            output += std::to_string(it->x);
                        }
                    }
                    output += '\n';
                    ++ballIter;
                }
                break;
            case 2:
                // BPM change.
                output += std::to_string(bpmChangesIter->beat);
                output += ':';
                output += std::to_string(bpmChangesIter->bpm);
                output += ":bpm\n";
                ++bpmChangesIter;
                break;
            case 3:
                // PW change.
                output += std::to_string(paddleWidthChangesIter->beat);
                output += ':';
                output += std::to_string(paddleWidthChangesIter->width);
                output += ":pw\n";
                ++paddleWidthChangesIter;
                break;
            case 4:
                // PS change.
                output += std::to_string(paddleSpeedChangesIter->beat);
                output += ':';
                output += std::to_string(paddleSpeedChangesIter->speed);
                output += ":ps\n";
                ++paddleSpeedChangesIter;
                break;
            case 5:
                // Dual mode switch.
                output += std::to_string(paddleDualChangesIter->beat);
                output += ':';
                output += paddleDualChangesIter->enabled ? '1' : '0';
                output += ":dual\n";
                ++paddleDualChangesIter;
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

SerializerResult Serializer::readBallfile(char *contents, size_t byteCount, bool startsWithMusicOffset)
{
    // Start parsing.
    this->line = 1;
    this->errors.clear();
    this->lines.clear();
    this->bpmChanges.clear();
    this->paddleWidthChanges.clear();
    this->paddleSpeedChanges.clear();
    this->paddleDualChanges.clear();

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
    bool first = startsWithMusicOffset;
    double musicOffset = 0;

    for (auto line = this->lines.begin(); line < this->lines.end(); ++line)
    {
        if (line->empty()) continue;
        if (first)
        {
            // Set the music offset.
            std::string strLine = "";
            for (int c = 0; c < line->size(); c++) strLine += line->at(c);
            if (!checkIsDouble(strLine, musicOffset))
            {
                this->errors.push_back((SerializerError) {
                    this->line,
                    std::string("Invalid music offset. Got: ") + strLine
                });
            }
            first = false;
            this->line++;
            continue;
        }
        std::vector<std::string> segments; // For creating notes.
        std::string currentSegment;
        auto ch = line->begin();
        BallType type(BallTypeNormal {});
        bool gotoNextChar = true;
        if (*ch == 'h')
            type = BallTypeHold {};
        else if (*ch == 'p')
            type = BallTypePit {};
        else if (*ch == 'm')
            type = BallTypeMine {};
        else if (*ch == 's')
            type = BallTypeSquare {};
        else if (*ch == 'b')
            type = BallTypeBouncy {};
        else
            gotoNextChar = false;
        if (gotoNextChar) ++ch;
        for (; ch != line->end(); ++ch)
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
                std::string("Invalid ball declaration. There must at least be <beat>:<x>.")
            });
        }
        else if (segments.size() > 1)
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
                else if (segments.at(2).compare(paddleWidthString) == 0)
                {
                    // Add paddle width change.
                    this->paddleWidthChanges.push_back((PaddleWidthChange) {
                        beat,
                        (float) x
                    });
                    continue;
                }
                else if (segments.at(2).compare(paddleSpeedString) == 0)
                {
                    // Add paddle width change.
                    this->paddleSpeedChanges.push_back((PaddleSpeedChange) {
                        beat,
                        (float) x
                    });
                    continue;
                }
                else if (segments.at(2).compare(paddleDualString) == 0)
                {
                    // Add dual change.
                    this->paddleDualChanges.push_back((PaddleDualChange) {
                        beat,
                        x > 0
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
                else if (std::holds_alternative<BallTypeBouncy>(type) )
                {
                    if (segments.size() != 5)
                    {
                        this->errors.push_back((SerializerError) {
                            this->line,
                            std::string("Bouncy balls must only have 5 segments, but instead got ") + std::to_string(segments.size())
                        });
                        continue;
                    }
                    BallTypeBouncy& data = std::get<BallTypeBouncy>(type);
                    data.lastHit = -INFINITY;
                    // <beat>:<x>:<speed>:<respawns>:<interval>
                    // <speed> is not optional
                    auto reverseIter = segments.rbegin();
                    std::string intervalString = *reverseIter++;
                    std::string respawnsString = *reverseIter;
                    if (!checkIsUnsignedInt(respawnsString, data.respawns)) {
                        this->errors.push_back((SerializerError) {
                            this->line,
                            std::string("Invalid respawns: ") + respawnsString
                        });
                    }
                    if (!checkIsUnsignedMinibeat(intervalString, data.interval)) {
                        this->errors.push_back((SerializerError) {
                            this->line,
                            std::string("Invalid interval: ") + intervalString
                        });
                    }
                }
                else if (std::holds_alternative<BallTypeHold>(type) ||
                    std::holds_alternative<BallTypePit>(type))
                {
                    // These balls are GUARANTEED to have speed, so go to fourth, fifth, ...
                    auto segment = segments.begin() + 3;
                    std::vector<BallTypeTailPoint> points;
                    while (true)
                    {
                        if (segment == segments.end()) break;
                        std::string beatStr = *segment++;
                        if (segment == segments.end()) break;
                        std::string xStr = *segment++;
                        double beat, x;
                        if (!checkIsDouble(beatStr, beat))
                        {
                            this->errors.push_back((SerializerError) {
                                this->line,
                                std::string("Invalid beat number. Got: ") + beatStr
                            });
                        }
                        if (!checkIsDouble(xStr, x))
                        {
                            this->errors.push_back((SerializerError) {
                                this->line,
                                std::string("Invalid abscicca (x-position) number. Got: ") + xStr
                            });
                        }
                        minibeat miniBeat = Ball::toMinibeats(beat);
                        points.push_back(BallTypeTailPoint {
                            (float) x, miniBeat
                        });
                    }
                    if (std::holds_alternative<BallTypeHold>(type))
                    {
                        BallTypeHold subtype = std::get<BallTypeHold>(type);
                        subtype.points = points;
                        type = subtype;
                    }
                    else if (std::holds_alternative<BallTypePit>(type))
                    {
                        BallTypePit subtype = std::get<BallTypePit>(type);
                        subtype.points = points;
                        type = subtype;
                    }
                }
            }
            Ball ball(beat, speed, x);
            ball.type = type;
            balls.push_back(ball);
        }

        this->line++;
    }

    if (errors.empty())
    {
        SerializerSuccess success {
            balls,
            this->bpmChanges,
            this->paddleWidthChanges,
            this->paddleSpeedChanges,
            this->paddleDualChanges,
            (float) musicOffset
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

std::string Serializer::writeCommands(std::vector<Command> &commands)
{
    std::string result;

    for (Command& command : commands)
    {
        // Commands separated by a comma are 'considered' separate.
        double beat = command.beat;
        std::string str = command.action;

        std::vector<std::string> actions = splitStringStream(str, ',');
        for (std::string& action : actions)
        {
            result += std::to_string(beat) + ":" + action + "\n";
        }
    }

    if (!result.empty())
    {
        // Trim last \n character.
        if (result.at(result.size() - 1) == '\n')
            result.erase(result.size() - 1, 1);
    }
    return result;
}

std::vector<Command> Serializer::readCommands(char *contents, size_t byteCount)
{
    std::vector<Command> result;
    readLines(contents, byteCount);

    double lastBeat = -INFINITY;

    for (std::vector<char>& line : lines)
    {
        size_t firstColonPos = 0;
        for (char c : line)
        {
            if (c == ':') break;
            firstColonPos++;
        }
        if (firstColonPos == line.size()) continue; // invalid
        std::string beat;
        for (size_t i = 0; i < firstColonPos; i++)
            beat += line.at(i);
        std::string action;
        for (size_t i = firstColonPos + 1; i < line.size(); i++)
            action += line.at(i);
        
        double beatDouble;
        if (!checkIsDouble(beat, beatDouble)) continue;

        if (lastBeat == beatDouble)
        {
            result.rbegin()->action += "," + action;
        }
        else
        {
            result.push_back(Command {
                beatDouble,
                action
            });
            lastBeat = beatDouble;
        }
    }

    return result;
}

void Serializer::readLines(char *contents, size_t byteCount)
{
    this->lines.clear();
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
}

SerializerResult Serializer::readSongList(std::filesystem::path basePath)
{
    // Try finding the file.
    std::string err;
    std::ifstream in("edit_resources/songs.txt", std::ios::binary);
    if (!in.is_open()) {

        std::vector<SerializerError> errs = {{0, "Could not find edit_resources/songs.txt."}};

        return SerializerResult {
            std::in_place_type<SerializerFailure>,
            SerializerFailure {errs}
        };
    }

    // Start parsing.
    std::string fileLine;
    this->errors.clear();

    EditSong song = {};
    std::vector<EditSong> editSongs = {};

    size_t l = 0; // Parsed line number.
    size_t i = 0;

    // Read each line.
    while (std::getline(in, fileLine))
    {
        i++;
        if (fileLine.empty()) continue;
        if (fileLine.at(0) == '#') continue;
        if (l % 2 == 0)
        {
            // Song name.
            song.song = fileLine;
            // Verify that a folder of this song exists.
            std::filesystem::path folder = basePath / ("edit_resources/" + fileLine);
            if (!std::filesystem::exists(folder)) {
                errors.push_back({i, "Got song named '" + fileLine + "', but couldn't find its folder! Skipping..."});
                std::getline(in, fileLine); // Skip the author as well.
                continue;
            }
            if (!std::filesystem::exists(folder / "music.ogg") && !std::filesystem::exists(folder / "music.wav") && !std::filesystem::exists(folder / "music.mp3")) {
                errors.push_back({i, "Got song named '" + fileLine + "', but couldn't find its music file! Skipping..."});
                std::getline(in, fileLine); // Skip the author as well.
                continue;
            }
            if (!std::filesystem::exists(folder / "song.txt")) {
                errors.push_back({i, "Got song named '" + fileLine + "', but couldn't find its song.txt file! Skipping..."});
                std::getline(in, fileLine); // Skip the author as well.
                continue;
            }
        }
        else 
        {
            // Song author.
            song.author = fileLine;
            // Add this to the list of songs.
            editSongs.push_back(song);
            song = {"", ""};
        }
        l++;
    }
    if (l % 2 == 1) errors.push_back({i, "No song author was provided for the last song listed in songs.txt."});

    in.close();

    if (errors.empty())
    {
        SerializerSongListSuccess success {
            editSongs
        };
        // Return errors.
        SerializerResult result {
            std::in_place_type<SerializerSongListSuccess>,
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

std::string Serializer::saveCompressedBallfile(size_t musicId, std::vector<Ball> &balls, std::vector<PaddleWidthChange> &paddleWidthChanges, std::vector<PaddleSpeedChange> &paddleSpeedChanges, std::vector<PaddleDualChange>& paddleDualChanges)
{
    std::string output = (musicId < 10 ? "0" : "") + std::to_string(musicId);
    // Sort the orderable items.
    auto ballIter = balls.begin();
    auto paddleWidthChangesIter = paddleWidthChanges.begin();
    auto paddleSpeedChangesIter = paddleSpeedChanges.begin();
    auto paddleDualChangesIter = paddleDualChanges.begin();
    this->errors.clear();
    std::vector<OrderableBallfileItem> orderableItems = {};
    while (true)
    {
        // Specifies next in line.
        // `0`: None
        // `1`: Ball
        // `3`: Paddle width change
        // `4`: Paddle speed change
        // `5`: Paddle dual change
        uint8_t next = 0;
        double leastBeat = INFINITY;
        // Prioritize Paddle Dual changes over the below changes.
        if (paddleDualChangesIter != paddleDualChanges.end())
        {
            if (paddleDualChangesIter->beat < leastBeat)
            {
                leastBeat = paddleDualChangesIter->beat;
                next = 5;
            }
        }
        // Prioritize Paddle Speed changes over the below changes.
        if (paddleSpeedChangesIter != paddleSpeedChanges.end())
        {
            if (paddleSpeedChangesIter->beat < leastBeat)
            {
                leastBeat = paddleSpeedChangesIter->beat;
                next = 4;
            }
        }
        // Prioritize Paddle Width changes over BPM changes.
        if (paddleWidthChangesIter != paddleWidthChanges.end())
        {
            if (paddleWidthChangesIter->beat < leastBeat)
            {
                leastBeat = paddleWidthChangesIter->beat;
                next = 3;
            }
        }
        // Now ball.
        if (ballIter != balls.end())
        {
            if (Ball::toBeats(ballIter->at) < leastBeat)
            {
                leastBeat = Ball::toBeats(ballIter->at);
                next = 1;
            }
        }
        if (next == 0) break;
        switch (next)
        {
            case 5: {orderableItems.push_back(*paddleDualChangesIter); paddleDualChangesIter++; break;}
            case 4: {orderableItems.push_back(*paddleSpeedChangesIter); paddleSpeedChangesIter++; break;}
            case 3: {orderableItems.push_back(*paddleWidthChangesIter); paddleWidthChangesIter++; break;}
            case 1: {orderableItems.push_back(*ballIter); ballIter++; break;}
        }
    }

    size_t measure = -1;
    size_t measureStart = 0; // inclusive.
    size_t measureEnd = 0; // exclusive.
    size_t i = 0;
    
    size_t measuresSkipped = 0;

    const minibeat quantizations[] = {
        96, 48, 24, 16, 12, 8, 6, 4, 3, 1
    };

    while (i < orderableItems.size())
    {
        // Go to the next measure.
        measure++;

        measureStart = measureEnd;
        i = measureStart;
        // First, identify the item IDs where the current measure starts & ends.
        while (i < orderableItems.size())
        {
            OrderableBallfileItem& item = orderableItems.at(i);
            double beat = getMinibeatOfOrderable(item) / MINIBEATS_PER_BEAT;
            if (beat >= (measure + 1) * 4) {
                measureEnd = i;
                break;
            } // End of measure.
            i++;
        }
        if (i == orderableItems.size()) measureEnd = orderableItems.size();

        // Skip this measure if nothing is inside it.
        if (measureEnd == measureStart + 1)
        {
            measuresSkipped++;
            continue;
        }

        // Specify the number of skipped measures, if any.
        if (measuresSkipped > 0)
        {
            if (measuresSkipped == 1) output += "00";
            else
            {
                std::string measuresSkippedStr = std::to_string(measuresSkipped);
                output += "9" + std::to_string(measuresSkippedStr.size() - 1) + measuresSkippedStr; 
            }
        }
        
        // Find the lowest number for the unit beat.
        size_t x = 0;
        minibeat q = 1;
        for (x = 0; x < 10; x++)
        {
            next_x:
            if (x == 10) continue; // failsafe, should not happen as (any num) % 1 is always 0.
            q = quantizations[x];
            for (int i = measureStart; i < measureEnd; i++)
            {
                minibeat modBeat = getMinibeatOfOrderable(orderableItems.at(i)) % (MINIBEATS_PER_BEAT * 4);
                if (modBeat % q != 0) {
                    x++;
                    goto next_x;
                }
            }
            // This quantization works!
            break;
        }
        // Specify this.
        output += "0" + std::to_string(x);

        // Now encode each item within this measure.
        size_t unitBeatsPassed = 0;
        for (int i = measureStart; i < measureEnd; i++)
        {
            OrderableBallfileItem& item = orderableItems.at(i);

            // We always add the unit beat first.
            size_t nextPassed = getMinibeatOfOrderable(orderableItems.at(i)) % (MINIBEATS_PER_BEAT * 4);
            size_t delta = nextPassed - unitBeatsPassed;
            // Ɐ uB < 200
            if (delta >= 200) errors.push_back(SerializerError {0, "Assertion failed: delta < 200"});
            std::string unitBeatStr = "";
            // Check for common unit beats.
            switch (delta)
            {
                case 0: { unitBeatStr = "0"; break; }
                case 1: { unitBeatStr = "1"; break; }
                case 2: { unitBeatStr = "2"; break; }
                case 3: { unitBeatStr = "3"; break; }
                case 4: { unitBeatStr = "4"; break; }
                case 6: { unitBeatStr = "5"; break; }
                case 8: { unitBeatStr = "6"; break; }

                case 5: { unitBeatStr = "70"; break; }
                case 7: { unitBeatStr = "71"; break; }
                case 9: { unitBeatStr = "72"; break; }
                case 10: { unitBeatStr = "73"; break; }
                case 11: { unitBeatStr = "74"; break; }
                case 12: { unitBeatStr = "75"; break; }
                case 13: { unitBeatStr = "76"; break; }
                case 14: { unitBeatStr = "77"; break; }
                case 15: { unitBeatStr = "78"; break; }
                case 16: { unitBeatStr = "79"; break; }
            }
            if (unitBeatStr.empty()) unitBeatStr = ((delta >= 100 ? "8" : "9") + (delta % 100));
            unitBeatsPassed = nextPassed;

            // Use a case-by-case basis for each type of item.
            if (std::holds_alternative<Ball>(item))
            {
                // Find the correct ball ID.
                Ball& ball = std::get<Ball>(item);
                bool isPit = false;
                if (std::holds_alternative<BallTypeNormal>(ball.type)) output += "1";
                if (std::holds_alternative<BallTypeMine>(ball.type)) output += "2";
                if (std::holds_alternative<BallTypeHold>(ball.type)) output += "3";
                if (std::holds_alternative<BallTypePit>(ball.type)) {output += "4"; isPit = true;}
                if (std::holds_alternative<BallTypeBouncy>(ball.type)) output += "5";

                output += unitBeatStr;

                // Get the x position of the ball.
                output += getCompressedXPos(ball.x, errors);

                // Get the speed of the ball.
                int speedInt = round(ball.speed * 1000);
                // Ɐ 0.001 < speed < 10
                if (!(1 < speedInt && speedInt < 10000)) errors.push_back(SerializerError {0, "Assertion failed: 0.001 < ball.speed < 10"});
                if (speedInt % 100 == 0 && speedInt <= 4000)
                {
                    output += std::to_string(speedInt / 100 - 1);
                }
                else if (speedInt % 10 == 0 && speedInt <= 5000)
                {
                    output += std::to_string(speedInt / 10 + 400 - 1);
                }
                else output += std::to_string(90000 + speedInt - 1);

                // For 'hold-like' balls.
                if (std::holds_alternative<BallTypeHold>(ball.type) || isPit) {
                    std::vector<BallTypeTailPoint> points;
                    if (isPit) points = std::get<BallTypePit>(ball.type).points;
                    else points = std::get<BallTypeHold>(ball.type).points;

                    // Store all hold tail points.
                    minibeat lastMb = 0;
                    for (auto& point : points)
                    {
                        // Store the point's minibeat delta.
                        output += getCompressedBeat(point.minibeats, lastMb, errors);

                        // Store the point's X position.
                        output += getCompressedXPos(point.x, errors);
                    }
                    output += "9"; // Terminator digit.
                }

                // For bouncy balls.
                if (std::holds_alternative<BallTypeBouncy>(ball.type))
                {
                    auto bouncy = std::get<BallTypeBouncy>(ball.type);
                    int r = bouncy.respawns;
                    // Ɐ 1 <= r <= 100
                    if (!(1 <= r && r <= 100)) errors.push_back(SerializerError {0, "Assertion failed: 1 <= r <= 100"});
                    output += (r < 10 ? "0" : "") + std::to_string(r);
                    minibeat lastMb = 0;
                    output += getCompressedBeat(bouncy.interval, lastMb, errors);
                }
            }
            else if (std::holds_alternative<PaddleWidthChange>(item))
            {
                output += "6";
                output += unitBeatStr;
                auto width = (int) round(std::get<PaddleWidthChange>(item).width);
                // Ɐ 0 <= width <= 454
                if (!(0 <= width && width <= 454)) errors.push_back(SerializerError {0, "Assertion failed: 0 <= width <= 454"});
                // Store the width.
                output += (width < 10 ? "00" : (width < 100 ? "0" : "")) + std::to_string(width);
            }
            else if (std::holds_alternative<PaddleSpeedChange>(item))
            {
                output += "7";
                output += unitBeatStr;
                auto speed = (int) round(std::get<PaddleSpeedChange>(item).speed);
                // Ɐ 0 <= speed <= 499.75
                if (!(0 <= speed && speed <= 499.75)) errors.push_back(SerializerError {0, "Assertion failed: 0 <= speed <= 499.75"});
                // Store the speed.
                speed *= 2;
                output += (speed < 10 ? "00" : (speed < 100 ? "0" : "")) + std::to_string(speed);
            }
            // Dual changes have no extra possible parameters.
            else if (std::holds_alternative<PaddleDualChange>(item))
            {
                output += "8";
                output += unitBeatStr;
            }
        }
        measuresSkipped = 0; // Reset this counter.
    }

    return output;
}

std::string Serializer::getCompressedXPos(float x, std::vector<SerializerError>& errors)
{
    // Ɐ 249.75 > uB > -250.25
    if (!(249.75 > x && x > -250.25)) errors.push_back(SerializerError {0, "Assertion failed: 249.75 > x > -250.25"});
    // Get the speed of the ball.
    int num = round(x * 2) + 500;
    return (num < 10 ? "00" : (num < 100 ? "0" : "")) + std::to_string(num);
}

std::string Serializer::getCompressedBeat(minibeat mb, minibeat& lastMb, std::vector<SerializerError>& errors)
{
    int deltaMbInt = mb - lastMb;
    if (deltaMbInt == 0) return "";
    std::string deltaMb = std::to_string(deltaMbInt);
    // Ɐ 0 <= Δmb < 1e9
    if (!(0 < deltaMbInt && deltaMbInt < 1e9)) errors.push_back(SerializerError {0, "Assertion failed: 0 < deltaMb < 1e9"});
    lastMb = mb;
    return std::to_string(deltaMb.size() - 1) + "" + deltaMb;
}

minibeat Serializer::getMinibeatOfOrderable(OrderableBallfileItem& item)
{
    if (std::holds_alternative<Ball>(item)) return std::get<Ball>(item).at;
    if (std::holds_alternative<BpmChange>(item)) return round(std::get<BpmChange>(item).beat * 48);
    if (std::holds_alternative<PaddleDualChange>(item)) return round(std::get<PaddleDualChange>(item).beat * 48);
    if (std::holds_alternative<PaddleSpeedChange>(item)) return round(std::get<PaddleSpeedChange>(item).beat * 48);
    if (std::holds_alternative<PaddleWidthChange>(item)) return round(std::get<PaddleWidthChange>(item).beat * 48);
    return 0;
}

SerializerResult Serializer::readCompressedBallfile(char *contents, size_t byteCount)
{
    size_t i = 0;
    char ch;
    std::vector<char> chars;
    while (i < byteCount)
    {
        ch = contents[i++];
        chars.push_back(ch);
    }
    
    /* for (i = 0; i < chars.size(); i++)
    {
        char macroCodeId = chars.at(i);
        switch (macroCodeId) {
            case '0':
            default: 
        }
    } */
}

bool Serializer::readCompressedFileChars(std::vector<char>& vector, size_t n, size_t& i, std::string& destStr)
{
    std::string result = "";
    size_t iInitial = i;
    for (; i < i + n; i++) {
        if (i >= vector.size()) return false;
        char ch = vector.at(i);
        if (ch < '0' || ch > '9') return false;
        result += ch;
    }
    destStr = result;
    return true;
}

std::vector<std::string> splitStringStream(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}