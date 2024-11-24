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
};

#endif