#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <memory>
#include <vector>
#include <optional>
#include <unordered_map>

#ifndef SCORE_ON_THE_GO_GAME
#define SCORE_ON_THE_GO_GAME

#include "texture_library.h"
#include "ball.h"
#include "serializer.h"
#include <cmath>

// Constants

// Title/Name of the game.
const char NAME[] = "Score on the Go";

// Organization of this game.
const char ORG[] = "Laptop59";

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

// Tail size (diameter).
const float TAIL_SIZE = 36;

// Default spacing between two beats.
const float DEFAULT_BEAT_SPACING = 45;

// Transparent ghost ball's alpha value `0-255`.
const uint8_t GHOST_BALL_ALPHA = 0x7Fu;

// Width of selector rectangle (excluding outline).
const float SELECTOR_RECTANGLE_WIDTH = 8.0f;

// Height of selector rectangle (excluding outline).
const float SELECTOR_RECTANGLE_HEIGHT = 32.0f;

// Thickness of outline of selector rectangle.
const float SELECTOR_RECTANGLE_OUTLINE_THICKNESS = 1.5f;

// Fast ball speed (speed >= `FAST_BALL_SPEED` has a thunderbolt symbol.)
const float FAST_BALL_SPEED = 1.5f;

// Default BPM when nothing else exists.
constexpr static double DEFAULT_BPM = 120.0;

// Unit worth of a relative font size.
const float UNIT_FONT_SIZE = 0.5f;

// Special error string returned when the user rejects an action.
const static std::string CANCELLED = "cancelled";

// Number of minibeats between each long ball node. Affects gameplay.
const minibeat LONG_BALL_NODE_SPACING = (minibeat) (MINIBEATS_PER_BEAT * 0.25);

// A platform-independent callback similar to `SDL_DialogFileCallback`, but gives file data instead of file paths. These callbacks
// should set the appropriate variables and `SDL_free(contents)`.
typedef void (*FileDataCallback)(void *userdata, void* contents, int filter, size_t sizeInBytes);

// A platform-independent callback similar to `SDL_DialogFileCallback`, but gives file data instead of file paths, and does not have a size parameter. These callbacks
// should set the appropriate variables.
typedef void (*FileDataSaveCallback)(void *userdata, const char* path, int filter);

// Specifies how text is aligned.
enum TextAlignment
{
    LEFT_ALIGNED   = 0,
    RIGHT_ALIGNED  = 1,
    CENTER_ALIGNED = 2
};

/*
 * An object representing a circle in a 2D dimension.
 */
struct Circle
{
    float x; // X-position of its center.
    float y; // Y-position of its center.
    float r; // Radius of circle.
};

/*
 * Judgment in game.
 */
enum Judgment
{
    PERFECT      = 0x01,
    GREAT        = 0x02,
    GOOD         = 0x03,
    MISS         = 0x00,
    OK         = 0x04,
    OOPS     = 0x05,
};

/*
 * Placing mode currently selected.
 */
enum PlacingMode
{
    BALL        = 0x00,
    MINE        = 0x01,
    SQUARE      = 0x02,
    BOUNCY      = 0x03,
    INVALID
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
    PLAYING               = 0x00,
    PLAYTESTING           = 0x01,
    EDITING_NONE          = 0x02,
    EDITING_BPM           = 0x03,
    EDITING_HELP          = 0x04,
    EDITING_PW            = 0x05,
    EDITING_PS            = 0x06,
    EDITING_CMD           = 0x07,
    EDITING_DUAL          = 0x08,
    EDITING_MUSIC_OFFSET  = 0x09,

    // Edit-specific
    EDITING_MUSIC         = 0x10
};

/*
 * Enums representating keys in paddle movement.
 */
enum PaddleKeys
{
    LEFT  = 0x01,
    HIT1  = 0x02,
    HIT2  = 0x04,
    RIGHT = 0x08
};

/*
 * Enums representing a key from a custom set of keys for a specific placing mode.
*/
enum CustomPlacingModeKey
{
    KEY_0  = 0,
    KEY_1  = 1,
    KEY_2  = 2,
    KEY_3  = 3,
    KEY_4  = 4,
    KEY_5  = 5,
    KEY_6  = 6,
    KEY_7  = 7,
    KEY_8  = 8,
    KEY_9  = 9,
};

/*
 * Enum representing input mode.
 */
enum InputMode
{
    NONE      = 0x00,
    NUMERIC   = 0x01,
    TEXT      = 0x02,
    SELECT    = 0x03
};

/*
 * Enum representing selected ball to be placed (normal, hold, etc...)
 */
enum SelectedPlaceableBall
{
    NORMAL      = 0x00,
};

/*
 * Structure representing a ball flash, after a ball is hit by the paddle.
 */
struct BallFlash
{
    double secondsWhenHit;
    Judgment judgment;
    float x;
    float y;
};

/*
 * Structure used for a macro for easily drawing text lines in the renderer.
 */
struct DTL_State
{
    float x;
    float y;
    float fontScale;
    bool ongoing;
};

#define WHITE (SDL_Color {0xFF, 0xFF, 0xFF, 0xFF})
#define BLACK (SDL_Color {0x00, 0x00, 0x00, 0xFF})
#define TEXT_LINE_SPACING (32)
#define KEY_MARGIN (13)
#define KEY_SEPARATION (18)
#define KEY_WIDTH (24)
#define KEY_HEIGHT (12)

/** Helper macro for making text for editing drawn easily. */
#define DRAW_TEXT_LINES(xPos, yPos, fontScale) for (DTL_State _DTL_ = {(xPos), (yPos), (fontScale), true}; _DTL_.ongoing; _DTL_.ongoing = false)

/** Draw text line with a color. */
#define DRAW_TEXT(string, color) drawText((string), (color), _DTL_.x, _DTL_.y, TextAlignment::LEFT_ALIGNED, _DTL_.fontScale); _DTL_.y += _DTL_.fontScale * TEXT_LINE_SPACING

/** Draw a help line using two text drawing calls. */
#define DRAW_HELP(key, string) drawWhiteRect(_DTL_.x + KEY_MARGIN - KEY_WIDTH / 2, _DTL_.y - KEY_HEIGHT / 2, KEY_WIDTH, KEY_HEIGHT); drawTextWithOutline((key), BLACK, _DTL_.x + KEY_MARGIN, _DTL_.y, TextAlignment::CENTER_ALIGNED, 0.25f, 1, BLACK); drawText((string), WHITE, _DTL_.x + KEY_MARGIN + KEY_SEPARATION, _DTL_.y, TextAlignment::LEFT_ALIGNED, _DTL_.fontScale); _DTL_.y += _DTL_.fontScale * TEXT_LINE_SPACING

/** Draw text line with white color. */
#define DRAW_TEXT_W(string) DRAW_TEXT((string), WHITE)

/** Leave a line in the text list. */
#define LEAVE_LINE() LEAVE_SPACE(TEXT_LINE_SPACING)

/** Leave space in the text list. */
#define LEAVE_SPACE(space) _DTL_.y += _DTL_.fontScale * space;

/*
 * An object representing the game.
 */
class Game
{
    private:
        //////////////////////////////// CONSTANTS ///////////////////////////////////////

        // Default paddle width.
        constexpr static float DEFAULT_PADDLE_WIDTH = 125.0f;

        // Maximum from left the paddle's edge can go in either side.
        float PADDLE_MAX_LEFT = GAMEPLAY_WIDTH / 2 - 13.0f;

        // Paddle top pixel.
        float PADDLE_TOP = GAMEPLAY_HEIGHT - 74.0f;

        // Paddle top pixel in `+-` form (Scratch relative)
        float PADDLE_TOP_SIGNED = 180.0f - PADDLE_TOP;

        // Paddle height.
        float PADDLE_HEIGHT = 22.0f;

        // Paddle speed (per second).
        constexpr static float DEFAULT_PADDLE_SPEED = 4.0f * 30;

        // Time it takes to fully complete the change of paddle width.
        double PADDLE_TRANSITION_DURATION = 1.0;

        // Default ball speed.
        float BALL_SPEED = 100.0f;

        // Ball flash expiry, the time it takes to do so for the time it was hit. Used for judgment showing.
        double BALL_FLASH_EXPIRY = 20.0 / 30;

        // Ball flash size. (diameter)
        float BALL_FLASH_SIZE = 42.0f;
        //////////////////////////////////////////////////////////////////////////////////

        // Current beat.
        double beat = 0.0;

        // Current seconds in the ballfile.
        double seconds = 0.0;

        // Whether 'shaded' text should be drawn.
        bool shadedTextEnabled = false;

        // Whether input has been enabled in same frame.
        bool instantaneousInput = false;

        // Music offset (negative = music plays earlier, positive = music plays later)
        float musicOffset = 0.0;

        // Selected ball to place.
        SelectedPlaceableBall selectedToPlace = SelectedPlaceableBall::NORMAL;

        // Queued balls for playing/playtesting.
        std::vector<Ball> queuedBalls;

        // Queued ball flashes for playing/playtesting.
        std::vector<BallFlash> queuedBallFlashes;

        // Beat where playtesting started from.
        double startPlaytestingBeat = 0.0;

        // Paddle position in +- context.
        float paddlePosition = 0.0;

        // Game state.
        GameState gameState = GameState::EDITING_NONE;

        // Selected placing mode.
        PlacingMode placingMode = PlacingMode::BALL;

        // Text input by the user, used in various things.
        std::string inputText = "";

        // Music ID chosen by the user (only applicable in edit mode.)
        size_t musicId = 0;

        // Currently applying music ID (only applicable in edit mode.)
        size_t appliedMusicId = 0;

        // The minimum beat allowed for anything to be placed on.
        float startBeat = -INFINITY;
        // The maximum beat allowed for anything to be placed on.
        float endBeat = INFINITY;

        // List of song names and their authors found when loading the game that can be used for edits.
        std::vector<EditSong> editSongs = {};

        // Text input cursor position
        // Example:
        // = 0 `|ABC`
        // = 1 `A|BC`
        size_t inputCursor = 0;

        // BPM changes in the song.
        std::vector<BpmChange> bpmChanges = {(BpmChange) {0.0, DEFAULT_BPM}};

        // Paddle width changes in the ballfile.
        std::vector<PaddleWidthChange> paddleWidthChanges = {(PaddleWidthChange) {0.0, DEFAULT_PADDLE_WIDTH}};

        // Paddle speed changes in the ballfile.
        std::vector<PaddleSpeedChange> paddleSpeedChanges = {(PaddleSpeedChange) {0.0, DEFAULT_PADDLE_SPEED}};

        // Paddle dual changes in the ballfile.
        std::vector<PaddleDualChange> paddleDualChanges = {(PaddleDualChange) {0.0, false}};

        // Commands in the ballfile. (In this editor, nothing happens due to commands)
        std::vector<Command> commands = {};

        // Current circle speed for editing.
        float selectedSpeed = 1.0f;

        // Whether left mouse is held down.
        bool leftMouseHeld = false;

        // The offset of the beat lines (negative = up, positive = down)
        float beatLinesOffset = 0.0f;

        // The speed modifier of balls (global)
        float globalSpeedModifier = 1.0f;

        // Index of ball for checking for creation of holds.
        size_t ballCheckedForTail = SIZE_MAX;

        // Mouse position `[X, Y]`. Its range is never changed and uses SCREEN_WIDTH & SCREEN_HEIGHT.
        float mousePosition[2] = { INFINITY, INFINITY };

        // Paddle keys pressed.
        uint8_t paddleKeysPressed = 0b0000;
        
        // Current amount of space in `y` dimension between two beats.
        float beatSpacing = DEFAULT_BEAT_SPACING;

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

        // Mixer to use for playing audio.
        MIX_Mixer* mixer;

        // Music to play.
        MIX_Audio* music;

        // Track that plays the current string.
        MIX_Track* track;

        // Selected point's owner,
        std::optional<std::vector<Ball>::iterator> selectedPointOwner;

        // Selected point.
        std::optional<std::vector<BallTypeTailPoint>::iterator> selectedPoint;

        // Last error occured (for certain actions like saving files)
        std::string lastError;

        // The beginning of the preferences path.
        std::string prefPath;

        // Whether the point should be cloned.
        bool shouldClonePoint;

        // The base path of the game (its folder).
        std::filesystem::path basePath;

        // Start playing music.
        void startPlayingMusic(double seconds);

        // Start playtesting from a beat.
        void startPlayTest(double beat);

        // Stop playtesting.
        void stopPlayTest();

        // Draw playtest elements.
        void renderPlaytest();

        // Draw editor elements.
        void renderEditor();

        // Format millis into a readable timer string.
        std::string formatMillis(Sint64 millis);

        // Get the current status of music (using SDL3)
        std::string getMusicStatus();

        // Formats a float with the specified precision value.
        std::string formatFloat(float number, int precision);

        // Draws a beat in the editor with a specific `y` coordinate.
        void renderEditorBeatLine(size_t beat, float y);

        // Draws text at a position.
        void drawText(std::string text, SDL_Color color, float x, float y, TextAlignment align, float size);

        // Draws text at a position, with outline.
        void drawTextWithOutline(std::string str, SDL_Color fill, float x, float y, TextAlignment align, float size, int outline, SDL_Color outlineColor);

        // Draws a divisor arrow that should be drawn.
        void drawDivisorIndicator();

        // Draws a ghost ball that 'could' be put.
        void drawGhostBall();

        // Draws an additional editor menu depending on GameState.
        void drawSpecificEditorMenu();

        // Draw a white rectangle with the specified parameters.
        void drawWhiteRect(float x, float y, float w, float h);

        // Draw balls in the editor for editing.
        void drawEditorBalls(float endY);

        // Open a open file dialog and call the callback after it has been closed.
        void showOpenFileDialog(FileDataCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, bool allow_many);

        // Normal handler for opening file dialog.
        static void SDLCALL showOpenFileDialogNormal(void *userdata, const char *const *filelist, int filter);

        // Normal handler for saving file dialog.
        static void SDLCALL showSaveFileDialogNormal(void* userdata, const char* const* filelist, int filter);

        // Open a save file dialog and call the callback after it has been closed.
        void showSaveFileDialog(FileDataSaveCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location);

        // Common function to save a file for any platform. Returns the same as `SDL_SaveFile()`, and sets an error message if necessary.
        bool trySaveFile(const char *file, const void *data, size_t datasize, const char *defaultFileName);

        // Checks whether a 'ball' is fast enough to have a thunderbolt symbol.
        bool isFast(float ballSpeed);

        // Warps to the nearest beat that satisfies the divisor.
        void warpBeatToDivisor();

        // Checks if a ball can be placed at current mouse position.
        std::optional<GameplayLeftPosition> ballCanBePlaced();

        // Used for selecting hold points, for example.
        bool setTouchedPointOwner();

        // Clears point selected.
        void clearTouchedPointOwner();

        // Moves selected point.
        void moveCurrentPoint(bool clone);

        // Draws a ball in the editor.
        void drawEditorBall(const Ball& ball);

        // Adds a ball in the balls vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        size_t addBall(const Ball& ball);

        // Removes a ball in the balls vector, returning a new iterator to the vector for continuing looping. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        std::vector<Ball>::iterator removeBall(std::vector<Ball>::iterator ball);

        // Move divisor with an event so that other things can also be handled.
        void movesTimesDivisorWithEvent(float amount, SDL_Event *event);

        // Adds a bpm change in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addBpmChange(const BpmChange& bpmChange);

        // Adds a paddle width change in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addPaddleWidthChange(const PaddleWidthChange &paddleWidthChange);

        // Adds a paddle speed change in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addPaddleSpeedChange(const PaddleSpeedChange &paddleSpeedChange);

        // Adds a dual speed change in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addPaddleDualChange(const PaddleDualChange &paddleDualChange);

        // Adds a command in their vector. DO NOT CALL THIS FUNCTION AT THE SAME TIME THE VECTOR'S ITERATORS ARE USED!
        void addCommand(const Command &command);

        // Draws balls in the editor.
        void drawEditorBalls();

        // After copying balls to be queued, creates queued ball nodes, like from queued holds, for example.
        void setQueuedBalls(double startFrom);

        // Handles mouse wheel events.
        void handleMouseWheelEvent(SDL_Event* event);

        // Handles mouse button down events.
        void handleMouseButtonDownEvent(SDL_Event* event);

        // Handles mouse button up events.
        void handleMouseButtonUpEvent(SDL_Event* event);

        // Handle key down events.
        void handleKeyDownEvent(SDL_Event* event);

        // Opens the file picker for getting a ballfile.
        void openLoadBallFilePicker();

        // Opens the file picker for getting a file of commands.
        void openLoadCommandsPicker();

        // Opens the file picker for saving a ballfile.
        void openSaveBallFilePicker();

        // Opens the file pciker for saving a file of commands.
        void openSaveCommandsPicker();

        // Opens the file picker for loading music.
        void openLoadMusicPicker();

        // Returns true if no other event should be handled. Handles menu events.
        bool handleMenuEvent(SDL_Event* event);

        // Get current paddle speed.
        float getPaddleSpeed();

        // Checks if number input should be handled.
        InputMode inputModeEnabled();

        // Renders queued balls.
        void renderQueuedBalls();

        // Gets the rendered offset of queued balls.
        float queuedBallsYoffset();

        // Resets text input.
        void resetInput();

        // Move a certain number of minibeats according to current divisor.
        void moveTimesDivisor(float direction);

        // Callback for opening-a-file picker.
        static void callbackLoadBallfilePicker(void* userdata, void* contents, int filter, size_t sizeInBytes);

        // Callback for saving-a-file picker.
        static void callbackSaveBallfilePicker(void* userdata, const char* path, int filter);

        // Callback for saving commands.
        static void callbackSaveCommandsPicker(void* userdata, const char* path, int filter);

        // Callback for loading commands.
        static void callbackLoadCommandsPicker(void* userdata, void* contents, int filter, size_t sizeInBytes);

        // Callback for opening-music picker.
        static void callbackLoadMusicPicker(void* userdata, void* contents, int filter, size_t sizeInBytes);

        // Get displayed text string with |
        std::string getDisplayedInputText();

        // Gets the signed falling ball pos (`-180 - 180` range).
        float getSignedFallingBallPos(const Ball& ball);

        // Gets the signed y pos, like `getSignedFallingBallPos`.
        float getSignedYPosFromBeat(double otherBeat, float speed);

        // Gets the signed y pos, but assumes `against` as the current beat.
        float getSignedYPosFromBeatAgainstAnother(double otherBeat, float speed, double against);

        // Reverses the function of `getSignedYPosFromBeat`.
        double getBeatFromSignedYPos(float yPos, float speed);

        // Prepare to save/load preferences of the user. Also syncs and sets the beginning path of the preferences (as a pointer parameter).
        // Returns whether this was successful.
        bool beforePrefs(bool initFS);

        // Gets the ball size of a ball (i.e. diameter)
        float getBallSize(Ball& ball);

        // Gets the offset of the gameplay region.
        float getGameplayXoffset();

        // Gets the minibeat of the last point of a tailed ball's type (like a hold)
        // Returns 0 if non-existent.
        minibeat getMinibeatOfLastPoint(BallType& type);
        
        // Updates falling balls (in gameplay) and handles collision.
        void updateBalls();

        // Updates flashes when hitting balls.
        void updateFlashes();

        // Render flashes and the most recent judgment.
        void renderFlashesAndJudgment();

        // Gets judgment from position difference.
        Judgment getJudgmentFromDifference(float difference);

        // Gets judgment from milliseconds. (Squares)
        Judgment getJudgmentFromMilliseconds(double milliseconds);

        // Checks if a ball can be hit by a paddle. Non-interactable balls' ball bodies are not rendered.
        bool isQueuedBallInteractable(Ball& ball);

        // Handles hitting of a QUEUED ball. Returns iterator to next ball.
        std::vector<Ball>::iterator handleAfterQueuedBallHit(std::vector<Ball>::iterator it);

        // Renders an independent ball (except fragments, which are dependent.)
        void renderIndependentBall(const Ball& ball, SDL_FRect destRect, uint8_t alpha = 0xFFu);

        // Get the y-level of the editor selected beat.
        float getEditorSelectedBeatY();

        // Converts minibeats to its readable units (e.g. 48 -> 1 4th)
        std::string toReadableUnits(minibeat miniBeats);

        // Handles a custom placing mode key in the editor.
        void handleCustomPlacingModeKey(CustomPlacingModeKey key);

        // Value to tell the number of respawns a placed bouncy ball SHALL HAVE (editor)
        size_t editorBouncyRespawns = 1;

        // Value to tell the number of minibeats a placed bouncy ball's interval SHALL BE (editor)
        minibeat editorBouncyInterval = MINIBEATS_PER_BEAT;

        // Get flash color from a judgment.
        constexpr SDL_Color getFlashColor(Judgment judgment)
        {
            const SDL_Color colors[] = {
                SDL_Color { 0x00, 0x00, 0x00, 0x00 }, // no flash for miss
                SDL_Color { 0x87, 0xD5, 0xFF, 0x7F }, // bluish
                SDL_Color { 0x87, 0xFF, 0x9B, 0x7F }, // greenish
                SDL_Color { 0xDF, 0xFF, 0x87, 0x7F }, // yellowish
                SDL_Color { 0xFF, 0xFF, 0xFF, 0x7F }, // white - for held
                SDL_Color { 0xFF, 0xFF, 0xFF, 0xFF }, // custom explosion texture
                SDL_Color { 0x00, 0x00, 0x00, 0x00 }  // no flash for avoiding mines
            };
            return colors[judgment];
        }

        // Get text color from a judgment.
        constexpr SDL_Color getTextColor(Judgment judgment)
        {
            const SDL_Color colors[] = {
                SDL_Color { 0xFF, 0x87, 0x87, 0xFF }, // redish
                SDL_Color { 0x88, 0xDF, 0xFF, 0xFF }, // bluish
                SDL_Color { 0xAD, 0xFF, 0x87, 0xFF }, // greenish
                SDL_Color { 0xFF, 0xFA, 0x87, 0xFF }, // yellowish
                SDL_Color { 0xFF, 0xDF, 0x70, 0xFF }, // goldish
                SDL_Color { 0xAF, 0x6F, 0xFF, 0x00 }  // purpleish
            };
            return colors[judgment];
        }

        // Get text from a judgment.
        const std::string getText(Judgment judgment)
        {
            const std::string strs[] = {
                "Miss...",
                "Perfect!!",
                "Great!",
                "Good",
                "OK!",
                "Oops..."
            };
            return strs[judgment];
        }

        // Get text from a placing mode.
        const std::string getText(PlacingMode mode)
        {
            const std::string strs[] = {
                "Ball",
                "Mine",
                "Square",
                "Bouncy"
            };
            return strs[mode];
        }

        // Checks if a judgment's text should be considered for taking last judgment to be shown.
        bool isShownAsTextWhenLast(Judgment judgment)
        {
            return judgment <= Judgment::GOOD;
        }

    public:

        // Whether to close the program in the next frame.
        bool quit = false;

        // Whether to make the program only use touch features (no keyboard ones).
        bool touch = false;

        // Delta time passed since last frame.
        double deltaTimePassed;

        // The current rendering scale. Cached for performance.
        float renderedScale = 0;

        // Renderer for rendering the game.
        SDL_Renderer* renderer;

        // Window of the game.
        SDL_Window* window;

        // Font used in rendering the game.
        TTF_Font* font;

        // Extra font used in rendering outlined text. This is used for keeping the main font's cache.
        TTF_Font* fontOutlined;

        // Texture library in rendering the game.
        std::unique_ptr<TextureLibrary> textureLibrary;

        // Paddle keys pressed, but only there for 1 frame.
        uint8_t paddleKeysPressedOnce = 0b0000;

        // Constructor for the Game object.
        Game(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font, TTF_Font* fontOutlined, MIX_Mixer* mixer, std::filesystem::path basePath);

        // Function to update with new delta time.
        void update();

        // Get the seconds used by music.
        double currentMusicSeconds();

        // Function to render to the renderer.
        void render();

        // Draws a line marker with the offset of a beat from the currently selected beat in the editor (used for BPM changes, etc.)
        // It also draws text with different color parameters and a y text offset and alignment.
        void drawLineMarker(double fromSelectedBeat, SDL_Color lineColor, std::string str, SDL_Color textColor, int textOffset, TextAlignment alignment, float widthMul);

        // Get current paddle width.
        float getPaddleWidth();

        // Get paddle width at a beat.
        float getPaddleWidth(double beat);

        // Get whether the paddle is currently in dual mode or not.
        bool getPaddleDualMode(double beat);

        // Get the interpolated paddle width at the current beat.
        float getInterpolatedPaddleWidth();

        // Function to handle an SDL_Event.
        void handleEvent(SDL_Event* event);

        // Get seconds from beat.
        double getSecondsFromBeat(double beat);

        // Get beat from seconds.
        double getBeatFromSeconds(double seconds);

        // Get current window size.
        void getWindowSize(int* width, int* height);

        // Get current renderer size. (DPI makes this different from window size sometimes)
        void getRendererSize(int *width, int *height);

        // Get aspect ratio window size.
        void getAspectRatioWindowSize(float* width, float* height);

        // Get the width/height of pixels 'unused' due to scaling.
        void getUnusedPixels(float* left, float* top);

        // Get current scaling of elements in the game.
        float getScale();

        // Get current scaling of rendered elements in the game.
        float getRenderedScale();

        // Load preferences of the user.
        bool loadPrefs();

        // Save preferences of the user.
        bool savePrefs();

        // Applies the new BPM changes and music given by the `musicId` property.
        void applyNewEditSong();

        // Deconstructor for the Game object.
        ~Game();
        
        // Get the number of balls with a certain type.
        template <typename T>
        size_t getBallCount();
};

/*
 * Struct used for wrapping userdata, along with the game instance, an opening and saving file data callback.
 */
struct UserdataWrapper
{
    FileDataCallback callback;
    FileDataSaveCallback saveCallback;
    void* userdata;
    Game* game;
};

#endif