#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <vector>
#include <optional>

#ifndef SCORE_ON_THE_GO_GAME
#define SCORE_ON_THE_GO_GAME

#include "texture_library.h"
#include "ball.h"
#include "serializer.h"
#include <cmath>

// Constants

// No. of pixels in width the gameplay area has.
const float GAMEPLAY_WIDTH = 480.0f;

// No. of pixels in height the gameplay area has.
const float GAMEPLAY_HEIGHT = 360.0f;

// No. of pixels in width the screen has.
const float SCREEN_WIDTH = GAMEPLAY_WIDTH + 240.0f;

// No. of pixels in height the screen has.
const float SCREEN_HEIGHT = GAMEPLAY_HEIGHT;

// Left offset of gameplay.
const float GAMEPLAY_OFFSET = (SCREEN_WIDTH - GAMEPLAY_WIDTH) / 2.0f;

// Y position of the selected beat.
const float EDITOR_SELECTED_BEAT_Y = 60.0f;

// Range required for placing circles to selected beat.
const float EDITOR_RANGE_SELECTED_BEAT = 25.0f;

// Minimum separation between two beats.
const float EDITOR_BEAT_SEPARATION_MIN = 5;

// Maximum separation between two beats.
const float EDITOR_BEAT_SEPARATION_MAX = 250;

// Step separation between two beats.
const float EDITOR_BEAT_SEPARATION_STEP = 5;

// Game font path.
const char FONT_PATH[] = "resources/NotoSans-Regular.ttf";

// Ball size.
const float BALL_SIZE = 45;

// Transparent ghost ball's alpha value `0-255`.
const uint8_t GHOST_BALL_ALPHA = 0x7Fu;

// Divisor arrow width.
const float DIVISOR_ARROW_WIDTH = 36;

// Divisor arrow height.
const float DIVISOR_ARROW_HEIGHT = 40;

// Fast ball speed (speed >= `FAST_BALL_SPEED` has a thunderbolt symbol.)
const float FAST_BALL_SPEED = 1.5f;

// Default BPM when nothing else exists.
const double DEFAULT_BPM = 120.0;

// Specifies how text is aligned.
enum TextAlignment
{
    LEFT_ALIGNED   = 0,
    RIGHT_ALIGNED  = 1,
    CENTER_ALIGNED = 2
};

/*
 * An object representing relative position to gameplay.
 */
struct GameplayLeftPosition
{
    float x; // X-position.
    float y; // Y-position.
};

/*
 * Enum representing game state.
 */
enum GameState
{
    PLAYING            = 0x00,
    PLAYTESTING        = 0x01,
    EDITING_NONE       = 0x02,
    EDITING_BPM        = 0x03
};

/*
 * Enums representating keys in paddle movement.
 */
enum PaddleKeys
{
    LEFT_FAST  = 0x01,
    LEFT_SLOW  = 0x02,
    RIGHT_SLOW = 0x04,
    RIGHT_FAST = 0x08
};

/*
 * Enum representing input mode.
 */
enum InputMode
{
    NONE      = 0x00,
    NUMERIC   = 0x01,
    TEXT      = 0x02
};

/*
 * An object representing the game.
 */
class Game
{
    private:
        // Current beat.
        double beat = 0.0;

        // Current seconds in the ballfile.
        double seconds = 0.0;

        // Queued balls for playing/playtesting.
        std::vector<Ball> queuedBalls;

        // Beat where playtesting started from.
        double startPlaytestingBeat = 0.0;

        // Paddle position in +- context.
        float paddlePosition = 0.0;

        // Game state.
        GameState gameState = GameState::EDITING_NONE;

        // Text input by the user, used in various things.
        std::string inputText = "";

        // Text input cursor position
        // Example:
        // = 0 `|ABC`
        // = 1 `A|BC`
        size_t inputCursor = 0;

        // BPM changes in the song.
        std::vector<BpmChange> bpmChanges = {(BpmChange) {0.0, DEFAULT_BPM}};

        // Current circle speed for editing.
        float selectedSpeed = 1.0f;

        // Mouse positoon `[X, Y]`
        float mousePosition[2] = { INFINITY, INFINITY };

        // Paddle keys pressed.
        uint8_t paddleKeysPressed = 0b0000;
        
        // Current amount of space in `y` dimension between two beats.
        float beatSpacing = 45.0f;

        // If a file picker is current active, whether it would be saving/loading.
        bool filePickerOpen = false;

        // Filter used in opening ballfiles.
        SDL_DialogFileFilter* fileFilter;

        // Balls in the song file.
        std::vector<Ball> balls;

        // Its own serializer for loading & saving.
        std::unique_ptr<Serializer> serializer;

        // Selected color divisor.
        ColorDivisor selectedDivisor = ColorDivisor::DIVISOR_4TH;

        // Start playtesting from a beat.
        void startPlayTest(double beat);

        // Stop playtesting.
        void stopPlayTest();

        // Draw playtest elements.
        void renderPlaytest();

        // Draw editor elements.
        void renderEditor();
    
        // Draws a beat in the editor with a specific `y` coordinate.
        void renderEditorBeatLine(size_t beat, float y);

        // Draws text at a position.
        void drawText(std::string text, SDL_Color color, float x, float y, TextAlignment align, float size);

        // Draws a divisor arrow that should be drawn.
        void drawDivisorArrow();

        // Draws a ghost ball that 'could' be put.
        void drawGhostBall();

        // Draws an additional editor menu depending on GameState.
        void drawSpecificEditorMenu();

        // Checks whether a 'ball' is fast enough to have a thunderbolt symbol.
        bool isFast(float ballSpeed);

        // Warps to the nearest beat that satisfies the divisor.
        void warpBeatToDivisor();

        // Checks if a ball can be placed at current mouse position.
        std::optional<GameplayLeftPosition> ballCanBePlaced();

        // Draws a ball in the editor.
        void drawEditorBall(const Ball& ball);

        // Adds a ball in the balls vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addBall(const Ball& ball);

        // Adds a bpm change in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addBpmChange(const BpmChange& bpmChange);

        // Draws balls in the editor.
        void drawEditorBalls();

        // Handles mouse wheel events.
        void handleMouseWheelEvent(SDL_Event* event);

        // Handles mouse button down events.
        void handleMouseButtonDownEvent(SDL_Event* event);

        // Handle key down events.
        void handleKeyDownEvent(SDL_Event* event);

        // Opens the file picker for getting a ballfile.
        void openLoadFilePicker();

        // Opens the file picker for saving a ballfile.
        void openSaveFilePicker();

        // Returns true if no other event should be handled. Handles menu events.
        bool handleMenuEvent(SDL_Event* event);

        // Checks if number input should be handled.
        InputMode inputModeEnabled();

        // Resets text input.
        void resetInput();

        // Callback for opening-a-file picker.
        static void SDLCALL callbackLoadFilePicker(void* userdata, const char* const* filelist, int filter);

        // Callback for saving-a-file picker.
        static void SDLCALL callbackSaveFilePicker(void* userdata, const char* const* filelist, int filter);

        // Get displayed text string with |
        std::string getDisplayedInputText();

        // Gets the signed falling ball pos (`-180 - 180` range).
        float getSignedFallingBallPos(const Ball& ball);

        // Paddle width.
        float PADDLE_WIDTH = 100.0f;

        // Maximum from left the paddle can go in either side.
        float PADDLE_MAX_LEFT = GAMEPLAY_WIDTH / 2 - 13.0f;

        // Paddle top pixel.
        float PADDLE_TOP = GAMEPLAY_HEIGHT - 74.0f;

        // Paddle top pixel in `+-` form (Scratch relative)
        float PADDLE_TOP_SIGNED = 180.0f - PADDLE_TOP;

        // Paddle height.
        float PADDLE_HEIGHT = 22.0f;

        // Paddle speed (per second).
        float PADDLE_SPEED = 4.0f * 30;

        // Default ball speed.
        float BALL_SPEED = 100.0f;

    public:
        // Delta time passed since last frame.
        double deltaTimePassed;

        // Renderer for rendering the game.
        SDL_Renderer* renderer;

        // Window of the game.
        SDL_Window* window;

        // Font used in rendering the game.
        TTF_Font* font;

        // Texture library in rendering the game.
        std::unique_ptr<TextureLibrary> textureLibrary;

        // Constructor for the Game object.
        Game(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font);

        // Function to update with new delta time.
        void update();

        // Function to render to the renderer.
        void render();

        // Function to handle an SDL_Event.
        void handleEvent(SDL_Event* event);

        // Get seconds from beat.
        double getSecondsFromBeat(double beat);

        // Get beat from seconds.
        double getBeatFromSeconds(double seconds);

        // Deconstructor for the Game object.
        ~Game();
};

#endif