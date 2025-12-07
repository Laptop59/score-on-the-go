#ifndef SCORE_ON_THE_GO_SERIALIZER
#define SCORE_ON_THE_GO_SERIALIZER

#include <vector>
#include "ball.h"
#include <stddef.h>
#include <variant>
#include <string>
#include <optional>
#include <filesystem>
#include "change.h"

/**
 * Structure used to store the data of an 'edit song'.
 */
struct EditSong
{
    std::string song;
    std::string author;
};


// Represents a serializer success.
struct SerializerSuccess
{
    std::vector<Ball> balls;
    std::vector<BpmChange> bpmChanges;
    std::vector<PaddleWidthChange> paddleWidthChanges;
    std::vector<PaddleSpeedChange> paddleSpeedChanges;
    std::vector<PaddleDualChange> paddleDualChanges;
    float musicOffset;
    // The music ID of the chart, + 1. If no such music is defined, this is 0.
    size_t musicId;
};

// Represents a serializer success for songs.txt.
struct SerializerSongListSuccess
{
    std::vector<EditSong> editSongs;
};

// Represents a serializer error.
struct SerializerError
{
    size_t line;       // Line at which the error occured.
    std::string error; // Error type.
};

// Represents a serializer failure. It has a vector of errors.
struct SerializerFailure
{
    std::vector<SerializerError> errors;
};

// Represents a result from the serializer.
using SerializerResult = std::variant<SerializerSuccessFromBallFile, SerializerSongListSuccess, SerializerSuccessFromDecoding, SerializerFailure>;

// Represents any orderable item (macrocode) in compressed ballfile data.
using OrderableBallfileItem = std::variant<Ball, BpmChange, PaddleWidthChange, PaddleSpeedChange, PaddleDualChange>;

// Used in parsing ballfiles for bpm changes.
const std::string bpmString = "bpm";

// Used in parsing ballfiles for paddle width changes.
const std::string paddleWidthString = "pw";

// Used in parsing ballfiles for paddle speed changes.
const std::string paddleSpeedString = "ps";

// Used in parsing ballfiles for dual changes.
const std::string paddleDualString = "dual";

// The list of quantizations for the conversion number in measure macrocodes (OX), starting from 0.
const minibeat quantizations[] = {
    96, 48, 24, 16, 12, 8, 6, 4, 3, 1
};

class Serializer
{
    private:
        size_t line;
        size_t char_i;

        size_t currentMeasure;
        minibeat mbPerUb;
        unitbeat currentUb;
        bool currentDualStatus;

        std::vector<std::vector<char>> lines;
        std::vector<Ball> balls;
        std::vector<BpmChange> bpmChanges;
        std::vector<PaddleWidthChange> paddleWidthChanges;
        std::vector<PaddleSpeedChange> paddleSpeedChanges;
        std::vector<PaddleDualChange> paddleDualChanges;
        std::vector<Command> commands;
        std::vector<char> chars;

        char parseConsumeChar();
        char parsePeekChar();

        size_t parseOneDigitNumber();
        size_t parseTwoDigitNumber();
        size_t parseNumber(size_t digits);

        std::string parseConsumeMany(size_t chars);
        std::string parsePeekMany(size_t chars);

        void parseNewMeasure();

        // Parses a normal ball or mine.
        void parseUnitBall(bool isMineLike);

        // Parses a hold or pit.
        void parseTailedBall(bool isMineLike);

        // Parses a mini beat, x-pos and speed as one object.
        Mxs parseUxs();

        // Parses unitbeats which are then multiplied to give minibeats.
        minibeat parseUbConvertToMb();

        // Parses delta unitbeats which are then multiplied to give minibeats.
        minibeat parseDeltaUbConvertToMb();

        // Parses unitbeats.
        unitbeat parseUb();

        float parseX();

        float parseSpeed();

        std::vector<BallTypeTailPoint> parseNodes();

    public:
        std::vector<SerializerError> errors;

        // Creates a new serializer for use.
        Serializer();

        // Saves a ball file by returning string contents.
        std::string saveBallfile(std::vector<Ball> &balls, std::vector<BpmChange> &bpmChanges, std::vector<PaddleWidthChange> &paddleWidthChanges, std::vector<PaddleSpeedChange> &paddleSpeedChange, std::vector<PaddleDualChange>& paddleDualChanges);
        
        // Reads a ball file from its contents and returns the result.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        // If `startsWithMusicOffset` is true, the first read line is the music offset.
        SerializerResult readBallfile(char* contents, size_t byteCount, bool startsWithMusicOffset);

        // Writes commands from its contents and returns the result.
        std::string writeCommands(std::vector<Command> &commands);

        // Reads commands from its contents and returns the result.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        std::vector<Command> readCommands(char* contents, size_t byteCount);

        // Checks if a string is a double and parses it if so.
        static bool checkIsDouble(std::string inputString, double &result)
        {
            bool negative = false;
            if (inputString.at(0) == '-')
            {
                negative = true;
                inputString = inputString.substr(1);
            }
            char* end;
            result = strtod(inputString.c_str(), &end) * (negative ? -1 : 1);
            if (end == inputString.c_str() || *end != '\0') return false;
            return true;
        }

        // Checks if a string is an unsigned integer and parses it if so.
        static bool checkIsUnsignedInt(std::string inputString, size_t &result)
        {
            return sscanf(inputString.c_str(), "%zu", &result) == 1;
        }

        // Checks if a string is an unsigned minibeat and parses it if so.
        static bool checkIsUnsignedMinibeat(std::string inputString, minibeat &result)
        {
            try {
                result = static_cast<minibeat>(std::stoul(inputString));
                return true;
            } catch (std::exception e) {
                return false;
            }
        }

        // Checks if a string is an unsigned integer and parses it if so.
        static bool checkIsUnsignedInt(std::string inputString, uint32_t &result)
        {
            try {
                result = static_cast<uint32_t>(std::stoul(inputString));
                return true;
            } catch (std::exception e) {
                return false;
            }
        }

        // Checks if a character is a digit from 0-9.
        static bool checkIsDigit(char inputChar)
        {
            return (inputChar >= '0' && inputChar <= '9');
        }

        // Checks if a character is a digit from 0-9, and parses it if so.
        static bool checkIsDigit(char inputChar, uint8_t &result)
        {
            if (inputChar >= '0' && inputChar <= '9') {
                result = (int) (inputChar - '0');
                return true;
            }
            return false;
        }

        // Tries to parse a Δb (change in beats) value for hold-like balls from a character array. Stores it in regular beats if successful.
        // If not, stores the error in the `error` string.
        static bool parseDeltaBeat(std::vector<char> &arr, size_t &i, minibeat &result, std::string &error)
        {
            try {
                uint8_t len = 0;
                if (!checkIsDigit(arr.at(++i), len)) return false;
                len++; // to get the actual length.
                std::string minibeats = "";
                for (size_t j = 0; j < len; j++) minibeats.push_back(arr.at(++i));
                uint32_t minibeatsInt = 0;
                if (!checkIsUnsignedMinibeat(minibeats, minibeatsInt)) {
                    error += "Invalid unsigned number for Δb: " + minibeats;
                    return false;
                }
                result = minibeatsInt;
                return true;
            } catch (std::exception e) { throw e; };
        }

        // Tries to parse a unit beat from a character array. Stores it if successful.
        // If not, stores the error in the `error` string.
        static bool parseUnitBeat(std::vector<char> &arr, size_t &i, uint8_t &result, std::string &error)
        {
            try {
                char first = arr.at(++i);
                switch (first) {
                    case '7':
                        {
                            // Less common unit beats (take a 2nd digit).
                            char second = arr.at(++i);
                            if (second >= '0' && second <= '9')
                            {
                                std::vector<uint8_t> lessCommon = {5, 7, 9, 10, 11, 12, 13, 14, 15, 16};
                                result = lessCommon.at(second - '0');
                                return true;
                            }
                            error = "Invalid 2nd character for unit beat: ";
                            error += second;
                            return false;
                        }
                    case '8':
                    case '9':
                        // Arbitrary unit beat.
                        uint8_t a1; uint8_t a2;
                        if (!checkIsDigit(arr.at(++i), a1) || !checkIsDigit(arr.at(++i), a2)) {
                            error = "Invalid digit(s) for arbitrary unit beat: ";
                            error += a1;
                            error += a2;
                            return false;
                        }
                        result = (first == '9' ? 100 : 0) + 10 * a1 + a2;
                        return true;
                    default:
                        if (first >= '0' && first <= '6')
                        {
                            // Common unit beats.
                            std::vector<uint8_t> common = {0, 1, 2, 3, 4, 6, 8};
                            result = common.at(first - '0');
                            return true;
                        }
                        error += "Invalid 1st character for unit beat: ";
                        error += first;
                        return false;
                }
            } catch (std::out_of_range& e) { throw e; };
        }

        // Tries to parse an X-position from a character array. Stores the actual amount if successful.
        // If not, stores the error in the `error` string.
        static bool parsePosition(std::vector<char> &arr, size_t &i, float &result, std::string &error)
        {
            try {
                std::string number = "";
                for (size_t j = 0; j < 3; j++) number.push_back(arr.at(++i));
                uint32_t pos = 0;
                if (!checkIsUnsignedInt(number, pos)) {
                    error = "Expected a number, found: " + number;
                    return false;
                }
                float x = ((float) pos - 500) / 2;
                if (x < 249.75 && x > -250.25) {
                    result = x;
                    return true;
                }
                error = "Invalid x position (x < 249.75 && x > -250.25): " + std::to_string(x);
                return false;
            } catch (std::exception e) { throw e; };
        }

        // Gets all the tail points of a ball's type.
        static std::vector<BallTypeTailPoint>* getPointsFromType(BallType& type);

        // Clears any currently stored lines, reads some file contents and returns any read lines.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        void readLines(char *contents, size_t byteCount);

        // Reads the songs.txt file in edit_resources and if valid, returns the songs found.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        SerializerResult readSongList(std::filesystem::path basePath);

        // Gets the minibeat of an orderable ballfile item.
        minibeat getMinibeatOfOrderable(OrderableBallfileItem& item);

        // Gets the compressed X position of an X value.
        std::string getCompressedXPos(float x, std::vector<SerializerError>& errors);

        // Gets the compressed beat of an X value. The `lastMb` value is set to the `minibeats` value at the end.
        std::string getCompressedBeat(minibeat minibeats, minibeat& lastMb, std::vector<SerializerError>& errors);

        // Saves a compressed ball file by returning string contents.
        std::string saveCompressedBallfile(size_t musicId, std::vector<Ball> &balls, std::vector<PaddleWidthChange> &paddleWidthChanges, std::vector<PaddleSpeedChange> &paddleSpeedChange, std::vector<PaddleDualChange>& paddleDualChanges);

        // Reads `n` characters from a compressed (number-only) ball file, and sets the read combined string into `destStr`.
        // Returns if this was successful.
        bool readCompressedFileChars(std::vector<char>& vector, size_t n, size_t& i, std::string& destStr);
};

std::vector<std::string> splitStringStream(const std::string& str, char delimiter);

#endif