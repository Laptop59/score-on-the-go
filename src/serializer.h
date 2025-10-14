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
using SerializerResult = std::variant<SerializerSuccess, SerializerSongListSuccess, SerializerFailure>;

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

class Serializer
{
    private:
        size_t line;
        std::vector<std::vector<char>> lines;
        std::vector<BpmChange> bpmChanges;
        std::vector<PaddleWidthChange> paddleWidthChanges;
        std::vector<PaddleSpeedChange> paddleSpeedChanges;
        std::vector<PaddleDualChange> paddleDualChanges;
        std::vector<Command> commands;

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

        // Gets all the tail points of a ball's type.
        static std::vector<BallTypeTailPoint>* getPointsFromType(BallType& type);

        // Clears any currently stored lines, reads some file contents and returns any read lines.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        void readLines(char *contents, size_t byteCount);

        // Reads the songs.txt file in edit_resources and if valid, returns the songs found.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        SerializerResult readSongList(std::filesystem::path basePath);

        // Reads a compressed (number-only) ball file from its contents and returns the result.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        SerializerResult readCompressedBallfile(char* contents, size_t byteCount);

        // Gets the minibeat of an orderable ballfile item.
        minibeat getMinibeatOfOrderable(OrderableBallfileItem& item);

        // Gets the compressed X position of an X value.
        std::string getCompressedXPos(float x, std::vector<SerializerError>& errors);

        // Gets the compressed beat of an X value. The `lastMb` value is set to the `minibeats` value at the end.
        std::string getCompressedBeat(minibeat minibeats, minibeat& lastMb, std::vector<SerializerError>& errors);

        // Deserializes a compressed ball file and returns the balls and other things associated within the file.
        SerializerResult readCompressedBallfile(char *contents, size_t byteCount);

        // Saves a compressed ball file by returning string contents.
        std::string saveCompressedBallfile(size_t musicId, std::vector<Ball> &balls, std::vector<PaddleWidthChange> &paddleWidthChanges, std::vector<PaddleSpeedChange> &paddleSpeedChange, std::vector<PaddleDualChange>& paddleDualChanges);

        // Reads `n` characters from a compressed (number-only) ball file, and sets the read combined string into `destStr`.
        // Returns if this was successful.
        bool readCompressedFileChars(std::vector<char>& vector, size_t n, size_t& i, std::string& destStr);
};

std::vector<std::string> splitStringStream(const std::string& str, char delimiter);

#endif