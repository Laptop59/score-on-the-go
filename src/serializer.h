#ifndef SCORE_ON_THE_GO_SERIALIZER

#include <vector>
#include "ball.h"
#include <stddef.h>
#include <variant>
#include <string>
#include <optional>
#include "bpm.h"

// Represents a serializer success.
struct SerializerSuccess
{
    std::vector<Ball> balls;
    std::vector<BpmChange> bpmChanges;
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
using SerializerResult = std::variant<SerializerSuccess, SerializerFailure>;

// Used in parsing ballfiles for bpm changes.
const std::string bpmString = "bpm";

class Serializer
{
    private:
        size_t line;
        std::vector<SerializerError> errors;
        std::vector<std::vector<char>> lines;
        std::vector<BpmChange> bpmChanges;

    public:
        // Creates a new serializer for use.
        Serializer();

        // Saves a ball file and returns string contents.
        std::string saveBallfile(std::vector<Ball>& balls, std::vector<BpmChange>& bpmChanges);
        
        // Reads a ball file from its contents and returns the result.
        // Note: DO NOT PASS `NULL`/`nullptr` into the contents.
        SerializerResult readBallfile(char* contents, size_t byteCount);

        // Checks if a string is a double and parses it if so.
        static bool checkIsDouble(std::string inputString, double &result) {
            char* end;
            result = strtod(inputString.c_str(), &end);
            if (end == inputString.c_str() || *end != '\0') return false;
            return true;
        }

        // Checks if a string is an unsigned integer and parses it if so.
        static bool checkIsUnsignedInt(std::string inputString, size_t &result) {
            return sscanf(inputString.c_str(), "%zu", &result) == 1;
        }

        // Checks if a string is an unsigned minibeat and parses it if so.
        static bool checkIsUnsignedMinibeat(std::string inputString, minibeat &result) {
            try {
                result = static_cast<minibeat>(std::stoul(inputString));
                return true;
            } catch (std::exception e) {
                return false;
            }
        }

        // Checks if a string is an unsigned integer and parses it if so.
        static bool checkIsUnsignedInt(std::string inputString, uint32_t &result) {
            try {
                result = static_cast<uint32_t>(std::stoul(inputString));
                return true;
            } catch (std::exception e) {
                return false;
            }
        }

        // Gets all the tail points of a ball's type.
        static std::vector<BallTypeTailPoint>* getPointsFromType(BallType& type);
};

#endif