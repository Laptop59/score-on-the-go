#include "game.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>

    EM_JS(void*, js_SDL_malloc, (size_t size), {
        return ccall('SDL_malloc', 'number', ['number'], [size]);
    });
#endif

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <string>
#include <iostream>
#include <sstream>
#include <optional>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include "assert.h"

Game::Game(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font, TTF_Font* fontOutlined, MIX_Mixer* mixer, std::filesystem::path basePath)
{
    this->renderer = renderer;
    this->font = font;
    this->fontOutlined = fontOutlined;
    this->window = window;
    this->serializer.reset(new Serializer);
    
    // Allocate memory for filter
    this->fileFilter = new SDL_DialogFileFilter[2];
    this->fileFilter[0] = { "Ballfile", "txt" };
    this->fileFilter[1] = { "All files", "*" };

    this->mixer = mixer;
    this->track = MIX_CreateTrack(mixer);
    this->music = nullptr;

    // Search for any edit songs.
    #ifdef EDIT_MODE
        auto list = serializer->readSongList(basePath);
        if (std::holds_alternative<SerializerSongListSuccess>(list))
        {
            // Successful!
            this->editSongs = std::get<SerializerSongListSuccess>(list).editSongs;
            SDL_Log("%d", this->editSongs.size());
        }
        else
        {
            auto failure = std::get<SerializerFailure>(list).errors;
            std::string failMessage = std::string("Could not load songs.txt.");
            failMessage += "\n[ERRORS: " + std::to_string(failure.size()) + "]";
            size_t errorsLeft = 16;
            for (auto error = failure.begin(); error < failure.end(); ++error)
            {
                failMessage += "\nAt line " + std::to_string(error->line) + ": " + error->error; 
                if (!--errorsLeft) break;
            }

            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                NAME,
                failMessage.c_str(),
                this->window
            );

            SDL_Log("%s", failMessage.c_str());
        }
        applyNewEditSong();
    #endif

    beforePrefs(true);
}

Game::~Game()
{
    // Free file filter.
    delete this->fileFilter;

    // Free music.
    if (this->music != nullptr)
    {
        MIX_DestroyAudio(this->music);
    }

    MIX_DestroyTrack(this->track);
}

void Game::render()
{
    // Draw a BG.
    float scale = getRenderedScale();
    SDL_SetRenderScale(this->renderer, scale, scale);
    SDL_SetRenderDrawColor(this->renderer, 0x1F, 0x1F, 0x1F, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
    {
        // Draw another result where gameplay will take place.
        int height;
        getRendererSize(nullptr, &height);
        SDL_FRect gameplayRect =
            SDL_FRect
            {
                getGameplayXoffset(),
                0,
                GAMEPLAY_WIDTH,
                static_cast<float>(height) / getRenderedScale()
            };
        SDL_SetRenderDrawColor(this->renderer, 0x33, 0x33, 0x33, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(this->renderer, &gameplayRect);
        switch (gameState)
        {
            case PLAYTESTING:
                this->renderPlaytest();
                break;
            default:
                this->renderEditor();
        }
    }
    SDL_RenderPresent(this->renderer);
}

float Game::getPaddleWidth()
{
    return getPaddleWidth(this->beat);
}

float Game::getPaddleWidth(double beat)
{
    if (paddleWidthChanges.empty()) {
        return DEFAULT_PADDLE_WIDTH;
    }

    auto it = std::upper_bound(
        paddleWidthChanges.begin(), paddleWidthChanges.end(), beat,
        [](double value, const PaddleWidthChange& c){
            return value < c.beat;
        }
    ); // first iterator >= beat

    if (it == paddleWidthChanges.begin()) return it->width;
    it--; // last iterator < beat;
    return it->width;
}

bool Game::getPaddleDualMode(double beat)
{
    if (paddleDualChanges.empty()) {
        return false;
    }

    auto it = std::upper_bound(
        paddleDualChanges.begin(), paddleDualChanges.end(), beat,
        [](double value, const PaddleDualChange& c){
            return value < c.beat;
        }
    ); // first iterator >= beat

    if (it == paddleDualChanges.begin()) return it->enabled;
    it--; // last iterator < beat;
    return it->enabled;
}

float Game::getInterpolatedPaddleWidth()
{
    if (paddleWidthChanges.empty()) {
        return DEFAULT_PADDLE_WIDTH;
    }

    auto it = std::upper_bound(
        paddleWidthChanges.begin(), paddleWidthChanges.end(), this->beat,
        [](double value, const PaddleWidthChange& c){
            return value < c.beat;
        }
    ); // first iterator >= beat

    if (it == paddleWidthChanges.begin()) return it->width;
    it--; // last iterator < beat;
    
    float now = it->width;
    float ago = this->beat - it->beat;
    if (it == paddleWidthChanges.begin()) return it->width;
    it--;
    float earlier = it->width;

    if (ago > 0.5) return now;
    else {
        float t = 1 - ago / 0.5;
        return earlier + (now - earlier) * (1 - t*t);
    }
}

void Game::renderPlaytest()
{
    // Render paddle;
    for (size_t i = 0; i <= getPaddleDualMode(this->beat) ? 1 : 0; i++) {
        float pos = i == 1 ? -paddlePosition : paddlePosition;
        auto alpha = i == 1 ? 0xAAu : 0xFFu;
        SDL_FRect paddleRect = SDL_FRect {
            getGameplayXoffset() + GAMEPLAY_WIDTH / 2 + pos - getInterpolatedPaddleWidth() / 2,
            PADDLE_TOP + queuedBallsYoffset(),
            getInterpolatedPaddleWidth(),
            PADDLE_HEIGHT
        };
        SDL_SetRenderDrawColor(this->renderer, 0xFFu, 0xFFu, 0xFFu, alpha);
        SDL_RenderFillRect(this->renderer, &paddleRect);
        // Outline is 5 pixels. However it's an odd number, so 4 is good here.
        paddleRect.x += 2;
        paddleRect.y += 2;
        paddleRect.w -= 4;
        paddleRect.h -= 4;
        SDL_SetRenderDrawColor(this->renderer, 0x33u, 0x33u, 0x33u, alpha);
        SDL_RenderFillRect(this->renderer, &paddleRect);
    }
    

    SDL_Color whiteColor = SDL_Color { 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    float x = getGameplayXoffset() + GAMEPLAY_WIDTH + 5;

    DRAW_TEXT_LINES(3, 8, 0.35f) {
        DRAW_TEXT_W("Beat: " + formatFloat(this->beat, 3));
        DRAW_TEXT_W("Seconds: " + formatFloat(getSecondsFromBeat(this->beat), 3));

        LEAVE_LINE();

        DRAW_TEXT_W("Paddle Width: " + formatFloat(getPaddleWidth(), 1));
        DRAW_TEXT_W("(interpolated " + formatFloat(getInterpolatedPaddleWidth(), 2) + ")");

        LEAVE_LINE();

        DRAW_TEXT_W("Paddle Speed: " + formatFloat(getPaddleSpeed(), 1));

        LEAVE_LINE();

        DRAW_TEXT_W("Paddle Dual Mode: ");
        DRAW_TEXT_W(getPaddleDualMode(this->beat) ? "Enabled" : "Disabled");

        LEAVE_LINE();

        DRAW_HELP("ESC", "Stop Playtest");
    }

    // Render balls.
    renderQueuedBalls();
}

bool Game::isQueuedBallInteractable(Ball& ball)
{
    if (std::holds_alternative<BallTypeHold>(ball.type))
    {
        // Check if it was already hit.
        BallTypeHold& data = std::get<BallTypeHold>(ball.type);
        if (data.hit)
            return false;
    }
    if (std::holds_alternative<BallTypePit>(ball.type))
    {
        // Check if it was already hit.
        BallTypePit& data = std::get<BallTypePit>(ball.type);
        if (data.hit)
            return false;
    }
    return true;
}

float Game::getBallSize(Ball& ball)
{
    if (std::holds_alternative<BallTypeHoldFragment>(ball.type))
        return TAIL_SIZE;
    if (std::holds_alternative<BallTypePitFragment>(ball.type))
        return TAIL_SIZE;
    return BALL_SIZE;
}

float Game::getGameplayXoffset() {
    float left;
    getUnusedPixels(&left, nullptr);
    return (left / 2) / getRenderedScale() + GAMEPLAY_OFFSET;
}

void Game::renderQueuedBalls()
{
    SDL_FRect srcRect, destRect;
    // Render the tail of long balls.
    // TODO TO FIX ERRORS: Find a way to render an already-hit queued hold ball's tail.
    for (auto ball = queuedBalls.begin();
        ball < queuedBalls.end();
        ++ball)
    {
        BallType& type = ball->type;
        if (!std::holds_alternative<BallTypeHold>(type) &&
            !std::holds_alternative<BallTypePit>(type))
        {
            continue; // Not a long note.
        }
        float renderY = getSignedFallingBallPos(*ball) + TAIL_SIZE / 2;
        if (renderY > SCREEN_HEIGHT / 2 + TAIL_SIZE / 2 + queuedBallsYoffset()) continue; // Off-screen, skip rendering.
        std::vector<BallTypeTailPoint>* ptr = Serializer::getPointsFromType(type);
        if (ptr == nullptr) continue;
        // Find tail beat.
        double tailBeat = (double) getMinibeatOfLastPoint(type) + ball->at;
        std::vector<BallTypeTailPoint>& points = *ptr;
        if (points.empty()) continue;
        tailBeat /= MINIBEATS_PER_BEAT;
        float x; // This will be used for where to place the tail.
        //
        auto it = points.begin();
        float x1 = ball->x, x2 = it->x;
        minibeat y1 = ball->at, y2 = it->minibeats + ball->at;
        //
        double holdStartsFrom = std::max((double) ball->at / MINIBEATS_PER_BEAT, this->beat);
        if (tailBeat <= holdStartsFrom) continue; // Don't want unnecessary looping.
        // Create a texture.
        float renderedScale = getRenderedScale();
        float renderedWidth = renderedScale * SCREEN_WIDTH;
        float renderedHeight = renderedScale * SCREEN_HEIGHT;
        int totalWidth, totalHeight;
        getRendererSize(&totalWidth, &totalHeight);
        SDL_Texture* texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, totalWidth, totalHeight);
        if (texture == NULL) continue;
        // Change target of the renderer to render to the texture.
        SDL_SetRenderTarget(this->renderer, texture);
        SDL_SetRenderDrawColor(this->renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(this->renderer);
        //
        float yStart = getSignedYPosFromBeat(holdStartsFrom, ball->speed);
        float yEnd = getSignedYPosFromBeat(tailBeat, ball->speed);
        for (float y = yStart; y <= yEnd; y += 1.0f)
        {
            double yb = getBeatFromSignedYPos(y, ball->speed);
            if (yb < 0) yb = 0.0;
            minibeat ymb = yb * MINIBEATS_PER_BEAT;
            while (ymb > y2)
            {
                // Go to next point.
                x1 = x2;
                y1 = y2;
                ++it;
                // Break if iterator of points has reached its end.
                if (it == points.end()) break;
                x2 = it->x;
                y2 = it->minibeats + ball->at;
            }
            // Break if no more points are available.
            if (it == points.end()) break;
            ASSERT(y1 <= y2);
            double slope = (double) (y2 - y1);
            if (slope == 0)
                x = x2;
            else
                x = x1 + (x2 - x1) / slope * (ymb - y1);
            destRect = SDL_FRect {
                (x + getGameplayXoffset() + GAMEPLAY_WIDTH / 2 - TAIL_SIZE / 2) * renderedScale,
                (GAMEPLAY_HEIGHT / 2 - y - TAIL_SIZE / 2 + queuedBallsYoffset()) * renderedScale,
                TAIL_SIZE * renderedScale,
                TAIL_SIZE * renderedScale
            };
            SDL_Color color;
            if (std::holds_alternative<BallTypePit>(type)) color = SDL_Color { 0xFF, 0xFF, 0xFF, 0x6F };
            else {
                color = ball->getColorDivisor().getProtrayingColor();
                color.a = 0xBF;
            }
            SDL_SetTextureColorMod(
                textureLibrary->tail,
                color.r,
                color.g,
                color.b
            );
            SDL_Texture* tex = std::holds_alternative<BallTypePit>(type) ? textureLibrary->pitTail : textureLibrary->tail;
            SDL_SetTextureAlphaMod(tex, color.a);
            SDL_RenderTexture(this->renderer, tex, NULL, &destRect);
            SDL_SetTextureColorMod(
                tex,
                0xFF,
                0xFF,
                0xFF
            );
            SDL_SetTextureAlphaMod(tex, 0xFF);
        }
        // Let the renderer render to the window again.
        SDL_SetRenderTarget(this->renderer, NULL);
        SDL_RenderTexture(this->renderer, texture, NULL, NULL);
        SDL_DestroyTexture(texture);
    }
    // Render ball circles.
    float sizeOffsetBase = (1 - fmodf(beat, 1));
    float sizeOffset = sizeOffsetBase * sizeOffsetBase;
    for (auto ball = queuedBalls.begin();
        ball < queuedBalls.end();
        ++ball)
    {
        if (!isQueuedBallInteractable(*ball)) continue;            // Is not rendered.
        float size = getBallSize(*ball) * (1 + sizeOffset * (std::holds_alternative<BallTypeMine>(ball->type) ? -0.05 : 0.1));
        float renderY = getSignedFallingBallPos(*ball) + size / 2 - queuedBallsYoffset();
        if (renderY > SCREEN_HEIGHT / 2 + size) continue; // Off-screen, skip rendering.
        float renderX = ball->x - size / 2;
        destRect = SDL_FRect {
            renderX + getGameplayXoffset() + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2 - renderY,
            size,
            size
        };
        // Do not render hold fragments as normal balls.
        std::optional<SDL_Color> fragmentColor;
        if (std::holds_alternative<BallTypeHoldFragment>(ball->type))
            fragmentColor = SDL_Color { 0xCF, 0xCF, 0xCF, 0xAF };
        else if (std::holds_alternative<BallTypePitFragment>(ball->type))
            fragmentColor = SDL_Color { 0xCF, 0x7F, 0x7F, 0xAF };

        if (fragmentColor.has_value())
        {
            SDL_Color color = fragmentColor.value();
            SDL_SetTextureColorMod(
                textureLibrary->tail,
                color.r,
                color.g,
                color.b
            );
            SDL_SetTextureAlphaMod(textureLibrary->tail, color.a);
            SDL_RenderTexture(this->renderer, this->textureLibrary->tail, NULL, &destRect);
            SDL_SetTextureColorMod(
                textureLibrary->tail,
                0xFF,
                0xFF,
                0xFF
            );
            SDL_SetTextureAlphaMod(textureLibrary->tail, 0xFF);
            continue; // Go to the next ball.
        }
        renderIndependentBall(*ball, destRect);
        if (std::holds_alternative<BallTypeBouncy>(ball->type))
        {
            BallTypeBouncy& ballTypeBouncy = std::get<BallTypeBouncy>(ball->type);
            // Create some fake balls.
            for (minibeat i = 0; i <= ballTypeBouncy.respawns; i++)
            {
                Ball fake(ball->at + i * ballTypeBouncy.interval, ball->speed, ball->x);
                float fakeY = getSignedFallingBallPos(fake) + size / 2 - queuedBallsYoffset();
                if (fakeY > SCREEN_HEIGHT / 2 + size) break; // Off-screen, skip rendering.
                destRect = SDL_FRect {
                    renderX + getGameplayXoffset() + GAMEPLAY_WIDTH / 2,
                    GAMEPLAY_HEIGHT / 2 - fakeY,
                    size,
                    size
                };
                renderIndependentBall(fake, destRect, 0x3F);
            }
        }
    }
    this->renderFlashesAndJudgment();
}

float Game::queuedBallsYoffset()
{
    int height;
    getRendererSize(nullptr, &height);
    float scale = getRenderedScale();
    float normalHeight = scale * GAMEPLAY_HEIGHT;
    return (height - normalHeight) / getRenderedScale();
}

void Game::renderIndependentBall(const Ball& ball, SDL_FRect destRect, uint8_t alpha)
{   
    minibeat miniBeatsOfBall = ball.at;
    ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeatsOfBall);
    unsigned int id;
    SDL_FRect srcRect;
    SDL_Texture* texture;
    bool isMineLike = std::holds_alternative<BallTypeMine>(ball.type) ||
        std::holds_alternative<BallTypePit>(ball.type);
    if (isMineLike)
    {
        id = 0;
        texture = this->textureLibrary->mine;
    }
    else if (std::holds_alternative<BallTypeSquare>(ball.type))
    {
        id = 2 * colorDivisor.getColor() + isFast(ball.speed);
        texture = this->textureLibrary->squares;
    }
    else
    {
        id = 2 * colorDivisor.getColor() + isFast(ball.speed);
        texture = this->textureLibrary->balls;
    }
    srcRect = this->textureLibrary->createRect(
        id * 128,
        0,
        128,
        128
    );
    SDL_SetTextureAlphaMod(texture, alpha);
    if (isMineLike)
    {
        // We need rotation for mines.
        double angleDeg = 90.0 * (this->beat - (double) ball.at / MINIBEATS_PER_BEAT);
        SDL_RenderTextureRotated(this->renderer, texture, &srcRect, &destRect,
            angleDeg, NULL, SDL_FLIP_NONE);
    }
    else
        SDL_RenderTexture(this->renderer, texture, &srcRect, &destRect);
    SDL_SetTextureAlphaMod(texture, 0xFFu);
    if (std::holds_alternative<BallTypeBouncy>(ball.type))
    {
        // Display text for bounce!!
        BallTypeBouncy bounce = std::get<BallTypeBouncy>(ball.type);
        if (bounce.respawns)
        {
            minibeat toNext = miniBeatsOfBall + bounce.interval;
            ColorDivisor nextColorDivisor = ColorDivisor::getColorDivisor(toNext);
            drawTextWithOutline(
                std::to_string(bounce.respawns + 1), 
                nextColorDivisor.getTextColor(),
                destRect.x + destRect.w / 2,
                destRect.y + destRect.h / 2,
                TextAlignment::CENTER_ALIGNED,
                1.0f,
                4,
                SDL_Color { 0xFF, 0xFF, 0xFF, 0xFF }
            );
        }
    }
}

float Game::getEditorSelectedBeatY()
{
    return EDITOR_SELECTED_BEAT_Y + this->beatLinesOffset;
}

void Game::renderEditor()
{
    float y;
    // For rendering...
    size_t earlier_beat = (size_t) std::floor(this->beat);
    size_t later_beat = (size_t) std::ceil(this->beat);
    if (earlier_beat == later_beat) later_beat++;

    float endY = GAMEPLAY_HEIGHT;
    float yOffset;
    this->getUnusedPixels(nullptr, &yOffset);
    endY += yOffset / this->getScale();

    // White lines, and translucent ones.
    y = getEditorSelectedBeatY() + this->beatSpacing * (earlier_beat - this->beat);
    do
    {
        this->renderEditorBeatLine(earlier_beat, y);
        y -= this->beatSpacing;
        if (earlier_beat > 0)
            earlier_beat--;
        else
            break;
    }
    while (y + this->beatSpacing / 2 >= -25.0f);

    y = getEditorSelectedBeatY() + this->beatSpacing * (later_beat - this->beat);
    do
    {
        this->renderEditorBeatLine(later_beat, y);
        y += this->beatSpacing;
        later_beat++;
    }
    while (y <= endY + 25.0f);

    SDL_Color white = SDL_Color { 0xFF, 0xFF, 0xFF, 0xFF };
    DRAW_TEXT_LINES(3, 8, 0.35f) {
        DRAW_TEXT_W("Beat: " + formatFloat(this->beat, 3));
        DRAW_TEXT_W("(" + std::to_string(Ball::toMinibeats(this->beat)) + " mb)");
        DRAW_TEXT_W("Seconds: " + formatFloat(this->getSecondsFromBeat(this->beat), 3));
        LEAVE_LINE();
        DRAW_TEXT_W("Ball Speed: " + formatFloat(this->selectedSpeed, 3));
        DRAW_TEXT_W("Music: " + getMusicStatus());
        if (this->music)
        {
            Sint64 audioFrames = MIX_GetAudioDuration(this->music);
            if (audioFrames != MIX_DURATION_UNKNOWN)
            {
                if (audioFrames == MIX_DURATION_INFINITE)
                {
                    DRAW_TEXT_W("(Infinite)");
                }
                else
                {
                    // Format the frames.
                    Sint64 millis = MIX_AudioFramesToMS(this->music, audioFrames);
                    std::string time = formatMillis(millis);
                    DRAW_TEXT_W("(" + time + ")");
                }
            }
        }
        DRAW_TEXT_W("Music Offset: " + formatFloat(musicOffset, 3));
        LEAVE_LINE();
        DRAW_TEXT_W("Mode: " + getText(this->placingMode));
        size_t i = 0;
        switch (this->placingMode)
        {
            case PlacingMode::BOUNCY:
                // Show extra attributes.
                DRAW_TEXT_W("Respawns (R): " + std::to_string(this->editorBouncyRespawns));
                DRAW_TEXT_W("I: " + toReadableUnits(this->editorBouncyInterval) + " (" + std::to_string(this->editorBouncyInterval) + ")");
                DRAW_HELP("1/2", "+/- 1");
                DRAW_HELP("3/4", "+/- 16th");
                DRAW_HELP("5/6", "+/- 48th");
                DRAW_HELP("7/8", "+/- 192nd");
                DRAW_HELP("9/0", "R=1/I=48");
                break;
            default: break;
        }
        LEAVE_LINE();
        DRAW_TEXT_W("Balls - " + std::to_string(this->balls.size()));
        DRAW_TEXT_W("-------------------");
        DRAW_TEXT_W("Normal: " + std::to_string(getBallCount<BallTypeNormal>()));
        DRAW_TEXT_W("Mines: " + std::to_string(getBallCount<BallTypeMine>()));
        DRAW_TEXT_W("Holds: " + std::to_string(getBallCount<BallTypeHold>()));
        DRAW_TEXT_W("Pits: " + std::to_string(getBallCount<BallTypePit>()));
        DRAW_TEXT_W("Bouncy: " + std::to_string(getBallCount<BallTypeBouncy>()));
        LEAVE_LINE();

        DRAW_TEXT_W("Lines Offset: " + formatFloat(this->beatLinesOffset, 1));
        DRAW_TEXT_W("Global Speed: " + formatFloat(this->globalSpeedModifier, 3));
        DRAW_TEXT_W("Spacing: " + formatFloat(this->beatSpacing, 2));

        LEAVE_LINE();
        DRAW_HELP("F1", "Help");
    }

    this->drawEditorBalls(endY);
    this->drawDivisorIndicator();

    this->drawSpecificEditorMenu();
}

template <typename T>
size_t Game::getBallCount() {
    return std::count_if(balls.begin(), balls.end(), [](auto& ball) { return std::holds_alternative<T>(ball.type); });
}

std::string Game::formatMillis(Sint64 millis)
{
    Sint64 centis = millis / 10;
    Sint64 seconds = centis / 100;
    centis -= seconds * 100;
    Sint64 minutes = seconds / 60;
    seconds -= minutes * 60;

    std::string centisString = std::to_string(centis);
    if (centisString.size() == 1) centisString = "0" + centisString;

    std::string sString = std::to_string(seconds);
    if (sString.size() == 1) sString = "0" + sString;

    if (minutes >= 0)
    {
        return std::to_string(minutes) + ":" + sString + "." + centisString;
    }
    else
    {
        return sString + "." + centisString;
    }
}

std::string Game::getMusicStatus()
{
    if (!this->track)
        return "Unsupported";
    else if (!this->music)
        return "Unloaded";
    else
        return "Loaded";
}

std::string Game::formatFloat(float number, int precision)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << number;
    return ss.str();
}

void Game::handleCustomPlacingModeKey(CustomPlacingModeKey key)
{
    ASSERT(gameState == GameState::EDITING_NONE);
    switch (placingMode)
    {
        case PlacingMode::BOUNCY:
        {
            if (key == CustomPlacingModeKey::KEY_0)
                return (void) (editorBouncyInterval = MINIBEATS_PER_BEAT);
            if (key == CustomPlacingModeKey::KEY_9)
                return (void) (editorBouncyRespawns = 1);
            if (key == CustomPlacingModeKey::KEY_2)
            {
                if (editorBouncyRespawns > 1)
                    editorBouncyRespawns--;
                return;
            }
            if (key == CustomPlacingModeKey::KEY_1)
            {
                if (editorBouncyRespawns < 98)
                    editorBouncyRespawns++;
                return;
            }
            // Define a set of changes for INTERVAL:
            const sminibeat CHANGES[] = {
                12_smb,  // [3]
                -12_smb, // [4]
                4_smb,   // [5]
                -4_smb,  // [6]
                1_smb,   // [7]
                -1_smb,  // [8]
                0_smb
            };
            sminibeat change = CHANGES[key - 3];
            if (!change) return; // No need to calculate.
            sminibeat newInterval = editorBouncyInterval + change;
            if (newInterval <= 0) return; // Stop the operation if it becomes non +ve.
            editorBouncyInterval = (minibeat) newInterval;
        }
        default: break;
    }
}

std::string Game::toReadableUnits(minibeat miniBeats)
{
    ColorDivisor greatestDivisor = ColorDivisor::DIVISOR_4TH;
    while (miniBeats % greatestDivisor.getWorth())
        greatestDivisor = greatestDivisor.getNext();
    size_t amount = miniBeats / greatestDivisor.getWorth();
    return std::to_string(amount) + " " + greatestDivisor.getThString() + (
        amount != 1 ? "s" : ""
    );
}

void Game::drawSpecificEditorMenu()
{
    if (gameState == GameState::PLAYING || gameState == GameState::EDITING_NONE) return;
    SDL_SetRenderDrawColor(this->renderer, 0x00u, 0x00u, 0x00u, 0xAFu);
    SDL_RenderFillRect(this->renderer, NULL);
    // Middle x-position & y-position
    int width, height;
    getRendererSize(&width, &height);
    width /= getRenderedScale();
    height /= getRenderedScale();
    float x = width / 2;
    float y = height / 2;
    switch (gameState)
    {
        case GameState::EDITING_BPM:
            drawText(
                "Change BPM", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to:",
                ChangeColors::BPM_LINE,
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                ChangeColors::BPM_TEXT,
                x, y + 50, TextAlignment::CENTER_ALIGNED, 1.5f
            );
            break;

        case GameState::EDITING_PW:
            drawText(
                "Change Paddle Width", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to:",
                ChangeColors::PADDLE_WIDTH_LINE,
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                ChangeColors::PADDLE_WIDTH_TEXT,
                x, y + 50, TextAlignment::CENTER_ALIGNED, 1.5f
            );
            break;

        case GameState::EDITING_PS:
            drawText(
                "Change Paddle Speed", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to:",
                ChangeColors::PADDLE_SPEED_LINE,
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                ChangeColors::PADDLE_SPEED_TEXT,
                x, y + 50, TextAlignment::CENTER_ALIGNED, 1.5f
            );
            break;

        case GameState::EDITING_DUAL:
            drawText(
                "Change Paddle Dual Mode", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to: (positive value = enabled)",
                ChangeColors::PADDLE_DUAL_LINE,
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                ChangeColors::PADDLE_DUAL_TEXT,
                x, y + 50, TextAlignment::CENTER_ALIGNED, 1.5f
            );
            break;

        case GameState::EDITING_CMD:
            drawText(
                "Change Command", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to:",
                ChangeColors::BG_COMMAND_LINE,
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                ChangeColors::BG_COMMAND_TEXT,
                x, y + 50, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                "Note: To specify multiple actions, separate by a comma like action1,action2,action3.",
                SDL_Color {0xAFu, 0xFFu, 0xAFu, 0xFFu},
                x, y + 100, TextAlignment::CENTER_ALIGNED, 0.5f
            );
            break;

        case GameState::EDITING_MUSIC: {
            drawText(
                "Change Music", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            EditSong currentSong = this->editSongs.at(musicId);
            drawText(
                currentSong.song,
                SDL_Color { 0xDFu, 0x8Fu, 0xDFu, 0xDFu },
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                currentSong.author,
                SDL_Color { 0xEFu, 0xAFu, 0xEFu, 0xFFu },
                x, y + 25, TextAlignment::CENTER_ALIGNED, 0.5f
            );
            break;
        }
        case GameState::EDITING_MUSIC_OFFSET:
            drawText(
                "Change Music Offset", WHITE,
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                getDisplayedInputText(),
                SDL_Color { 0xDFu, 0x6Fu, 0xDFu, 0xFFu },
                x, y + 50, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                "Note: Negative = music plays earlier, while Positive = music plays later.",
                SDL_Color {0xFFu, 0xAFu, 0xFFu, 0xFFu},
                x, y + 100, TextAlignment::CENTER_ALIGNED, 0.5f
            );
            break;

        case GameState::EDITING_HELP:
            drawTextWithOutline("HELP", WHITE, x, 10, CENTER_ALIGNED, 0.5f, 1, WHITE);
            shadedTextEnabled = true;
            DRAW_TEXT_LINES(5, 22, 0.39f)
            {
                DRAW_TEXT_W("GAMEPLAY");
                DRAW_HELP("ESC", "Stop Playtest");
                LEAVE_SPACE(6);
                DRAW_TEXT_W("EDITOR");
                DRAW_HELP("F1", "Open/Close Help Menu");
                DRAW_HELP("F2", "Start Playtest from Beginning");
                DRAW_HELP("F3", "Start Playtest from Current Beat");
                DRAW_HELP("+", "Increase Beat Spacing");
                DRAW_HELP("-", "Decrease Beat Spacing");
                DRAW_HELP(".", "Increase Ball Speed (+ CTRL and/or SHIFT for lower values) (+ ALT for global)");
                DRAW_HELP(",", "Decrease Ball Speed (+ CTRL and/or SHIFT for lower values) (+ ALT for global)");
                DRAW_HELP("↑/↓", "Move Current Beat (or use Mouse Scroll) (↑/↓ + ALT to change lines offset by little)");
                DRAW_HELP("←/→", "Change division (quantization)");
                DRAW_HELP("PgUp", "Move 8 divisions up (+ ALT to change lines offset greatly)");
                DRAW_HELP("PgDn", "Move 8 divisions down (+ ALT to change lines offset greatly)");
                DRAW_HELP("Home", "Move to beat 0");
                DRAW_HELP("End", "Move to beat of last ball");
                DRAW_HELP("ESC", "Escape from Current Menu");
                DRAW_HELP("L", "Load a Ballfile");
                DRAW_HELP("S", "Save a Ballfile");
                #ifdef EDIT_MODE
                    DRAW_HELP("M", "Select Music for Edit");
                #else
                    DRAW_HELP("M", "Load Audio for Playtesting (or + ALT to set music offset)");
                #endif
                DRAW_HELP("P", "Cycle Placing Mode");
                #ifndef EDIT_MODE
                    DRAW_HELP("B", "Add BPM Change");
                #endif
                DRAW_HELP("W", "Add Paddle Width Change");
                DRAW_HELP("R", "Reset Preferences to Defaults");
                DRAW_HELP("D", "Add Paddle Speed Change");
                DRAW_HELP("U", "Add Paddle Dual Change");
                #ifndef EDIT_MODE
                    DRAW_HELP("C", "Add Command");
                    DRAW_HELP("I/O", "Load/Save Commands");
                #endif
            }
            shadedTextEnabled = false;
            break;
        default: break;
    }
}

void Game::drawWhiteRect(float x, float y, float w, float h)
{
    SDL_FRect rect {x, y, w, h};
    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderFillRect(this->renderer, &rect);
}

void Game::drawEditorBalls(float endY)
{
    // We want to try to optimise drawing balls in the editor; there may be thousands of them
    // that won't even be shown to the user.

    // Minimum beat
    double min = beat;
    min -= (double) getEditorSelectedBeatY() / this->beatSpacing; // For above selected beat.
    min -= (double) BALL_SIZE / this->beatSpacing; // For ball texture to be drawn offscreen.
    min -= 1.0f; // Just to be safe.
    if (min < 0) min = 0;

    // Maximum beat
    double max = beat;
    max += (double) (endY - getEditorSelectedBeatY()) / this->beatSpacing; // For below gameplay screen, below selected beat.
    max += (double) BALL_SIZE / this->beatSpacing; // For ball texture to be drawn offscreen.
    max += 1.0f; // Just to be safe.
    if (max < 0) max = 0;

    // Now convert them to their minibeat counterparts.
    minibeat minibeatsMin = Ball::toMinibeats(min);      // Mininum minibeats for ball to be drawn.
    minibeat minibeatsMax = Ball::toMinibeats(max);      // Maximum minibeats for ball to be drawn.
    minibeat minibeatsCurrent = Ball::toMinibeats(beat); // Current minibeats.

    // Now draw lines.
    for (Ball& ball : this->balls)
    {
        std::vector<BallTypeTailPoint>* points = Serializer::getPointsFromType(ball.type);
        if (points != nullptr)
        {
            float x1 = ball.x, x2;
            minibeat y1 = ball.at, y2;
            for (auto point = points->begin(); point != points->end(); ++point)
            {
                x2 = point->x;
                y2 = ball.at + point->minibeats;
                {
                    // Draw points.
                    float p1 = x1 + getGameplayXoffset() + GAMEPLAY_WIDTH / 2;
                    float p2 = getEditorSelectedBeatY() + (Ball::toBeats(y1) - this->beat) * beatSpacing;
                    float p3 = x2 + getGameplayXoffset() + GAMEPLAY_WIDTH / 2;
                    float p4 = getEditorSelectedBeatY() + (Ball::toBeats(y2) - this->beat) * beatSpacing;
                    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                    SDL_RenderLine(this->renderer, p1, p2, p3, p4);
                }
                x1 = x2;
                y1 = y2;
            }
        }
    }

    // Draw BPM changes too.
    for (BpmChange& bpmChange : this->bpmChanges)
    {
        if (bpmChange.beat >= min && bpmChange.beat <= max)
        {
            double fromSelectedBeat = bpmChange.beat - this->beat;
            float y = getEditorSelectedBeatY() + this->beatSpacing * fromSelectedBeat;
            drawLineMarker(fromSelectedBeat, ChangeColors::BPM_LINE, formatFloat(bpmChange.bpm, 4),  ChangeColors::BPM_TEXT, 8, TextAlignment::RIGHT_ALIGNED, 1);
        }
    }

    // Draw PW changes too.
    for (PaddleWidthChange& paddleWidthChange : this->paddleWidthChanges)
    {
        if (paddleWidthChange.beat >= min && paddleWidthChange.beat <= max)
        {
            double fromSelectedBeat = paddleWidthChange.beat - this->beat;
            this->drawLineMarker(fromSelectedBeat, ChangeColors::PADDLE_WIDTH_LINE, formatFloat(paddleWidthChange.width, 0), ChangeColors::PADDLE_WIDTH_TEXT, -6, TextAlignment::CENTER_ALIGNED, 0.25);
        }
    }

    // Draw PS changes too.
    for (PaddleSpeedChange& paddleSpeedChange : this->paddleSpeedChanges)
    {
        if (paddleSpeedChange.beat >= min && paddleSpeedChange.beat <= max)
        {
            double fromSelectedBeat = paddleSpeedChange.beat - this->beat;
            this->drawLineMarker(fromSelectedBeat, ChangeColors::PADDLE_SPEED_LINE, formatFloat(paddleSpeedChange.speed, 0), ChangeColors::PADDLE_SPEED_TEXT, -6, TextAlignment::CENTER_ALIGNED, 0.75);
        }
    }

    // Draw DUAL changes too.
    for (PaddleDualChange& paddleDualChange : this->paddleDualChanges)
    {
        if (paddleDualChange.beat >= min && paddleDualChange.beat <= max)
        {
            double fromSelectedBeat = paddleDualChange.beat - this->beat;
            this->drawLineMarker(fromSelectedBeat, ChangeColors::PADDLE_DUAL_LINE, paddleDualChange.enabled ? "enabled" : "disabled", ChangeColors::PADDLE_DUAL_TEXT, 8, TextAlignment::CENTER_ALIGNED, 0.5);
        }
    }

    // Draw commands too.
    for (Command& command : this->commands)
    {
        if (command.beat >= min && command.beat <= max)
        {
            double fromSelectedBeat = command.beat - this->beat;
            this->drawLineMarker(fromSelectedBeat, ChangeColors::BG_COMMAND_LINE, command.action, ChangeColors::BG_COMMAND_TEXT, 8, TextAlignment::LEFT_ALIGNED, 0);
        }
    }

    // `0`: Ghost ball will not be drawn before a higher beat ball.
    // `1`: Ghost ball will be drawn before a higher beat ball.
    // `2`: Ghost ball is already drawn.
    int8_t ghostBallStage = 0;

    // Loop & draw.
    for (auto ball = this->balls.begin(); ball != this->balls.end(); ++ball)
    {
        if (ball->at > minibeatsCurrent && ghostBallStage == 1)
        {
            // Draw ghost ball.
            this->drawGhostBall();
            ghostBallStage++;
        }
        if (ball->at >= minibeatsMin && ball->at <= minibeatsMax)
        {
            drawEditorBall(*ball);
        }
        if (std::holds_alternative<BallTypeBouncy>(ball->type))
        {
            BallTypeBouncy& ballTypeBouncy = std::get<BallTypeBouncy>(ball->type);
            // Create some fake balls.
            for (minibeat i = 0; i <= ballTypeBouncy.respawns; i++)
            {
                Ball fake(ball->at + i * ballTypeBouncy.interval, ball->speed, ball->x);
                SDL_FRect rect = this->textureLibrary->createRect(
                    ball->x + getGameplayXoffset() + GAMEPLAY_WIDTH / 2 - (float) (BALL_SIZE) / 2,
                    getEditorSelectedBeatY() + (Ball::toBeats(ball->at + i * ballTypeBouncy.interval) - this->beat) * this->beatSpacing - (float) (BALL_SIZE) / 2,
                    BALL_SIZE,
                    BALL_SIZE
                );
                if (rect.y + BALL_SIZE / 2 < 0) continue;
                if (rect.y > SCREEN_HEIGHT) break;
                renderIndependentBall(fake, rect, 0x1F);
            }
        }
        if (minibeatsCurrent >= ball->at && ghostBallStage == 0)
        {
            ghostBallStage++;
        }
        if (ball->at > minibeatsCurrent && ghostBallStage == 1)
        {
            // Draw ghost ball.
            this->drawGhostBall();
            ghostBallStage++;
        }
    }

    if (ghostBallStage < 2)
        this->drawGhostBall(); // Draw it if it hasn't been drawn already.
}

void Game::drawLineMarker(double fromSelectedBeat, SDL_Color lineColor, std::string str, SDL_Color textColor, int textOffset, TextAlignment alignment, float widthMul)
{
    float y = getEditorSelectedBeatY() + this->beatSpacing * fromSelectedBeat;
    SDL_SetRenderDrawColor(this->renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
    SDL_RenderLine(
        this->renderer,
        getGameplayXoffset(), y,
        GAMEPLAY_WIDTH + getGameplayXoffset(), y
    );

    int x = 0;
    if (alignment == TextAlignment::LEFT_ALIGNED && widthMul == 1) x = 100;

    this->drawText(
        str,
        textColor,
        GAMEPLAY_WIDTH * widthMul + getGameplayXoffset() + x - 1,
        y + textOffset,
        alignment,
        0.375f
    );
}

void Game::showOpenFileDialog(FileDataCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, bool allow_many)
{
    #ifdef __EMSCRIPTEN__
        EM_ASM({
            var callbackPtr = $0;
            var userdataPtr = $1;
            var windowPtr = $2;
            var filtersPtr = $3;
            var nfilters = $4;
            var allowMany = $5;
            var voidPtrSize = $6;

            var input = document.createElement('input');
            input.multiple = allowMany;
            input.type = 'file';
            var allowAll = false;
            var allowedFilters = [];

            // Read the filters
            if (filtersPtr != 0) {
                var currentPtr = filtersPtr;
                for (var i = 0; i < nfilters; i++) {
                    currentPtr += voidPtrSize; // skip name
                    var patternPtr = getValue(currentPtr, '*');
                    currentPtr += voidPtrSize;

                    var pattern = UTF8ToString(patternPtr);
                    if (pattern != '*') {
                        // convert something like this jpg;png;svg to .jpg,.png,.svg
                        for (var filter of pattern.split(';')) {
                            allowedFilters.push('.' + filter);
                        }
                    }
                }

                if (!allowAll && allowedFilters.length > 0) {
                    input.accept = allowedFilters.join(',');
                }
            }

            input.oncancel = return_null_ptr;
            input.onchange = function(e) {
                var file = e.target.files[0];
                if (!file) {
                    return_null_ptr();
                    return;
                }
                var reader = new FileReader();
                reader.onload = function() {
                    try {
                        var arrayBuffer = reader.result;
                        var data = new Uint8Array(arrayBuffer);
                        // allocate memory
                        var ptr = js_SDL_malloc(data.byteLength);
                        Module.HEAPU8.set(data, ptr);
                        done(ptr, data.byteLength);
                    } catch(e) {
                        console.error("Could not read file: ", e);
                        return_null_ptr();
                    }
                };
                reader.onerror = return_null_ptr;
                reader.readAsArrayBuffer(file);
            };
            input.click();

            function return_null_ptr() {
                done(0, 0);
            }

            function done(sdlResultPtr, sizeInBytes) {
                // call callback
                wasmTable.get(callbackPtr)(userdataPtr, sdlResultPtr, -1, sizeInBytes);
            }
        }, callback, userdata, window, filters, nfilters, allow_many, sizeof(void*));
    #else
        // Userdata is our game object.
        UserdataWrapper* wrapper = new UserdataWrapper {callback, NULL, userdata, this};
        SDL_ShowOpenFileDialog(&Game::showOpenFileDialogNormal, wrapper, window, filters, nfilters, default_location, allow_many);
    #endif
}

void SDLCALL Game::showOpenFileDialogNormal(void* userdata, const char* const* filelist, int filter)
{
    // Unwrap the wrapper.
    UserdataWrapper* wrapper = (UserdataWrapper*) userdata;
    FileDataCallback callback = wrapper->callback;
    void* actualUserdata = wrapper->userdata;
    char* fileContent = NULL;
    size_t sizeInBytes = 0;

    if (!filelist)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Load File Picker: %s", SDL_GetError());
    }
    else if (*filelist)
    {
        // Get the first item (we don't care about the other ones)
        const char* file = *filelist;

        // Read from the file.
        void* contents = SDL_LoadFile(file, &sizeInBytes);

        if (contents)
        {
            fileContent = (char*) contents;
        }
        else
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL Error Loading File: %s", SDL_GetError());
        }
    }

    callback(actualUserdata, fileContent, filter, sizeInBytes);

    delete wrapper;
}

void Game::showSaveFileDialog(FileDataSaveCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location)
{
    #ifdef __EMSCRIPTEN__
        callback(userdata, NULL, 0);
    #else
        UserdataWrapper* wrapper = new UserdataWrapper {NULL, callback, userdata, this};
        SDL_ShowSaveFileDialog(&Game::showSaveFileDialogNormal, wrapper, window, filters, nfilters, default_location);
    #endif
}

void SDLCALL Game::showSaveFileDialogNormal(void* userdata, const char* const* filelist, int filter)
{
    // Unwrap the wrapper.
    UserdataWrapper* wrapper = (UserdataWrapper*) userdata;
    FileDataSaveCallback callback = wrapper->saveCallback;
    void* actualUserdata = wrapper->userdata;
    char* path = NULL;
    
    if (!filelist)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Save File Picker: %s", wrapper->game->lastError.c_str());
    }
    else if (*filelist)
    {
        // Get the first item (we don't care about the other ones)
        const char* path = *filelist;
        callback(actualUserdata, path, filter);
    }

    delete wrapper;
}

bool Game::trySaveFile(const char *file, const void *data, size_t datasize, const char *defaultFileName)
{
    #ifdef __EMSCRIPTEN__
        char* error = (char*) EM_ASM_PTR({
            var defFileNamePtr = $0;
            var dataPtr = $1;
            var dataSize = $2;

            // Make the user download the file.
            try {
                var arrayBuffer = Module.HEAPU8.subarray(dataPtr, dataPtr + dataSize);
                var link = document.createElement('a');
                var blob = new Blob([arrayBuffer]);
                link.href = URL.createObjectURL(blob);
                var defaultFileNameStr = UTF8ToString(defFileNamePtr);
                var customFileName = window.prompt('Enter the name you want to download your file as:', defaultFileNameStr);
                if (!customFileName) {
                    throw 'cancelled';
                }
                link.download = customFileName;
                document.body.appendChild(link);
                link.click();
                document.body.removeChild(link);
                URL.revokeObjectURL(link.href);
                return 0; // NULL
            } catch (e) {
                try {
                    // Allocate some memory on the stack
                    var string = e.toString();
                    var len = lengthBytesUTF8(string) + 1;
                    var ptr = js_SDL_malloc(len);
                    stringToUTF8(string, ptr, len);
                    return ptr;
                } catch (e2) {
                    return 0; // out of memory? this should not happen
                }
            }
        }, defaultFileName, (char*) data, datasize);

        // if NULL, successful.
        bool returnedBool = true;
        if (error) {
            returnedBool = false;
            this->lastError.assign(error);
            SDL_free(error);
        }
        return returnedBool;
    #else
        bool success = SDL_SaveFile(file, data, datasize);
        if (!success) {
            this->lastError.assign(SDL_GetError());
        }
        return success;
    #endif
}


void Game::openLoadBallFilePicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    Game::showOpenFileDialog(
        &callbackLoadBallfilePicker,
        this,
        this->window,
        this->fileFilter,
        2,
        NULL,
        false
    );
}

void Game::openLoadCommandsPicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    const SDL_DialogFileFilter filters[] = {
        { "Background Commands", "bgc" },
    };
    Game::showOpenFileDialog(
        &callbackLoadCommandsPicker,
        this,
        this->window,
        (SDL_DialogFileFilter*) filters,
        1,
        NULL,
        false
    );
}

void Game::openLoadMusicPicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    const SDL_DialogFileFilter musicFilters[] = {
        { "Audio files", "mp3;ogg" },
        { "All files", "*" }
    };
    Game::showOpenFileDialog(
        &callbackLoadMusicPicker,
        this,
        this->window,
        (SDL_DialogFileFilter*) musicFilters,
        2,
        NULL,
        false
    );
}

void Game::openSaveBallFilePicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    Game::showSaveFileDialog(
        &callbackSaveBallfilePicker,
        this,
        this->window,
        this->fileFilter,
        2,
        NULL
    );
}

void Game::openSaveCommandsPicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    const SDL_DialogFileFilter filters[] = {
        { "Background Commands", "bgc" },
    };
    Game::showSaveFileDialog(
        &callbackSaveCommandsPicker,
        this,
        this->window,
        (SDL_DialogFileFilter*) filters,
        1,
        NULL
    );
}

void Game::callbackSaveBallfilePicker(void* userdata, const char* path, int filter)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;

    // Create necessary data for saving to the file.
    std::string text =
    #ifndef EDIT_MODE
        game->serializer->saveBallfile(
            game->balls,
            game->bpmChanges,
            game->paddleWidthChanges,
            game->paddleSpeedChanges,
            game->paddleDualChanges
        );
    #else
        game->serializer->saveCompressedBallfile(
            game->appliedMusicId,
            game->balls,
            game->paddleWidthChanges,
            game->paddleSpeedChanges,
            game->paddleDualChanges
        );
    #endif
    if (!game->serializer->errors.empty())
    {
        std::string failMessage = "";
        failMessage += "\n[ERRORS: " + std::to_string(game->serializer->errors.size()) + "]";
        size_t errorsLeft = 16;
        for (auto error = game->serializer->errors.begin(); error < game->serializer->errors.end(); ++error)
        {
            failMessage += "\n" + error->error; 
            if (!--errorsLeft) break;
        }
        std::string errorMessage = std::string("Could not save ballfile: \n") + game->lastError;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            errorMessage.c_str(),
            game->window
        );
        return;
    }

    // Attempt to save file.
    if (game->trySaveFile(path, text.c_str(), text.length(), "ballfile.txt"))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION,
            NAME,
            "Successfully saved ballfile.",
            game->window
        );
    }
    else if (game->lastError != CANCELLED)
    {
        std::string errorMessage = std::string("Could not save ballfile: \n") + game->lastError;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            errorMessage.c_str(),
            game->window
        );
    }
}

void Game::callbackSaveCommandsPicker(void* userdata, const char* path, int filter)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    // Get the first item (we don't care about the other ones)

    // Create necessary data for saving to the file.
    std::string text = game->serializer->writeCommands(
        game->commands
    );

    // Attempt to save file.
    if (game->trySaveFile(path, text.c_str(), text.length(), "commands.bgc"))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION,
            NAME,
            "Successfully saved commands.",
            game->window
        );
    }
    else if (game->lastError != CANCELLED)
    {
        std::string errorMessage = std::string("Could not save commands: \n") + game->lastError;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            errorMessage.c_str(),
            game->window
        );
    }
}

void Game::callbackLoadCommandsPicker(void* userdata, void* contents, int filter, size_t sizeInBytes)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (contents == NULL) return;
    std::vector<Command> result = game->serializer->readCommands((char *) contents, sizeInBytes);
    if (result.empty())
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            "No commands could be loaded from the file - no change has been done to your existing commands.",
            game->window
        );
    }
    else
    {
        game->commands.clear();

        for (Command& command : result)
            game->commands.push_back(command);

        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            (std::string("Successfully loaded ") + std::to_string(result.size()) + "commands.").c_str(),
            game->window
        );
    }
    
    SDL_free(contents);
}

void Game::callbackLoadMusicPicker(void* userdata, void* contents, int filter, size_t sizeInBytes)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (contents == NULL) return;
    // Load the music.
    MIX_Audio* music = MIX_LoadAudio_IO(game->mixer, SDL_IOFromMem(contents, sizeInBytes), false, true);

    if (music == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Loading Music: %s", SDL_GetError());
        return;
    }

    // Free the existing music.
    if (game->music != NULL)
    {
        MIX_DestroyAudio(game->music);
    }
    game->music = music;

    SDL_free(contents);
}

void Game::callbackLoadBallfilePicker(void* userdata, void* contents, int filter, size_t sizeInBytes)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (contents == NULL) return;
    char* text = (char *) contents;
    bool isProbablyUncompressed = false;
    size_t i = 0;
    while (i < sizeInBytes)
    {
        char ch = text[i++];
        if (ch == '\n' || ch == ':') {
            isProbablyUncompressed = true;
            break;
        }
    }
    SerializerResult result = isProbablyUncompressed ?
        game->serializer->readBallfile(text, sizeInBytes, false) :
        game->serializer->readCompressedBallfile(text, sizeInBytes);
    if (std::holds_alternative<SerializerSuccess>(result))
    {
        SerializerSuccess success = std::get<SerializerSuccess>(result);

        game->balls.clear();
        for (auto ball = success.balls.begin(); ball < success.balls.end(); ++ball)
            game->addBall(*ball);

        if (isProbablyUncompressed) {
            game->bpmChanges.clear();
            for (const auto& bpmChange : success.bpmChanges)
                game->addBpmChange(bpmChange);
        }

        game->paddleWidthChanges.clear();
        for (const auto& paddleWidthChange : success.paddleWidthChanges)
            game->addPaddleWidthChange(paddleWidthChange);

        game->paddleSpeedChanges.clear();
        for (const auto& paddleSpeedChange : success.paddleSpeedChanges)
            game->addPaddleSpeedChange(paddleSpeedChange);

        game->paddleDualChanges.clear();
        for (const auto& paddleDualChanges : success.paddleDualChanges)
            game->addPaddleDualChange(paddleDualChanges);

        game->commands.clear();

        if (success.musicId > 0)
        {
            size_t newAppliedMusicId = success.musicId - 1;
            if (newAppliedMusicId != game->appliedMusicId) {
                game->musicId = newAppliedMusicId;
                game->appliedMusicId = newAppliedMusicId;
                game->applyNewEditSong();
            }
        }

        std::string successMessage = std::string("Successfully loaded a ball file with ");
        successMessage += std::to_string(success.balls.size());
        successMessage += " balls.";
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION,
            NAME,
            successMessage.c_str(),
            game->window
        );
    }
    else if (std::holds_alternative<SerializerFailure>(result))
    {
        SerializerFailure failure = std::get<SerializerFailure>(result);
        std::string failMessage = std::string("Could not load the ballfile.");
        failMessage += "\n[ERRORS: " + std::to_string(failure.errors.size()) + "]";
        size_t errorsLeft = 16;
        for (auto error = failure.errors.begin(); error < failure.errors.end(); ++error)
        {
            failMessage += 
            #ifdef EDIT_MODE
                "\nAt character " + std::to_string(error->line) + ": " + error->error;
            #else
                "\nAt line " + std::to_string(error->line) + ": " + error->error;
            #endif
            if (!--errorsLeft) break;
        }

        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            NAME,
            failMessage.c_str(),
            game->window
        );

        SDL_Log("%s", failMessage.c_str());
    }

    SDL_free(contents);
}

std::optional<GameplayLeftPosition> Game::ballCanBePlaced()
{
    GameplayLeftPosition pos;
    pos.x = mousePosition[0] - getGameplayXoffset();
    pos.y = mousePosition[1];

    if (std::abs(getEditorSelectedBeatY() - pos.y) <= EDITOR_RANGE_SELECTED_BEAT
        && pos.x >= 0
        && pos.x < GAMEPLAY_WIDTH)
    {
        return std::optional<GameplayLeftPosition>(pos);
    }
    return std::nullopt;
}

void Game::drawGhostBall()
{
    // Check if the range is satisfied.
    std::optional<GameplayLeftPosition> mousePos = this->ballCanBePlaced();
    if (mousePos)
    {
        SDL_FRect destRect = this->textureLibrary->createRect(
            mousePos->x + getGameplayXoffset() - (float) (BALL_SIZE) / 2,
            getEditorSelectedBeatY() - (float) (BALL_SIZE) / 2,
            BALL_SIZE,
            BALL_SIZE
        );
        Ball imaginaryBall(this->beat, this->selectedSpeed, 0);
        switch (this->placingMode)
        {
            case PlacingMode::BALL:
                imaginaryBall.type = BallTypeNormal {};
                break;
            case PlacingMode::MINE:
                imaginaryBall.type = BallTypeMine {};
                break;
            case PlacingMode::SQUARE:
                imaginaryBall.type = BallTypeSquare {};
                break;
            case PlacingMode::BOUNCY:
                imaginaryBall.type = BallTypeBouncy {
                    editorBouncyRespawns,
                    editorBouncyInterval,
                    -INFINITY
                };
                break;
            default:
                UNEXPECTED("Invalid placing mode attempted to be drawn as a ghost ball.");
        }
        renderIndependentBall(imaginaryBall, destRect, GHOST_BALL_ALPHA);
    }
}

void Game::drawEditorBall(const Ball& ball)
{
    minibeat miniBeatsAtEditor = Ball::toMinibeats(this->beat);
    minibeat miniBeatsOfBall   = ball.at;
    ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeatsOfBall);
    SDL_FRect srcRect = this->textureLibrary->createRect(
        (2 * colorDivisor.getColor() + isFast(ball.speed)) * BALL_SIZE,
        0,
        BALL_SIZE,
        BALL_SIZE
    );
    double fromSelectedBeat = Ball::toBeats(ball.at) - this->beat;
    SDL_FRect destRect = this->textureLibrary->createRect(
        ball.x + getGameplayXoffset() + GAMEPLAY_WIDTH / 2 - (float) (BALL_SIZE) / 2,
        getEditorSelectedBeatY() + fromSelectedBeat * this->beatSpacing - (float) (BALL_SIZE) / 2,
        BALL_SIZE,
        BALL_SIZE
    );
    renderIndependentBall(ball, destRect);
    float x = destRect.x + destRect.w * 0.5f;
    float y = destRect.y + destRect.h * 0.5f;
    SDL_Color speedTextColor = colorDivisor.getTextColor();
    SDL_Color speedOutlineColor = SDL_Color { 0x00, 0x00, 0x00, 0x00 };
    int outline = 0;
    if (std::holds_alternative<BallTypeMine>(ball.type) ||
        std::holds_alternative<BallTypePit>(ball.type))
    {
        outline = 5;
        speedOutlineColor = SDL_Color { 0xFF, 0xFF, 0xFF, 0xFF };
    }
    drawTextWithOutline(std::to_string(ball.speed).substr(0, 5), speedTextColor,
        x, y, TextAlignment::CENTER_ALIGNED, 0.45f, outline, speedOutlineColor);
}

void Game::renderEditorBeatLine(size_t beat, float y)
{
    uint8_t lightness = 0xE4;
    SDL_Color color {
        lightness, lightness, lightness, 0xFF
    };

    SDL_SetRenderDrawColor(this->renderer, lightness, lightness, lightness, 0xFF);
    for (int i = 0; i <= 1; i++)
        SDL_RenderLine(
            this->renderer,
            getGameplayXoffset(), y + i,
            GAMEPLAY_WIDTH + getGameplayXoffset(), y + i
        );

    // Print a beat number.
    std::string str = std::to_string(beat);

    this->drawTextWithOutline(
        str,
        color,
        getGameplayXoffset() - 2,
        y,
        TextAlignment::RIGHT_ALIGNED,
        0.37f,
        1,
        color
    );

    y += 0.5f * this->beatSpacing;

    lightness = 0x7F;
    SDL_SetRenderDrawColor(this->renderer, lightness, lightness, lightness, 0xFF);
    SDL_SetRenderDrawBlendMode(this->renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i <= 1; i++)
        SDL_RenderLine(
            this->renderer,
            getGameplayXoffset(), y + i,
            GAMEPLAY_WIDTH + getGameplayXoffset(), y + i
        );
}

void Game::drawTextWithOutline(std::string str, SDL_Color fill, float x, float y, TextAlignment align, float size, int outline, SDL_Color outlineColor)
{
    size = size * UNIT_FONT_SIZE;
    SDL_Surface* fillSurface = TTF_RenderText_Blended(
        this->font,
        str.c_str(),
        str.length(),
        fill
    );
    if (fillSurface == nullptr) return;

    SDL_Surface* outlineSurface = nullptr;
    if (size > 0)
    {
        TTF_SetFontOutline(fontOutlined, outline);
        outlineSurface = TTF_RenderText_Blended(
            this->fontOutlined,
            str.c_str(),
            str.length(),
            outlineColor
        );
        if (outlineSurface == nullptr) return;
    }
    // Convert them to textures.
    SDL_Texture* fillTexture = SDL_CreateTextureFromSurface(this->renderer, fillSurface);
    SDL_Texture* outlineTexture = nullptr;
    if (outlineSurface != nullptr)
        outlineTexture = SDL_CreateTextureFromSurface(this->renderer, outlineSurface);
    float textWidth1 = fillTexture->w * size;  // up/downsize width
    float textHeight1 = fillTexture->h * size; // up/downsize height
    float textWidth2 = 0.0f, textHeight2 = 0.0f;
    if (outlineTexture != nullptr)
    {
        textWidth2 = outlineTexture->w * size;  // up/downsize width
        textHeight2 = outlineTexture->h * size; // up/downsize height
    }
    float drawX1, drawX2;
    switch (align)
    {
        case TextAlignment::LEFT_ALIGNED:
            drawX1 = drawX2 = x;
            break;
        case TextAlignment::RIGHT_ALIGNED:
            drawX1 = x - textWidth1;
            drawX2 = x - textWidth2;
            break;
        case TextAlignment::CENTER_ALIGNED:
            drawX1 = x - textWidth1 / 2;
            drawX2 = x - textWidth2 / 2;
            break;
    }
    SDL_FRect destRect1 =
        {
            drawX1,
            y - textHeight1 / 2,
            textWidth1,
            textHeight1
        };
    SDL_FRect destRect2 =
        {
            drawX2,
            y - textHeight2 / 2,
            textWidth2,
            textHeight2
        };
    SDL_DestroySurface(fillSurface);
    SDL_DestroySurface(outlineSurface);
    if (outlineTexture != nullptr) {
        if (shadedTextEnabled)
            SDL_SetTextureBlendMode(outlineTexture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(this->renderer, outlineTexture, NULL, &destRect2);
    }
    if (shadedTextEnabled)
        SDL_SetTextureBlendMode(fillTexture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(this->renderer, fillTexture, NULL, &destRect1);
    SDL_DestroyTexture(fillTexture);
    if (outlineTexture != nullptr)
        SDL_DestroyTexture(outlineTexture);
}

void Game::drawText(std::string str, SDL_Color color, float x, float y, TextAlignment align, float size)
{
    return drawTextWithOutline(str, color, x, y, align, size, 0, SDL_Color { 0x00, 0x00, 0x00, 0x00} );
}

void Game::drawDivisorIndicator()
{
    SDL_FRect rect = {
        GAMEPLAY_WIDTH + getGameplayXoffset() + 6,
        getEditorSelectedBeatY() - SELECTOR_RECTANGLE_HEIGHT / 2,
        SELECTOR_RECTANGLE_WIDTH,
        SELECTOR_RECTANGLE_HEIGHT
    };

    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderFillRect(this->renderer, &rect);

    rect.x += SELECTOR_RECTANGLE_OUTLINE_THICKNESS;
    rect.y += SELECTOR_RECTANGLE_OUTLINE_THICKNESS;
    rect.w -= SELECTOR_RECTANGLE_OUTLINE_THICKNESS * 2;
    rect.h -= SELECTOR_RECTANGLE_OUTLINE_THICKNESS * 2;

    SDL_Color color = this->selectedDivisor.getProtrayingColor();
    SDL_SetRenderDrawColor(this->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(this->renderer, &rect);

    minibeat miniBeats = Ball::toMinibeats(this->beat);
    ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeats);

    color.r = color.r / 2 + 128;
    color.g = color.g / 2 + 128;
    color.b = color.b / 2 + 128;

    drawTextWithOutline(
        std::to_string(this->selectedDivisor.getTh()).c_str(),
        color,
        rect.x + rect.w / 2,
        rect.y + rect.h / 2,
        TextAlignment::CENTER_ALIGNED,
        0.5f,
        4,
        { 255, 255, 255, 255 }
    );

    drawTextWithOutline(
        std::to_string(this->selectedDivisor.getTh()).c_str(),
        color,
        rect.x + rect.w / 2,
        rect.y + rect.h / 2,
        TextAlignment::CENTER_ALIGNED,
        0.5f,
        2,
        { 0, 0, 0, 255 }
    );
}

bool Game::isFast(float ballSpeed)
{
    return ballSpeed - FAST_BALL_SPEED >= -0.00001f;
}

void Game::moveTimesDivisor(float direction)
{
    minibeat minibeats = Ball::toMinibeats(this->beat);
    // Add/Subtract required minibeats and convert back.
    minibeat netChange = this->selectedDivisor.getWorth() * std::abs(direction);
    if (direction < 0 && netChange >= minibeats)
    {
        this->beat = 0;
    }
    else
    {
        minibeat remainingMinibeats;
        if (direction > 0)
            remainingMinibeats = minibeats + netChange;
        else
            remainingMinibeats = minibeats - netChange;
        this->beat = Ball::toBeats(remainingMinibeats);
    }
    if (!leftMouseHeld) return;                     // Without left mouse holding, do not allow creation of long balls.
    if (balls.size() <= ballCheckedForTail) return; // Cannot be out of bounds.
    Ball& ball = balls.at(ballCheckedForTail);      // Then find the required ball.
    minibeat holdLength;
    if (ball.at < Ball::toMinibeats(this->beat))
    {
        holdLength = Ball::toMinibeats(this->beat) - ball.at;
    }
    else if (ball.at == Ball::toMinibeats(this->beat))
    {
        // Convert the ball back into a non-hold ball.
        if (std::holds_alternative<BallTypeHold>(ball.type))
        {
            ball.type = BallTypeNormal {};
        }
        if (std::holds_alternative<BallTypePit>(ball.type))
        {
            ball.type = BallTypeMine {};
        }
        return;
    }
    else return;
    if (std::holds_alternative<BallTypeNormal>(ball.type))
        // Normal --> Hold
        ball.type = BallTypeHold {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            },
            false
        };
    else if (std::holds_alternative<BallTypeMine>(ball.type))
        // Normal --> Hold
        ball.type = BallTypePit {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            },
            false
        };
    else if (std::holds_alternative<BallTypeHold>(ball.type))
        // Update the hold point.
        ball.type = BallTypeHold {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            },
            false
        };
    else if (std::holds_alternative<BallTypePit>(ball.type))
        // Update the hold point.
        ball.type = BallTypePit {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            },
            false
        };
}

void Game::handleMouseWheelEvent(SDL_Event* event)
{
    movesTimesDivisorWithEvent(-event->wheel.integer_y, event);
}

void Game::handleMouseButtonUpEvent(SDL_Event* event)
{
    switch (event->button.button)
    {
        case SDL_BUTTON_LEFT:
            leftMouseHeld = false;
            ballCheckedForTail = SIZE_MAX;
            clearTouchedPointOwner();
            break;
    }
}

void Game::clearTouchedPointOwner()
{
    selectedPointOwner = {};
    selectedPoint = {};
}

bool Game::setTouchedPointOwner()
{
    std::optional<std::vector<BallTypeTailPoint>::iterator> nearest = {};
    std::optional<std::vector<Ball>::iterator> owner = {};
    float leastDistanceSquared = INFINITY;
    for (auto ball = balls.begin(); ball != balls.end(); ++ball)
    {
        std::vector<BallTypeTailPoint>* points = Serializer::getPointsFromType(ball->type);
        if (points != nullptr)
        {
            std::vector<BallTypeTailPoint>& vector = *points;
            for (auto point = vector.begin(); point != vector.end(); ++point)
            {
                float mx = mousePosition[0] - getGameplayXoffset() - GAMEPLAY_WIDTH / 2;
                float dx = mx - point->x;
                if (std::abs(dx) > 10) continue;
                float my = mousePosition[1];
                float dy =
                    my - ((Ball::toBeats(ball->at + point->minibeats) - this->beat) * beatSpacing + getEditorSelectedBeatY());
                if (std::abs(dy) > 10) continue;
                float distanceSquared = dx * dx + dy * dy;
                if (leastDistanceSquared > distanceSquared)
                {
                    leastDistanceSquared = distanceSquared;
                    nearest = point;
                    owner = ball;
                }
            }
        }
    }
    selectedPoint = {};
    selectedPointOwner = {};
    if (!nearest) return false;
    selectedPoint = nearest;
    selectedPointOwner = owner;
    return true;
}

void Game::handleMouseButtonDownEvent(SDL_Event* event)
{
    std::optional<GameplayLeftPosition> ballPosition;
    switch (event->button.button)
    {
        case SDL_BUTTON_LEFT:
            leftMouseHeld = true;

            if (this->setTouchedPointOwner()) return;

            ballPosition = this->ballCanBePlaced();
            if (ballPosition)
            {
                // The ball can be placed. Add the ball.
                float ballX = ballPosition->x - GAMEPLAY_WIDTH / 2;
                Ball ball(this->beat, this->selectedSpeed, ballX);
                // Make required changes to it.
                if (placingMode == PlacingMode::MINE)
                {
                    BallTypeMine type;
                    ball.type = type;
                }
                else if (placingMode == PlacingMode::SQUARE)
                {
                    BallTypeSquare type;
                    ball.type = type;
                }
                else if (placingMode == PlacingMode::BOUNCY)
                {
                    BallTypeBouncy type {
                        editorBouncyRespawns,
                        editorBouncyInterval,
                        -INFINITY
                    };
                    ball.type = type;
                }
                // Add the ball.
                ballCheckedForTail = addBall(ball);
            }
            break;
        case SDL_BUTTON_RIGHT:
            // Delete an existing ball.
            ballPosition = this->ballCanBePlaced();
            if (ballPosition)
            {
                // The ball can be deleted. Find balls close to the range.
                float ballX = ballPosition->x - GAMEPLAY_WIDTH / 2;
                float leastDistance = INFINITY;
                minibeat minibeats = Ball::toMinibeats(this->beat);
                std::vector<Ball>::iterator closest;
                for (auto ball = balls.begin(); ball < balls.end(); ++ball)
                {
                    sminibeat distance = (sminibeat)(ball->at) - (sminibeat)(minibeats);
                    if (std::abs(ball->x - ballX) < 10.0f &&
                        std::abs(distance) < 24_smb)
                    {
                        // Delete the ball.
                        ball = removeBall(ball);
                        break;
                    }
                }
            }
            break;
    }
}

std::vector<Ball>::iterator Game::removeBall(std::vector<Ball>::iterator ball)
{
    clearTouchedPointOwner();
    return balls.erase(ball);
}

void Game::movesTimesDivisorWithEvent(float amount, SDL_Event* event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.mod & SDL_KMOD_ALT)
    {
        beatLinesOffset += amount;
        savePrefs();
    }
    else
    {
        moveTimesDivisor(amount);
    }
}

void Game::handleKeyDownEvent(SDL_Event* event)
{
    ColorDivisor toGoto;
    switch (event->key.key)
    {
        case SDLK_LEFT:
            // Go to previous divisor.
            toGoto = this->selectedDivisor.getPrevious();
            if (toGoto) this->selectedDivisor = toGoto;
            warpBeatToDivisor();
            break;
        case SDLK_RIGHT:
            // Go to next divisor.
            toGoto = this->selectedDivisor.getNext();
            if (toGoto) this->selectedDivisor = toGoto;
            warpBeatToDivisor();
            break;
        case SDLK_UP:
            movesTimesDivisorWithEvent(-1.0f, event);
            break;
        case SDLK_DOWN:
            movesTimesDivisorWithEvent(1.0f, event);
            break;
        case SDLK_PAGEUP:
            movesTimesDivisorWithEvent(-8.0f, event);
            break;
        case SDLK_PAGEDOWN:
            movesTimesDivisorWithEvent(8.0f, event);
            break;
        case SDLK_HOME:
            this->beat = 0;
            break;
        case SDLK_END:
            // Get the last ball.
            this->beat = this->balls.empty() ? 0 : Ball::toBeats(this->balls.rbegin()->at);
            break;
        case SDLK_EQUALS:
            // Increase spacing between two beats.
            beatSpacing += EDITOR_BEAT_SEPARATION_STEP;
            if (beatSpacing > EDITOR_BEAT_SEPARATION_MAX)
                beatSpacing = EDITOR_BEAT_SEPARATION_MAX;
            savePrefs();
            break;
        case SDLK_MINUS:
            // Decrease spacing between two beats.
            beatSpacing -= EDITOR_BEAT_SEPARATION_STEP;
            if (beatSpacing < EDITOR_BEAT_SEPARATION_MIN)
                beatSpacing = EDITOR_BEAT_SEPARATION_MIN;
            savePrefs();
            break;
        case SDLK_COMMA:
        case SDLK_PERIOD:
            {
                float netChange = event->key.key == SDLK_COMMA ? -0.1f : 0.1f;
                if (event->key.mod & SDL_KMOD_CTRL) netChange /= 10;
                if (event->key.mod & SDL_KMOD_SHIFT) netChange /= 100;
                float* toChange = &selectedSpeed;
                if (event->key.mod & SDL_KMOD_ALT) toChange = &globalSpeedModifier;
                *toChange += netChange;
                if (*toChange <= 0)
                    *toChange = -netChange;
                if (toChange == &selectedSpeed)
                    savePrefs();
            }
            break;
        case SDLK_L:
            // Open file picker.
            if (!filePickerOpen)
            {
                openLoadBallFilePicker();
            }
            break;
        case SDLK_I:
            #ifndef EDIT_MODE
                // Open file picker.
                if (!filePickerOpen)
                {
                    openLoadCommandsPicker();
                }
            #endif
            break;
        case SDLK_M:
            #ifndef EDIT_MODE
                if (event->key.mod & SDL_KMOD_ALT)
                {
                    if (gameState == EDITING_MUSIC_OFFSET)
                        gameState = EDITING_NONE;
                    else if (gameState == EDITING_NONE)
                    {
                        gameState = EDITING_MUSIC_OFFSET;
                        resetInput();
                    }
                }
                else if (!filePickerOpen) // Open music picker.
                {
                    openLoadMusicPicker();
                }
            #endif
            break;
        case SDLK_S:
            // Save file picker.
            if (!filePickerOpen)
            {
                openSaveBallFilePicker();
            }
            break;
        case SDLK_O:
            #ifndef EDIT_MODE
                // Save file picker.
                if (!filePickerOpen)
                {
                    openSaveCommandsPicker();
                }
            #endif
            break;
        case SDLK_R:
            this->beatLinesOffset = 0.0f;
            this->globalSpeedModifier = 1.0f;
            this->beatSpacing = DEFAULT_BEAT_SPACING;
            savePrefs();
            break;
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            shouldClonePoint = true;
            break;
        case SDLK_P:
            // Cycle placing mode.
            placingMode = static_cast<PlacingMode>(static_cast<int>(placingMode) + 1);
            // Hide square mode
            if (placingMode == PlacingMode::SQUARE)
                placingMode = static_cast<PlacingMode>(static_cast<int>(placingMode) + 1);
            if (placingMode == PlacingMode::INVALID)
                placingMode = PlacingMode::BALL;
            break;
    }
}

void Game::startPlayTest(double beat)
{
    paddleKeysPressed = 0;
    paddleKeysPressedOnce = 0;

    gameState = GameState::PLAYTESTING;
    startPlaytestingBeat = beat;
    seconds = getSecondsFromBeat(startPlaytestingBeat);
    seconds -= 0.8; // Introduce delay for giving the player time to playtest.
    startPlaytestingBeat = getBeatFromSeconds(seconds);
    this->beat = startPlaytestingBeat;
    // Now expand the queued balls (e.g. holds)
    setQueuedBalls(beat);
    if (currentMusicSeconds() >= 0) {
        startPlayingMusic(currentMusicSeconds());
    }
}

void Game::startPlayingMusic(double seconds) {
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, std::round(seconds * 1000));
    MIX_SetTrackAudio(this->track, this->music);
    MIX_PlayTrack(this->track, props);
}

minibeat Game::getMinibeatOfLastPoint(BallType& type)
{
    std::vector<BallTypeTailPoint>* optional = Serializer::getPointsFromType(type);
    if (optional != NULL)
    {
        std::vector<BallTypeTailPoint>& vec = *optional;
        if (!vec.empty())
        {
            return vec.rbegin()->minibeats;
        }
    }
    return 0_mb;
}

void Game::setQueuedBalls(double startFrom)
{
    queuedBalls.clear();
    queuedBallFlashes.clear();
    auto iterStartFrom = std::lower_bound(balls.begin(), balls.end(), startFrom);
    std::copy(iterStartFrom, balls.end(), std::back_inserter(queuedBalls));
    for (auto it = queuedBalls.begin(); it != queuedBalls.end(); ++it)
    {
        std::vector<BallTypeTailPoint>* points = Serializer::getPointsFromType(it->type);
        if (points != nullptr)
        {
            // This ball is either a hold or a pit.
            minibeat longBallAt = it->at;
            float longBallX = it->x;
            BallType originalType = it->type;
            // Create fragments from 0.25 to hold end incrementing by 0.25.
            if (points->empty()) continue;
            // Get the last point.
            BallTypeTailPoint lastPoint = *points->rbegin();
            for (minibeat node = LONG_BALL_NODE_SPACING; node <= lastPoint.minibeats; node += LONG_BALL_NODE_SPACING)
            {
                float x = lastPoint.x;
                // Calculate the appropriate 'x' value.
                float x1 = longBallX, x2;
                minibeat y1 = 0_mb, y2;
                auto point = points->begin();
                while (point != points->end())
                {
                    x2 = point->x;
                    y2 = point->minibeats;
                    ASSERT(y1 <= y2);
                    if (node <= y2)
                    {
                        if (y1 == y2)
                            x = y2;
                        else
                            x = x1 + (x2 - x1) / (y2 - y1) * (node - y1);
                        break;
                    }
                    // Go to the next point.
                    x1 = x2;
                    y1 = y2;
                    ++point;
                }

                // Node ball minibeat = Hold ball minibeat + Relative minibeats.
                Ball nodeBall(longBallAt + node, it->speed, x);

                // Create type object and put that.
                BallType type;

                if (std::holds_alternative<BallTypeHold>(originalType))
                    type = BallTypeHoldFragment {};
                else if (std::holds_alternative<BallTypePit>(originalType))
                    type = BallTypePitFragment {};
                
                nodeBall.type = type;

                it = queuedBalls.insert(it + 1, nodeBall);
            }
        }
    }
}

void Game::stopPlayTest()
{
    gameState = GameState::EDITING_NONE;
    // Round beat.
    if (beat < 0) beat = 0;
    warpBeatToDivisor();
    queuedBalls.clear();
    queuedBallFlashes.clear();
    MIX_StopTrack(this->track, 0);
}

bool Game::handleMenuEvent(SDL_Event* event)
{
    char toAdd = '\0';
    GameState prevState = gameState;
    switch (event->type)
    {
        case SDL_EVENT_KEY_DOWN:
            switch (event->key.key)
            {
                case SDLK_ESCAPE:
                    // Go back to the editor.
                    if (gameState != GameState::PLAYING)
                    {
                        if (gameState == GameState::PLAYTESTING)
                            stopPlayTest();
                        gameState = GameState::EDITING_NONE;
                    }
                    break;
                case SDLK_F1:
                    // Open/close Help Menu.
                    if (gameState == GameState::EDITING_HELP)
                    {
                        gameState = GameState::EDITING_NONE; // Menu closed.
                    }
                    else if (gameState == GameState::EDITING_NONE)
                    {
                        gameState = GameState::EDITING_HELP; // Menu open.
                    }
                    break;
                case SDLK_F2:
                    // Playtest from start.
                    if (gameState == GameState::EDITING_NONE)
                    {
                        startPlayTest(0.0);
                    }
                    break;
                case SDLK_F3:
                    // Playtest from current beat.
                    if (gameState == GameState::EDITING_NONE)
                    {
                        startPlayTest(this->beat);
                    }
                    break;
                case SDLK_B:
                    // Open/close BPM changer.
                    #ifndef EDIT_MODE // Can't edit BPM for edits.
                        if (gameState == GameState::EDITING_BPM)
                        {
                            gameState = GameState::EDITING_NONE; // BPM menu closed.
                        }
                        else if (gameState == GameState::EDITING_NONE)
                        {
                            resetInput();
                            gameState = GameState::EDITING_BPM; // BPM menu open.
                        }
                        #endif
                    break;
                case SDLK_W:
                    // Open/close Paddle Width changer.
                    if (gameState == GameState::EDITING_PW)
                    {
                        gameState = GameState::EDITING_NONE; // menu closed.
                    }
                    else if (gameState == GameState::EDITING_NONE)
                    {
                        resetInput();
                        gameState = GameState::EDITING_PW; // menu open.
                    }
                    break;
                case SDLK_D:
                    // Open/close Paddle Speed changer.
                    if (gameState == GameState::EDITING_PS)
                    {
                        gameState = GameState::EDITING_NONE; // menu closed.
                    }
                    else if (gameState == GameState::EDITING_NONE)
                    {
                        resetInput();
                        gameState = GameState::EDITING_PS; // menu open.
                    }
                    break;
                case SDLK_U:
                    // Open/close Paddle Dual changer.
                    if (gameState == GameState::EDITING_DUAL)
                    {
                        gameState = GameState::EDITING_NONE; // menu closed.
                    }
                    else if (gameState == GameState::EDITING_NONE)
                    {
                        resetInput();
                        gameState = GameState::EDITING_DUAL; // menu open.
                    }
                    break;
                case SDLK_C:
                    // Open/close Command changer.
                    #ifndef EDIT_MODE // Can't add commands for edits.
                        if (gameState == GameState::EDITING_NONE)
                        {
                            resetInput();
                            for (Command& command : this->commands)
                            {
                                if (command.beat == beat)
                                {
                                    this->inputText = command.action;
                                    this->inputCursor = inputText.size();
                                    break;
                                }
                            }
                            gameState = GameState::EDITING_CMD; // menu open.
                        }
                    #endif
                    break;
                case SDLK_M:
                    #ifdef EDIT_MODE
                        if (gameState == GameState::EDITING_NONE)
                        {
                            resetInput();
                            gameState = GameState::EDITING_MUSIC; // menu open.
                            musicId = appliedMusicId;
                        }
                    #endif
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (gameState == GameState::EDITING_BPM)
                    {
                        double bpm;
                        gameState = GameState::EDITING_NONE;
                        if (Serializer::checkIsDouble(inputText, bpm))
                        {
                            // Add BPM at location.
                            addBpmChange((BpmChange) {
                                this->beat,
                                bpm
                            });
                        }
                    }
                    else if (gameState == GameState::EDITING_PW)
                    {
                        double width;
                        gameState = GameState::EDITING_NONE;
                        if (Serializer::checkIsDouble(inputText, width))
                        {
                            // Add PW at location.
                            addPaddleWidthChange((PaddleWidthChange) {
                                this->beat,
                                (float) width
                            });
                        }
                    }
                    else if (gameState == GameState::EDITING_PS)
                    {
                        double speed;
                        gameState = GameState::EDITING_NONE;
                        if (Serializer::checkIsDouble(inputText, speed))
                        {
                            // Add PS at location.
                            addPaddleSpeedChange((PaddleSpeedChange) {
                                this->beat,
                                (float) speed
                            });
                        }
                    }
                    else if (gameState == GameState::EDITING_DUAL)
                    {
                        double e;
                        gameState = GameState::EDITING_NONE;
                        if (Serializer::checkIsDouble(inputText, e))
                        {
                            // Add PS at location.
                            addPaddleDualChange((PaddleDualChange) {
                                this->beat,
                                (float) e > 0
                            });
                        }
                    }
                    else if (gameState == GameState::EDITING_CMD)
                    {
                        gameState = GameState::EDITING_NONE;
                        {
                            // Add CMD at location.
                            addCommand((Command) {
                                this->beat,
                                this->inputText
                            });
                        }
                    }
                    else if (gameState == GameState::EDITING_MUSIC_OFFSET)
                    {
                        double result;
                        gameState = GameState::EDITING_NONE;
                        if (Serializer::checkIsDouble(this->inputText, result))
                        {
                            this->musicOffset = (float) result;
                        }
                    }
                    else if (gameState == GameState::EDITING_MUSIC)
                    {
                        double bpm;
                        gameState = GameState::EDITING_NONE;
                        if (musicId != appliedMusicId)
                        {
                            appliedMusicId = musicId;
                            applyNewEditSong();
                        }
                    }
                    break;
            }
    }
    // Handle number input.
    if (event->type == SDL_EVENT_KEY_DOWN && !instantaneousInput)
    {
        bool shift = (event->key.mod & SDL_KMOD_SHIFT) ? true : false;

        if (inputModeEnabled() == InputMode::NUMERIC || inputModeEnabled() == InputMode::TEXT)
        {
            SDL_Keycode key = event->key.key;
            char n = '\0';
            if (key >= SDLK_0 && key <= SDLK_9)
                n = (char) (key - SDLK_0) + '0';
            else if (key == SDLK_KP_0)
                n = '0';
            else if (gameState == GameState::EDITING_MUSIC_OFFSET && (key == SDLK_MINUS || key == SDLK_KP_MINUS))
                n = '-';
            else if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
                n = (char) (key - SDLK_KP_1) + '1';
            else if (!shift && (key == SDLK_PERIOD || key == SDLK_KP_PERIOD))
            {
                n = '.';
            }
            else if (key == SDLK_LEFT && inputCursor > 0)
            {
                inputCursor--;
            }
            else if (key == SDLK_RIGHT && inputCursor < inputText.size())
            {
                inputCursor++;
            }
            else if ((key == SDLK_BACKSPACE || key == SDLK_KP_BACKSPACE) && inputCursor > 0)
            {
                inputText.erase(inputCursor - 1, 1u);
                inputCursor--;
            }
            else if (key == SDLK_DELETE && inputCursor < inputText.size())
            {
                inputText.erase(inputCursor, 1u);
            }

            if (n >= '0' && n <= '9' && shift)
            {
                if (inputModeEnabled() == InputMode::TEXT)
                {
                    const char shiftedNums[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                    n = shiftedNums[n - '0'];
                }
                else
                    n = '\0';
            }

            if (n)
            {
                inputText.insert(inputCursor, 1u, n);
                inputCursor++;
            }
        }
        if (inputModeEnabled() == InputMode::TEXT)
        {
            // Accept letters
            SDL_Keycode key = event->key.key;
            char n = '\0';
            if (key >= SDLK_A && key <= SDLK_Z)
            {
                bool isUppercase = false;
                if (event->key.mod & SDL_KMOD_CAPS) isUppercase = !isUppercase;
                if (shift) isUppercase = !isUppercase;
                char start = isUppercase ? 'A' : 'a';
                n = (char) (key - SDLK_A) + start;
            }
            else if (key == SDLK_SPACE || key == SDLK_KP_SPACE)
            {
                n = ' ';
            }
            else if (key == SDLK_MINUS && key == SDLK_KP_MINUS)
            {
                n = shift ? '_' : '-';
            }
            else if (key == SDLK_SEMICOLON)
            {
                n = shift ? ':' : ';'; 
            }
            else if (!shift && (key == SDLK_COMMA || key == SDLK_KP_COMMA))
            {
                n = ',';
            }
            if (n)
            {
                inputText.insert(inputCursor, 1u, n);
                inputCursor++;
            }
        }
        if (inputModeEnabled() == InputMode::SELECT)
        {
            SDL_Keycode key = event->key.key;
            if (key == SDLK_LEFT && musicId > 0)
            {
                musicId--;
            }
            if (key == SDLK_RIGHT && musicId < editSongs.size() - 1)
            {
                musicId++;
            }
        }
        if (gameState == GameState::EDITING_NONE)
        {
            SDL_Keycode key = event->key.key;
            if (key >= SDLK_0 && key <= SDLK_9)
            {
                CustomPlacingModeKey cpmk = (CustomPlacingModeKey) (key - SDLK_0);
                handleCustomPlacingModeKey(cpmk);
            }
        }
    }
    this->instantaneousInput = false;
    // Handle player input here.
    if ((gameState == GameState::PLAYING || gameState == GameState::PLAYTESTING) &&
        !event->key.repeat)
    {
        uint8_t flags = 0;
        switch (event->key.key)
        {
            case SDLK_A:
                flags |= PaddleKeys::LEFT;
                break;
            case SDLK_S:
                flags |= PaddleKeys::HIT1;
                break;
            case SDLK_D:
                flags |= PaddleKeys::HIT2;
                break;
            case SDLK_F:
                flags |= PaddleKeys::RIGHT;
                break;
        }
        if (event->type == SDL_EVENT_KEY_DOWN)
        {
            // Let's OR the flags.
            this->paddleKeysPressed |= flags;
            this->paddleKeysPressedOnce |= flags;
        }
        else if (event->type == SDL_EVENT_KEY_UP)
            // Let's AND the inverse flags.
            this->paddleKeysPressed &= ~flags;
    }
    if (prevState < 0x03 && gameState >= 0x03) {
        int width, height;
        getRendererSize(&width, &height);
        width /= getRenderedScale();
        height /= getRenderedScale();
        int y = height / 2;
        SDL_Rect rect {0, y, width, y + 100};
        SDL_SetTextInputArea(window, &rect, this->mousePosition[0]);
        SDL_StartTextInput(window);
    }
    if (prevState >= 0x03 && gameState < 0x03) SDL_StopTextInput(window);
    return this->gameState != GameState::EDITING_NONE;
}

void Game::applyNewEditSong() {
    // TODO: apply new music and BPMs
    // Get the BPM first.
    std::string song = editSongs.at(musicId).song;
    std::filesystem::path folder = basePath / "edit_resources" / song;
    std::ifstream in(folder / "song.txt", std::ios::binary);
    ASSERT(in.is_open() && !in.badbit);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string str = buffer.str();
    SerializerResult result = serializer->readBallfile(str.data(), str.length(), true);
    if (std::holds_alternative<SerializerSuccess>(result)) {
        SerializerSuccess success = std::get<SerializerSuccess>(result);
        bpmChanges = success.bpmChanges;
        musicOffset = success.musicOffset;
    }

    // Load the music.
    std::ifstream inMusic(folder / "music.mp3", std::ios::binary);
    ASSERT(inMusic.is_open() && !inMusic.badbit);
    std::stringstream bufferMusic;
    bufferMusic << inMusic.rdbuf();
    std::string strMusic = bufferMusic.str();
    MIX_Audio* music = MIX_LoadAudio_IO(this->mixer, SDL_IOFromMem(strMusic.data(), strMusic.length()), false, true);
    
    if (music == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Loading Music: %s", SDL_GetError());
        return;
    }

    // Free the existing music.
    if (this->music != NULL)
    {
        MIX_DestroyAudio(this->music);
    }
    this->music = music;
}

float Game::getPaddleSpeed()
{
    if (paddleWidthChanges.empty()) {
        return DEFAULT_PADDLE_SPEED;
    }

    auto it = std::upper_bound(
        paddleSpeedChanges.begin(), paddleSpeedChanges.end(), beat,
        [](double value, const PaddleSpeedChange& c){
            return value < c.beat;
        }
    ); // first iterator >= beat

    if (it == paddleSpeedChanges.begin()) return it->speed;
    it--; // last iterator < beat;
    return it->speed;
}

void Game::update()
{
    if (gameState != GameState::PLAYING && gameState != GameState::PLAYTESTING) return;
    float speed =
        !!(paddleKeysPressed & PaddleKeys::LEFT) * -3.5f +
        !!(paddleKeysPressed & PaddleKeys::HIT1) * -1.0f +
        !!(paddleKeysPressed & PaddleKeys::HIT2) * 1.0f +
        !!(paddleKeysPressed & PaddleKeys::RIGHT) * 3.5f;
    float change = speed * getPaddleSpeed() * this->deltaTimePassed;
    paddlePosition += change;
    if (std::abs(paddlePosition) > PADDLE_MAX_LEFT - getPaddleWidth() / 2)
    {
        paddlePosition = (paddlePosition / std::abs(paddlePosition)) * (PADDLE_MAX_LEFT - getPaddleWidth() / 2);
    }
    // Ball handling.
    if (currentMusicSeconds() < 0 && currentMusicSeconds() + deltaTimePassed >= 0)
    {
        startPlayingMusic(currentMusicSeconds() + deltaTimePassed);
    }
    seconds += deltaTimePassed;
    beat = getBeatFromSeconds(seconds);
    updateBalls();
    updateFlashes();
}

double Game::currentMusicSeconds()
{
    return seconds - musicOffset;
}

void Game::updateFlashes()
{
    for (auto flash = queuedBallFlashes.begin();
        flash < queuedBallFlashes.end();)
    {
        double timeDiff = seconds - flash->secondsWhenHit;
        if (timeDiff >= BALL_FLASH_EXPIRY)
        {
            // Remove the element.
            flash = queuedBallFlashes.erase(flash);
        }
        else
            ++flash;
    }
}

void Game::renderFlashesAndJudgment()
{
    BallFlash* latestFlash = NULL;
    BallFlash* latestOkFlash = NULL;
    BallFlash* latestOopsFlash = NULL;
    for (auto flash = queuedBallFlashes.begin();
        flash < queuedBallFlashes.end();
        ++flash)
    {
        double timeDiff = seconds - flash->secondsWhenHit;
        // From 0..1, preferably. Might not be...
        float alpha = 0.5 - timeDiff * 3.0 / 2.0;
        SDL_Color flashColor = getFlashColor(flash->judgment);
        alpha *= ((float) flashColor.a) / 255.0f;
        if (alpha > 0 && ((flash->judgment >= Judgment::PERFECT && flash->judgment <= Judgment::GOOD) || flash->judgment == Judgment::OOPS))
        {
            SDL_Texture* tex = flash->judgment == Judgment::OOPS ? textureLibrary->mineFlash : textureLibrary->flash;
            SDL_SetTextureColorMod(
                tex,
                flashColor.r,
                flashColor.g,
                flashColor.b
            );
            SDL_SetTextureAlphaModFloat(tex, alpha);
            float flashSize = BALL_FLASH_SIZE * (1.0f + timeDiff * 3.0f);
            SDL_FRect destRect = SDL_FRect {
                getGameplayXoffset() + GAMEPLAY_WIDTH / 2 + flash->x - flashSize / 2,
                GAMEPLAY_HEIGHT / 2 - flash->y - flashSize / 2 + queuedBallsYoffset(),
                flashSize,
                flashSize
            };
            SDL_RenderTexture(
                this->renderer,
                tex,
                NULL,
                &destRect
            );
        }
        // We want to show both types of judgments at the same time!
        if (flash->judgment == Judgment::OK) latestOkFlash = &*flash;
        else if (flash->judgment == Judgment::OOPS) latestOopsFlash = &*flash;
        else latestFlash = &*flash;
    }
    // Display judgment(s).
    if (latestFlash != NULL)
    {
        drawText(
            getText(latestFlash->judgment), getTextColor(latestFlash->judgment), getGameplayXoffset() + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2 + queuedBallsYoffset(), TextAlignment::CENTER_ALIGNED, (1.0f + (seconds - latestFlash->secondsWhenHit) * 1.50f)
        );
    }
    if (latestOkFlash != NULL) {
        drawText(
            getText(latestOkFlash->judgment), getTextColor(latestOkFlash->judgment), getGameplayXoffset() + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2 + queuedBallsYoffset() + 30, TextAlignment::CENTER_ALIGNED, (1.0f + (seconds - latestOkFlash->secondsWhenHit) * 0.7f) * 0.65f
        );
    }
    if (latestOopsFlash != NULL) {
        drawText(
            getText(latestOopsFlash->judgment), getTextColor(latestOopsFlash->judgment), getGameplayXoffset() + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2 + queuedBallsYoffset() + 60, TextAlignment::CENTER_ALIGNED, (1.0f + (seconds - latestOopsFlash->secondsWhenHit) * 0.7f) * 0.65f
        );
    }
    // Revert color mods.
    SDL_SetTextureColorMod(textureLibrary->flash, 0xFF, 0xFF, 0xFF);
    SDL_SetTextureAlphaModFloat(textureLibrary->flash, 255.0f);
    SDL_SetTextureColorMod(textureLibrary->mineFlash, 0xFF, 0xFF, 0xFF);
    SDL_SetTextureAlphaModFloat(textureLibrary->mineFlash, 255.0f);
}

Judgment Game::getJudgmentFromDifference(float difference)
{
    if (difference < DEFAULT_PADDLE_WIDTH * 0.2)
        return Judgment::PERFECT;
    else if (difference < DEFAULT_PADDLE_WIDTH * 0.35)
        return Judgment::GREAT;
    else
        return Judgment::GOOD;
}

Judgment Game::getJudgmentFromMilliseconds(double milliseconds)
{
    if (std::abs(milliseconds) < 75.0f)
        return Judgment::PERFECT;
    else if (std::abs(milliseconds) < 100.0f)
        return Judgment::GREAT;
    else if (std::abs(milliseconds) < 125.0f)
        return Judgment::GOOD;
    else
        return Judgment::MISS;
}

std::vector<Ball>::iterator Game::handleAfterQueuedBallHit(std::vector<Ball>::iterator it)
{
    if (std::holds_alternative<BallTypeHold>(it->type))
    {
        BallTypeHold& type = std::get<BallTypeHold>(it->type);
        // Convert to hit version.
        type.hit = true;
        return it + 1;
    }
    if (std::holds_alternative<BallTypePit>(it->type))
    {
        BallTypePit& type = std::get<BallTypePit>(it->type);
        // Convert to hit version.
        type.hit = true;
        return it + 1;
    }
    if (std::holds_alternative<BallTypeBouncy>(it->type))
    {
        BallTypeBouncy& type = std::get<BallTypeBouncy>(it->type);
        // SDL_Log("ball was at %d and had interval %d", it->at, type.interval);
        it->at += type.interval;
        if (type.respawns == 0)
        {
            // Ball ran out of respawns. Kill it.
            return queuedBalls.erase(it);
        }
        else
        {
            type.respawns--;
            type.lastHit = this->beat;
            // SDL_Log("ball now has %d respawns with at %d, interval %d last hit at %f x %f", type.respawns, it->at, type.interval, type.lastHit, it->x);
        }
        return it;
    }
    return queuedBalls.erase(it);
}

void Game::updateBalls()
{
    for (auto ball = queuedBalls.begin();
        ball < queuedBalls.end();)
    {
        // Check if the ball intersects the rectangle.
        // Taken from https://stackoverflow.com/questions/401847/circle-rectangle-collision-detection-intersection

        if (!isQueuedBallInteractable(*ball))
        {
            ++ball;
            continue; // Non-interactable.
        }

        // For bouncy balls.
        if (this->beat - Ball::toBeats(ball->at) < -0.00001)
        {
            ++ball;
            continue;
        }

        float cy = getSignedFallingBallPos(*ball);

        float size = BALL_SIZE;
        if (std::holds_alternative<BallTypeHoldFragment>(ball->type) ||
            std::holds_alternative<BallTypePitFragment>(ball->type))
            size = TAIL_SIZE;

        bool missed = cy <= SCREEN_HEIGHT / -2 - size / 2;
        if (!missed && std::holds_alternative<BallTypeBouncy>(ball->type))
        {
            BallTypeBouncy bouncyData = std::get<BallTypeBouncy>(ball->type);
            // Cannot exceed beat + interval
            minibeat threshold = ball->at + bouncyData.interval;
            missed = Ball::toMinibeats(this->beat) >= threshold;
        }
        if (missed)
        {
            // Remove the ball and count it as a miss.
            Judgment judgment = Judgment::MISS;
            if (std::holds_alternative<BallTypePitFragment>(ball->type) ||
                std::holds_alternative<BallTypeMine>(ball->type) ||
                std::holds_alternative<BallTypePit>(ball->type))
                judgment = Judgment::OK;
            if (std::holds_alternative<BallTypeHoldFragment>(ball->type))
                judgment = Judgment::OOPS;
            queuedBallFlashes.push_back(BallFlash {
                seconds,
                judgment,
                0.0f,
                0.0f
            });
            ball = handleAfterQueuedBallHit(ball);
            continue;
        }

        if (std::holds_alternative<BallTypeSquare>(ball->type))
        {
            ++ball;
            continue; // Squares are handled here only for MISSES.
        }

        float cx = ball->x;
        // The collision in the editor is not the same as that of the Scratch project, 
        // but it can be considered to be close enough.
        float cr = size / 2;
        if (std::holds_alternative<BallTypeMine>(ball->type)) cr -= 4;
        if (std::holds_alternative<BallTypePit>(ball->type) || std::holds_alternative<BallTypePitFragment>(ball->type)) cr -= 18;
        if (std::holds_alternative<BallTypeHoldFragment>(ball->type)) cr += 15;
        bool isTouching = false;
        size_t lastI = getPaddleDualMode(this->beat) ? 1 : 0;

        for (size_t i = 0; i <= lastI && !isTouching; i++) {
            SDL_FRect rect = SDL_FRect {
                paddlePosition * (i == 1 ? -1 : 1) - getPaddleWidth() / 2,
                SCREEN_HEIGHT / 2 - PADDLE_TOP - PADDLE_HEIGHT,
                getPaddleWidth(),
                PADDLE_HEIGHT  
            };

            float rx = rect.x + rect.w / 2;
            float ry = rect.y + rect.h / 2;

            float dx = std::abs(rx - cx);
            float dy = std::abs(ry - cy);

            if (dx > rect.w / 2 + cr || dy > rect.h / 2 + cr)
            {
                // Definitely not touching.
                continue;
            }

            isTouching = dx <= (rect.w / 2) || dy <= (rect.h / 2);

            if (!isTouching)
            {
                float cornerDistanceSq =
                    std::pow(dx - rect.w / 2, 2.0f) + std::pow(dy - rect.h / 2, 2.0f);
                isTouching = cornerDistanceSq <= cr * cr;
            }
        }

        if (isTouching)
        {
            // If the ball is a hold fragment, the judgment should be an OK.
            Judgment judgment;
            if (std::holds_alternative<BallTypeHoldFragment>(ball->type))
                judgment = Judgment::OK;
            else if (std::holds_alternative<BallTypePitFragment>(ball->type) ||
                std::holds_alternative<BallTypeMine>(ball->type) ||
                std::holds_alternative<BallTypePit>(ball->type))
                judgment = Judgment::OOPS;
            else
                judgment = getJudgmentFromDifference(std::min(std::abs(paddlePosition - ball->x), std::abs(paddlePosition + ball->x)));
            
            queuedBallFlashes.push_back(BallFlash {
                seconds,
                judgment,
                cx,
                cy
            });
            ball = handleAfterQueuedBallHit(ball);
        }
        else
            ++ball;
    }
    uint8_t hits = 0;
    if (paddleKeysPressedOnce & PaddleKeys::HIT1)
        hits++;
    if (paddleKeysPressedOnce & PaddleKeys::HIT2)
        hits++;
    while (true)
    {
        double earliestBall = INFINITY;
        std::vector<Ball>::iterator earliest;
        std::vector<size_t> toMarkHit;
        double bestMilliseconds = 0.0f;
        for (auto it = queuedBalls.begin(); it != queuedBalls.end();)
        {
            if (!std::holds_alternative<BallTypeSquare>(it->type))
            {
                ++it;
                continue;
            }
            double difference = Ball::toBeats(it->at) - this->beat;
            double seconds = getSecondsFromBeat(Ball::toBeats(it->at)) - getSecondsFromBeat(this->beat);
            double milliseconds = seconds * 1000;
            Judgment judgment = getJudgmentFromMilliseconds(milliseconds);
            /*
            if (seconds <= 0 && judgment == Judgment::MISS)
            {
                // Missed the square.
                queuedBallFlashes.push_back(BallFlash {
                    this->seconds,
                    judgment,
                    0.0f,
                    0.0f
                });
                toMarkHit.push_back(std::distance(queuedBalls.begin(), it++));
                continue;
            }
            */
            if (seconds < earliestBall || earliestBall == INFINITY)
            {
                float y = getSignedFallingBallPos(*it);
                SDL_FRect square = SDL_FRect {
                    it->x,
                    y - BALL_SIZE / 2,
                    BALL_SIZE,
                    BALL_SIZE
                };
                SDL_FRect paddlePlane = SDL_FRect {
                    paddlePosition - getPaddleWidth() / 2,
                    -SCREEN_HEIGHT/2,
                    getPaddleWidth(),
                    SCREEN_HEIGHT
                };
                if (SDL_HasRectIntersectionFloat(&square, &paddlePlane) &&
                    judgment != Judgment::MISS)
                {
                    bestMilliseconds = milliseconds;
                    earliestBall = seconds;
                    earliest = it;
                }
            }
            ++it;
        }
        for (auto it = toMarkHit.rbegin(); it != toMarkHit.rend(); ++it)
        {
            std::vector<Ball>::iterator it2 = queuedBalls.begin() + *it;
            handleAfterQueuedBallHit(it2);
        }
        if (hits == 0) break; // Stop if no more hits are available.
        hits--;
        if (earliestBall != INFINITY)
        {
            // Provide a judgment.
            Judgment judgment = getJudgmentFromMilliseconds(bestMilliseconds);
            queuedBallFlashes.push_back(BallFlash {
                seconds,
                judgment,
                earliest->x,
                getSignedFallingBallPos(*earliest)
            });
            earliest = handleAfterQueuedBallHit(earliest);
        }
    }
}

std::string Game::getDisplayedInputText()
{
    std::string text = inputText;
    text.insert(inputCursor, 1u, '|');
    return text;
}

void Game::resetInput()
{
    this->inputText = "";
    this->inputCursor = 0;
    this->instantaneousInput = true;
}

InputMode Game::inputModeEnabled()
{
    if (gameState == GameState::EDITING_BPM || gameState == GameState::EDITING_PW || gameState == GameState::EDITING_PS || gameState == GameState::EDITING_DUAL || gameState == GameState::EDITING_MUSIC_OFFSET) return InputMode::NUMERIC;
    if (gameState == GameState::EDITING_CMD) return InputMode::TEXT;
    if (gameState == GameState::EDITING_MUSIC) return InputMode::SELECT;
    return InputMode::NONE;
}

void Game::handleEvent(SDL_Event* event)
{
    if (handleMenuEvent(event)) return;
    switch (event->type)
    {
        case SDL_EVENT_MOUSE_WHEEL:
            handleMouseWheelEvent(event);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            handleMouseButtonUpEvent(event);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            handleMouseButtonDownEvent(event);
            break;
        case SDL_EVENT_KEY_DOWN:
            handleKeyDownEvent(event);
            break;
        case SDL_EVENT_KEY_UP:
            switch (event->key.key)
            {
                case SDLK_LCTRL:
                case SDLK_RCTRL:
                    shouldClonePoint = false;
                    break;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            float scaling = getScale();
            this->mousePosition[0] = event->motion.x / scaling;
            this->mousePosition[1] = event->motion.y / scaling;
            moveCurrentPoint(shouldClonePoint);
            shouldClonePoint = false;
    }
}

void Game::moveCurrentPoint(bool clone)
{
    if (!selectedPoint) return;
    if (clone)
    {
        BallType& ballType = selectedPointOwner.value()->type;
        std::vector<BallTypeTailPoint>* points = Serializer::getPointsFromType(ballType);
        if (std::holds_alternative<BallTypeHold>(ballType))
        {
            BallTypeHold& ballTypeData = std::get<BallTypeHold>(ballType);
            std::vector<BallTypeTailPoint>::iterator& iterator = selectedPoint.value();
            BallTypeTailPoint copy = *iterator;
            if (iterator != ballTypeData.points.end())
            {
                ++iterator;
            }
            selectedPoint = ballTypeData.points.insert(iterator, copy);
        }
        if (std::holds_alternative<BallTypePit>(ballType))
        {
            BallTypePit& ballTypeData = std::get<BallTypePit>(ballType);
            std::vector<BallTypeTailPoint>::iterator& iterator = selectedPoint.value();
            BallTypeTailPoint copy = *iterator;
            if (iterator != ballTypeData.points.end())
            {
                ++iterator;
            }
            selectedPoint = ballTypeData.points.insert(iterator, copy);
        }
    }
    std::vector<BallTypeTailPoint>::iterator point = selectedPoint.value();
    point->x = this->mousePosition[0] - getGameplayXoffset() - GAMEPLAY_WIDTH / 2;
    double ballBeats = this->beat + (this->mousePosition[1] - getEditorSelectedBeatY()) / beatSpacing;
    ballBeats -= Ball::toBeats(this->selectedPointOwner.value()->at);
    point->minibeats = Ball::toMinibeats(ballBeats);
}

size_t Game::addBall(const Ball& ball)
{
    // We want to find the position where the ball can be added such
    // that the balls are in ascending order depending on their beat.
    clearTouchedPointOwner();
    auto position = std::lower_bound(balls.begin(), balls.end(), ball);
    return std::distance(this->balls.begin(), this->balls.insert(
        position,
        ball
    ));
}

void Game::addBpmChange(const BpmChange& bpmChange)
{
    // We want to find the position where the change can be added such
    // that the changes are in ascending order depending on their beat.
    bool isPut = false;
    for (auto bc = bpmChanges.begin(); bc < bpmChanges.end(); ++bc)
    {
        if (bc->beat == bpmChange.beat)
        {
            *bc = bpmChange;
            isPut = true;
            break;
        }
    }
    if (!isPut)
    {
        this->bpmChanges.insert(
            std::lower_bound(bpmChanges.begin(), bpmChanges.end(), bpmChange),
            bpmChange
        );
    }
    // Remove unnecessary changes.
    double lastBpm = NAN;
    for (auto bpmChange = bpmChanges.begin(); bpmChange < bpmChanges.end();)
    {
        if (bpmChange->bpm == lastBpm)
            bpmChange = bpmChanges.erase(bpmChange);
        else
        {
            lastBpm = bpmChange->bpm;
            ++bpmChange;
        }
    }
    if (bpmChanges.empty())
        bpmChanges.push_back((BpmChange) {
            0.0,
            DEFAULT_BPM
        });
}

void Game::addPaddleWidthChange(const PaddleWidthChange& paddleWidthChange)
{
    // We want to find the position where the change can be added such
    // that the changes are in ascending order depending on their beat.
    bool isPut = false;
    for (auto bc = paddleWidthChanges.begin(); bc < paddleWidthChanges.end(); ++bc)
    {
        if (bc->beat == paddleWidthChange.beat)
        {
            *bc = paddleWidthChange;
            isPut = true;
            break;
        }
    }
    if (!isPut)
    {
        this->paddleWidthChanges.insert(
            std::lower_bound(paddleWidthChanges.begin(), paddleWidthChanges.end(), paddleWidthChange),
            paddleWidthChange
        );
    }
    // Remove unnecessary changes.
    double lastWidth = NAN;
    for (auto paddleWidthChange = paddleWidthChanges.begin(); paddleWidthChange < paddleWidthChanges.end();)
    {
        if (paddleWidthChange->width == lastWidth)
            paddleWidthChange = paddleWidthChanges.erase(paddleWidthChange);
        else
        {
            lastWidth = paddleWidthChange->width;
            ++paddleWidthChange;
        }
    }
    if (paddleWidthChanges.empty())
        paddleWidthChanges.push_back((PaddleWidthChange) {
            0.0,
            DEFAULT_BPM
        });
}

void Game::addPaddleSpeedChange(const PaddleSpeedChange& paddleSpeedChange)
{
    // We want to find the position where the change can be added such
    // that the changes are in ascending order depending on their beat.
    bool isPut = false;
    for (auto bc = paddleSpeedChanges.begin(); bc < paddleSpeedChanges.end(); ++bc)
    {
        if (bc->beat == paddleSpeedChange.beat)
        {
            *bc = paddleSpeedChange;
            isPut = true;
            break;
        }
    }
    if (!isPut)
    {
        this->paddleSpeedChanges.insert(
            std::lower_bound(paddleSpeedChanges.begin(), paddleSpeedChanges.end(), paddleSpeedChange),
            paddleSpeedChange
        );
    }
    // Remove unnecessary changes.
    double lastSpeed = NAN;
    for (auto change = paddleSpeedChanges.begin(); change < paddleSpeedChanges.end();)
    {
        if (change->speed == lastSpeed)
            change = paddleSpeedChanges.erase(change);
        else
        {
            lastSpeed = change->speed;
            ++change;
        }
    }
    if (paddleSpeedChanges.empty())
        paddleSpeedChanges.push_back((PaddleSpeedChange) {
            0.0,
            DEFAULT_BPM
        });
}

void Game::addPaddleDualChange(const PaddleDualChange& paddleDualChange)
{
    // We want to find the position where the change can be added such
    // that the changes are in ascending order depending on their beat.
    bool isPut = false;
    for (auto bc = paddleDualChanges.begin(); bc < paddleDualChanges.end(); ++bc)
    {
        if (bc->beat == paddleDualChange.beat)
        {
            *bc = paddleDualChange;
            isPut = true;
            break;
        }
    }
    if (!isPut)
    {
        this->paddleDualChanges.insert(
            std::lower_bound(paddleDualChanges.begin(), paddleDualChanges.end(), paddleDualChange),
            paddleDualChange
        );
    }
    // Remove unnecessary changes.
    double last = NAN;
    for (auto change = paddleDualChanges.begin(); change < paddleDualChanges.end();)
    {
        if (change->enabled == last)
            change = paddleDualChanges.erase(change);
        else
        {
            last = change->enabled;
            ++change;
        }
    }
    if (paddleDualChanges.empty())
        paddleDualChanges.push_back((PaddleDualChange) {
            0.0,
            false
        });
}

void Game::addCommand(const Command& command)
{
    // We want to find the position where the change can be added such
    // that the changes are in ascending order depending on their beat.
    bool isPut = false;
    for (auto bc = commands.begin(); bc < commands.end(); ++bc)
    {
        if (bc->beat == command.beat)
        {
            *bc = command;
            isPut = true;
            break;
        }
    }
    if (!isPut)
    {
        this->commands.insert(
            std::lower_bound(commands.begin(), commands.end(), command),
            command
        );
    }
    // Remove unnecessary changes.
    std::string lastAction = "";
    for (auto cmd = commands.begin(); cmd < commands.end();)
    {
        if (cmd->action == lastAction)
            cmd = commands.erase(cmd);
        else
        {
            lastAction = cmd->action;
            ++cmd;
        }
    }
    for (auto cmd = commands.begin(); cmd < commands.end();)
    {
        if (cmd->action.empty())
            cmd = commands.erase(cmd);
        else
            ++cmd;
    }
}

void Game::warpBeatToDivisor()
{
    // Round to nearest minibeats.
    float mbFloat = Ball::toMinibeats(this->beat);
    minibeat minibeats = 0;
    // Round to divisor.
    float mbSegments = mbFloat / this->selectedDivisor.getWorth();
    mbSegments = std::round(mbSegments);
    mbFloat = mbSegments * this->selectedDivisor.getWorth();
    if (mbFloat > 0)
    {
        minibeats = (minibeat) mbFloat;
    }
    // Now convert back.
    double newBeat = Ball::toBeats(minibeats);
    this->beat = newBeat;
}

double Game::getBeatFromSeconds(double seconds)
{
    double currentBpm = DEFAULT_BPM;

    // If there are BPM changes, initialize the first BPM
    if (!bpmChanges.empty())
    {
        const BpmChange& firstChange = bpmChanges.front();
        if (firstChange.beat == 0.0)  // Only update if beat is at the start
            currentBpm = firstChange.bpm;
    }

    double currentSeconds = 0.0;
    double currentBeat = 0.0;
    double lastBeat = 0.0;

    // Iterate over BPM changes
    for (BpmChange& bpmChange : this->bpmChanges)
    {
        double timeBeforeNext = (bpmChange.beat - lastBeat) * 60.0 / currentBpm;
        
        // Check if we've reached or exceeded the desired time
        if (seconds < currentSeconds + timeBeforeNext + 1e-9)
        {
            double remainingSeconds = seconds - currentSeconds;
            currentBeat += remainingSeconds * currentBpm / 60.0;
            return currentBeat;
        }

        // Otherwise, continue to the next BPM change
        currentBeat += bpmChange.beat - lastBeat;
        currentSeconds += timeBeforeNext;
        
        // Update the current BPM
        currentBpm = bpmChange.bpm;
        lastBeat = bpmChange.beat;
    }

    // After the last BPM change, calculate the remaining time
    if (seconds > currentSeconds)
    {
        double remainingSeconds = seconds - currentSeconds;
        currentBeat += remainingSeconds * currentBpm / 60.0;
    }

    return currentBeat;
}

void Game::getWindowSize(int *width, int *height)
{
    SDL_GetWindowSize(this->window, width, height);
}

void Game::getRendererSize(int *width, int *height)
{
    SDL_GetWindowSizeInPixels(this->window, width, height);
}

void Game::getAspectRatioWindowSize(float* width, float* height)
{
    float scale = getScale();

    if (width != nullptr)
        *width = scale * SCREEN_WIDTH;

    if (height != nullptr)
        *height = scale * SCREEN_HEIGHT;
}

void Game::getUnusedPixels(float* left, float* top)
{
    float aspectRatioWidth, aspectRatioHeight;
    int windowWidth, windowHeight;

    getAspectRatioWindowSize(&aspectRatioWidth, &aspectRatioHeight);
    getWindowSize(&windowWidth, &windowHeight);

    if (left != nullptr)
        *left = windowWidth - aspectRatioWidth;

    if (top != nullptr)
        *top = windowHeight - aspectRatioHeight;
}

float Game::getScale()
{
    int width, height;
    getWindowSize(&width, &height);
    return std::min((float) width / SCREEN_WIDTH, (float) height / SCREEN_HEIGHT);
}

float Game::getRenderedScale()
{
    int width, height;
    getRendererSize(&width, &height);
    return std::min((float) width / SCREEN_WIDTH, (float) height / SCREEN_HEIGHT);
}

double Game::getSecondsFromBeat(double beat)
{
    double currentBpm = DEFAULT_BPM;
    
    // Handle the case where bpmChanges is empty.
    if (!bpmChanges.empty())
    {
        const BpmChange& firstChange = bpmChanges.at(0);
        if (firstChange.beat == 0.0)
            currentBpm = firstChange.bpm;
    }

    double currentSeconds = 0.0;
    double beatsRemaining = beat;
    double lastBeat = 0.0;

    // Iterate through all BPM changes.
    for (auto bpmChange = bpmChanges.begin(); bpmChange != bpmChanges.end(); ++bpmChange)
    {
        // Find the beats between two changes.
        double beatsBetweenChanges = bpmChange->beat - lastBeat;

        // If the requested beat is within this segment.
        if (beatsRemaining < beatsBetweenChanges - 1e-9) {
            break;
        }

        // Otherwise, accumulate the time for the entire segment and reduce the remaining beats.
        currentSeconds += (beatsBetweenChanges / currentBpm) * 60.0;
        beatsRemaining -= beatsBetweenChanges;

        // Update the current BPM for the next segment.
        currentBpm = bpmChange->bpm;
        lastBeat = bpmChange->beat;
    }

    // After the loop, if there are remaining beats, process them with the last BPM.
    if (beatsRemaining > 0) {
        currentSeconds += (beatsRemaining / currentBpm) * 60.0;
    }

    return currentSeconds;
}

float Game::getSignedFallingBallPos(const Ball& ball)
{
    double miniBeats = ball.at;
    float speed = ball.speed;

    float y = getSignedYPosFromBeat(miniBeats / MINIBEATS_PER_BEAT, speed);

    if (std::holds_alternative<BallTypeBouncy>(ball.type))
    {
        BallTypeBouncy type = std::get<BallTypeBouncy>(ball.type);
        // Parabola if applicable.
        if (type.lastHit != -INFINITY)
        {
            double beats = miniBeats / MINIBEATS_PER_BEAT;
            double x3 = type.lastHit;            // where the ball last was
            double x1 = Ball::toBeats(ball.at);  // where the ball is now going
            float minimum_y = PADDLE_TOP_SIGNED + BALL_SIZE / 2;
            float y2 = getSignedYPosFromBeatAgainstAnother(beats, speed, Ball::toBeats(ball.at - type.interval));

            float t = (this->beat - x3) / (x1 - x3);
            float y_quadratic_float = 1 - 4 * std::pow(t - 0.5f, 2);

            y = minimum_y + (y2 - minimum_y) * y_quadratic_float;
        }
    }

    return y;
}

float Game::getSignedYPosFromBeat(double otherBeat, float speed)
{
    return getSignedYPosFromBeatAgainstAnother(otherBeat, speed, this->beat);
}

float Game::getSignedYPosFromBeatAgainstAnother(double otherBeat, float speed, double against)
{
    double differenceInBeats = otherBeat - against;
    double amplifiedDifference = differenceInBeats * speed;

    float y1 = amplifiedDifference * BALL_SPEED * globalSpeedModifier;
    y1 += PADDLE_TOP_SIGNED + BALL_SIZE / 2;

    return y1;
}

double Game::getBeatFromSignedYPos(float yPos, float speed)
{
    // Reverse getSignedYPosFromBeat, look above for reference.
    yPos -= PADDLE_TOP_SIGNED + BALL_SIZE / 2;

    double amplifiedDifference = yPos / BALL_SPEED;
    double differenceInBeats = amplifiedDifference / speed / globalSpeedModifier;

    return differenceInBeats + beat;
}

bool Game::beforePrefs(bool initFS)
{
    #ifdef __EMSCRIPTEN__
        // Use Emscripten's IndexedDB (with IDBFS).
        int success = EM_ASM_INT({
            var initFS = $0;
            if (initFS) {
                // Make a directory other than '/' (in this case, we use /offline).
                FS.mkdir('/offline');
                // Mount with IDBFS type.
                FS.mount(IDBFS, {autoPersist: true}, '/offline');
            }

            // Sync.
            FS.syncfs(true, function (err) {
                if (err !== null) console.error('Could not sync preferences (before): ' + err);

                if (initFS) {
                    var gamePtr = $1;
                    // call the C wrapper using ccall
                    Module.ccall(
                        'Game_loadPrefs',
                        null,
                        ['number'],
                        [gamePtr]
                    );
                    // We should show the game canvas as well.
                    canvasElement.className = "emscripten";
                    // We should show the Fullscreen button.
                    fullscreenElement.hidden = false;
                    emfsElement.className = "emscripten emscripten_fullscreen";
                    // Hide the 'status'.
                    statusElement.hidden = true;
                    initElement.remove();
                }
            });
            return 1;
        }, initFS, this);
        if (success == 0) return false;
        // We can now store our files! Just set prefPath to use our mounted directory.
        this->prefPath = "/offline/";
    #else
        char* path = SDL_GetPrefPath(ORG, NAME);
        this->prefPath = std::string(path);
        if (!path || prefPath.empty()) {
            SDL_free(path);
            return false;
        }
        SDL_free(path);
    #endif
    return true;
}

bool Game::savePrefs()
{
    // Now we can create a text file for preferences.
    std::string filePath = prefPath + "prefs.bin";

    std::ofstream out(filePath, std::ios::binary);
    if (!out) {
        std::cerr << "Could not save prefs.bin.";
        return false;
    }

    std::vector<float> floats;
    floats.push_back(beatLinesOffset);
    floats.push_back(globalSpeedModifier);
    floats.push_back(beatSpacing);

    size_t count = floats.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    out.write(reinterpret_cast<const char*>(floats.data()), count * sizeof(float));
    out.close();

    return true;
}

bool Game::loadPrefs()
{
    std::string filePath = prefPath + "prefs.bin";

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error opening preferences: "
             << std::endl;

        // Check for specific error conditions
        if (in.bad()) {
            std::cerr << "Fatal error: badbit is set." << std::endl;
        }

        if (in.fail()) {
            // Print a more detailed error message using
            // strerror
            std::cerr << "Error details: " << strerror(errno)
                 << std::endl;
        }

        return false;
    }

    std::vector<float> floats;
    size_t count = 0;

    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    floats.resize(count);

    in.read(reinterpret_cast<char*>(floats.data()), count * sizeof(float));
    in.close();

    if (count >= 3)
    {
        this->beatLinesOffset = floats.at(0);
        this->globalSpeedModifier = floats.at(1);
        this->beatSpacing = floats.at(2);
    }

    return true;
}