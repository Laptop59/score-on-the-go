#include "game.h"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <string>
#include <iostream>
#include <optional>
#include <algorithm>

Game::Game(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font)
{
    this->renderer = renderer;
    this->font = font;
    this->window = window;
    this->serializer.reset(new Serializer);
    
    // Allocate memory for filter
    this->fileFilter = new SDL_DialogFileFilter;
    this->fileFilter->name = "Ballfile";
    this->fileFilter->pattern = "txt";
}

Game::~Game()
{
    // Free file filter.
   delete this->fileFilter;
}

void Game::render()
{
    // Draw a BG.
    SDL_SetRenderDrawColor(this->renderer, 0x1F, 0x1F, 0x1F, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
    {
        // Draw another result where gameplay will take place.
        SDL_FRect gameplayRect =
            SDL_FRect
            {
                (SCREEN_WIDTH - GAMEPLAY_WIDTH) / 2.0f,
                (SCREEN_HEIGHT - GAMEPLAY_HEIGHT) / 2.0f,
                GAMEPLAY_WIDTH,
                GAMEPLAY_HEIGHT
            };
        SDL_SetRenderDrawColor(this->renderer, 0x33, 0x33, 0x33, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(this->renderer, &gameplayRect);
        this->renderEditor();
    }
    SDL_RenderPresent(this->renderer);
}

void Game::renderEditor()
{
    float y;
    // For rendering...
    size_t earlier_beat = (size_t) std::floor(this->beat);
    size_t later_beat = (size_t) std::ceil(this->beat);
    if (earlier_beat == later_beat) later_beat++;

    // White lines, and translucent ones.
    y = EDITOR_SELECTED_BEAT_Y + this->beatSpacing * (earlier_beat - this->beat);
    do
    {
        this->renderEditorBeatLine(earlier_beat, y);
        y -= this->beatSpacing;
        if (earlier_beat > 0)
            earlier_beat--;
        else
            break;
    }
    while (y >= 0.0f);

    y = EDITOR_SELECTED_BEAT_Y + this->beatSpacing * (later_beat - this->beat);
    do
    {
        this->renderEditorBeatLine(later_beat, y);
        y += this->beatSpacing;
        later_beat++;
    }
    while (y <= SCREEN_HEIGHT);

    this->drawEditorBalls();
    this->drawDivisorArrow();

    this->drawSpecificEditorMenu();
}

void Game::drawSpecificEditorMenu()
{
    if (gameState == GameState::PLAYING || gameState == GameState::EDITING_NONE) return;
    SDL_SetRenderDrawColor(this->renderer, 0x00u, 0x00u, 0x00u, 0x7Fu);
    SDL_FRect rect = { 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT };
    SDL_RenderFillRect(this->renderer, &rect);
    // Middle x-position & y-position
    float x = SCREEN_WIDTH / 2;
    float y = SCREEN_HEIGHT / 2;
    switch (gameState)
    {
        case GameState::EDITING_BPM:
            drawText(
                "Change BPM", SDL_Color {0xFFu, 0x3Fu, 0x3Fu, 0xFFu},
                x, y - 50, TextAlignment::CENTER_ALIGNED, 1.0f
            );
            drawText(
                "at beat " + std::to_string(beat) + " to:",
                SDL_Color {0xBFu, 0xBFu, 0xBFu, 0xFFu},
                x, y, TextAlignment::CENTER_ALIGNED, 0.75f
            );
            drawText(
                getDisplayedInputText(),
                SDL_Color { 0xFFu, 0xCFu, 0xCFu, 0xCFu },
                x, y + 50, TextAlignment::CENTER_ALIGNED, 1.5f
            );
    }
}

void Game::drawEditorBalls()
{
    // We want to try to optimise drawing balls in the editor; there may be thousands of them
    // that won't even be shown to the user.

    // Minimum beat
    double min = beat;
    min -= (double) EDITOR_SELECTED_BEAT_Y / this->beatSpacing; // For above selected beat.
    min -= (double) BALL_SIZE / this->beatSpacing; // For ball texture to be drawn offscreen.
    min -= 1.0f; // Just to be safe.
    if (min < 0) min = 0;

    // Maximum beat
    double max = beat;
    max += (double) (GAMEPLAY_HEIGHT - EDITOR_SELECTED_BEAT_Y) / this->beatSpacing; // For below gameplay screen, below selected beat.
    max += (double) BALL_SIZE / this->beatSpacing; // For ball texture to be drawn offscreen.
    max += 1.0f; // Just to be safe.
    if (max < 0) max = 0;

    // Now convert them to their minibeat counterparts.
    minibeat minibeatsMin = Ball::toMinibeats(min);      // Mininum minibeats for ball to be drawn.
    minibeat minibeatsMax = Ball::toMinibeats(max);      // Maximum minibeats for ball to be drawn.
    minibeat minibeatsCurrent = Ball::toMinibeats(beat); // Current minibeats.

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

    // Draw BPM changes too.
    for (auto bpmChange = this->bpmChanges.begin(); bpmChange < this->bpmChanges.end(); ++bpmChange)
    {
        if (bpmChange->beat >= min && bpmChange->beat <= max)
        {
            double fromSelectedBeat = bpmChange->beat - this->beat;
            this->drawText(
                std::to_string(bpmChange->bpm),
                SDL_Color { 0xFFu, 0x7Fu, 0x7Fu, 0xFFu },
                GAMEPLAY_OFFSET + GAMEPLAY_WIDTH + 15,
                EDITOR_SELECTED_BEAT_Y + fromSelectedBeat * this->beatSpacing,
                TextAlignment::LEFT_ALIGNED,
                0.5f
            );
        }
    }

    if (ghostBallStage < 2)
        this->drawGhostBall(); // Draw it if it hasn't been drawn already.
}

void Game::openLoadFilePicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    SDL_ShowOpenFileDialog(
        &callbackLoadFilePicker,
        this,
        this->window,
        this->fileFilter,
        1,
        NULL,
        false
    );
}

void Game::openSaveFilePicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    SDL_ShowSaveFileDialog(
        &callbackSaveFilePicker,
        this,
        this->window,
        this->fileFilter,
        1,
        NULL
    );
}

void SDLCALL Game::callbackSaveFilePicker(void* userdata, const char* const* filelist, int filter)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (!filelist)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Save File Picker: %s", SDL_GetError());
    }
    else if (*filelist)
    {
        // Get the first item (we don't care about the other ones)
        const char* file = *filelist;

        // Create necessary data for saving to the file.
        std::string text = game->serializer->saveBallfile(
            game->balls,
            game->bpmChanges
        );

        // Attempt to save file.
        if (SDL_SaveFile(file, text.c_str(), text.length()))
        {
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_INFORMATION,
                "Score on the Go",
                "Successfully saved ballfile.",
                game->window
            );
        }
        else
        {
            std::string errorMessage = std::string("Could not save ballfile: \n") + SDL_GetError();
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Score on the Go",
                errorMessage.c_str(),
                game->window
            );
        }
    }
}

void SDLCALL Game::callbackLoadFilePicker(void* userdata, const char* const* filelist, int filter)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (!filelist)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Load File Picker: %s", SDL_GetError());
    }
    else if (*filelist)
    {
        // Get the first item (we don't care about the other ones)
        const char* file = *filelist;

        // Read from the file.
        size_t sizeInBytes;
        void* contents = SDL_LoadFile(file, &sizeInBytes);

        if (contents)
        {
            char* text = (char *) contents;
            SerializerResult result = game->serializer->readBallfile(text, sizeInBytes);
            
            if (std::holds_alternative<SerializerSuccess>(result))
            {
                SerializerSuccess success = std::get<SerializerSuccess>(result);
                game->balls.clear();
                for (auto ball = success.balls.begin(); ball < success.balls.end(); ++ball)
                    game->addBall(*ball);
                game->bpmChanges.clear();
                for (auto bpmChange = success.bpmChanges.begin();
                    bpmChange < success.bpmChanges.end();
                    ++bpmChange)
                    game->addBpmChange(*bpmChange);
                std::string successMessage = std::string("Successfully loaded a ball file with ");
                successMessage += std::to_string(success.balls.size());
                successMessage += " balls.";
                SDL_ShowSimpleMessageBox(
                    SDL_MESSAGEBOX_INFORMATION,
                    "Score on the Go",
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
                    failMessage += "\nAt line " + std::to_string(error->line) + ": " + error->error; 
                    if (!--errorsLeft) break;
                }

                SDL_ShowSimpleMessageBox(
                    SDL_MESSAGEBOX_ERROR,
                    "Score on the Go",
                    failMessage.c_str(),
                    game->window
                );
            }
            
            SDL_free(contents);
        }
        else
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL Error Loading File: %s", SDL_GetError());
        }
    }
}

std::optional<GameplayLeftPosition> Game::ballCanBePlaced()
{
    GameplayLeftPosition pos;
    pos.x = mousePosition[0] - GAMEPLAY_OFFSET;
    pos.y = mousePosition[1];

    if (std::abs(EDITOR_SELECTED_BEAT_Y - pos.y) <= EDITOR_RANGE_SELECTED_BEAT
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
        SDL_Texture* texture = this->textureLibrary->balls;
        minibeat miniBeats = Ball::toMinibeats(this->beat);
        ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeats);
        SDL_FRect srcRect = this->textureLibrary->createRect(
            (2 * colorDivisor.getColor() + isFast(selectedSpeed)) * BALL_SIZE,
            0,
            BALL_SIZE,
            BALL_SIZE
        );
        SDL_FRect destRect = this->textureLibrary->createRect(
            mousePos->x + GAMEPLAY_OFFSET - (float) (BALL_SIZE) / 2,
            EDITOR_SELECTED_BEAT_Y - (float) (BALL_SIZE) / 2,
            BALL_SIZE,
            BALL_SIZE
        );
        SDL_SetTextureAlphaMod(texture, GHOST_BALL_ALPHA);
        SDL_RenderTexture(this->renderer, texture, &srcRect, &destRect);
        SDL_SetTextureAlphaMod(texture, 0xFFu);
    }
}

void Game::drawEditorBall(const Ball& ball)
{
    SDL_Texture* texture = this->textureLibrary->balls;
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
        ball.x + GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2 - (float) (BALL_SIZE) / 2,
        EDITOR_SELECTED_BEAT_Y + fromSelectedBeat * this->beatSpacing - (float) (BALL_SIZE) / 2,
        BALL_SIZE,
        BALL_SIZE
    );
    SDL_RenderTexture(this->renderer, texture, &srcRect, &destRect);
    float x = destRect.x + destRect.w * 0.5f;
    float y = destRect.y + destRect.h * 0.5f;
    drawText(std::to_string(ball.speed).substr(0, 5), colorDivisor.getTextColor(), x, y, TextAlignment::CENTER_ALIGNED, 0.45f);
}

void Game::renderEditorBeatLine(size_t beat, float y)
{
    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderLine(
        this->renderer,
        GAMEPLAY_OFFSET, y,
        GAMEPLAY_OFFSET + GAMEPLAY_WIDTH, y
    );

    // Print a beat number.
    std::string str = std::to_string(beat);

    this->drawText(
        str,
        SDL_Color {0xFF, 0xFF, 0xFF, 0xFF},
        GAMEPLAY_OFFSET - 30,
        y,
        TextAlignment::RIGHT_ALIGNED,
        0.5f
    );

    y += 0.5f * this->beatSpacing;

    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0x7F);
    SDL_SetRenderDrawBlendMode(this->renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderLine(
        this->renderer,
        GAMEPLAY_OFFSET, y,
        GAMEPLAY_OFFSET + GAMEPLAY_WIDTH, y
    );
}

void Game::drawText(std::string str, SDL_Color color, float x, float y, TextAlignment align, float size)
{
    SDL_Surface* textSurface = TTF_RenderText_Blended(
        this->font,
        str.c_str(),
        str.length(),
        color
    );

    if (textSurface == nullptr) return;
    
    // Convert it to a texture.
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(this->renderer, textSurface);
    float textWidth = textTexture->w * size;  // up/downsize width
    float textHeight = textTexture->h * size; // up/downsize height
    float drawX;
    switch (align)
    {
        case TextAlignment::LEFT_ALIGNED:
            drawX = x;
            break;
        case TextAlignment::RIGHT_ALIGNED:
            drawX = x - textWidth;
            break;
        case TextAlignment::CENTER_ALIGNED:
            drawX = x - textWidth / 2;
            break;
    }
    SDL_FRect destRect =
        {
            drawX,
            y - textHeight / 2,
            textWidth,
            textHeight
        };
    SDL_DestroySurface(textSurface);
    SDL_RenderTexture(this->renderer, textTexture, NULL, &destRect);
    SDL_DestroyTexture(textTexture);
}

void Game::drawDivisorArrow()
{
    SDL_Texture* texture = this->textureLibrary->divisorArrows;

    // Draw a triangle like this:
    //     /|
    //    / |  1 - color of the ghost ball
    //   /| |  2 - color of the divisor selected.
    //  /1|2|
    //  \ | |
    //   \| |
    //    \ |
    //     \|

    SDL_FRect srcRect =
        this->textureLibrary->createRect(
            DIVISOR_ARROW_WIDTH * this->selectedDivisor.getColor(),
            0,
            DIVISOR_ARROW_WIDTH,
            DIVISOR_ARROW_HEIGHT
        );

    SDL_FRect destRect =
        this->textureLibrary->createRect(
            GAMEPLAY_OFFSET + GAMEPLAY_WIDTH + 10,
            EDITOR_SELECTED_BEAT_Y - (float) DIVISOR_ARROW_HEIGHT * 0.5f,
            DIVISOR_ARROW_WIDTH,
            DIVISOR_ARROW_HEIGHT
        );

    SDL_RenderTexture(
        this->renderer,
        texture,
        &srcRect,
        &destRect
    );

    minibeat miniBeats = Ball::toMinibeats(this->beat);
    ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeats);

    drawText(
        std::to_string(this->selectedDivisor.getTh()).c_str(),
        (SDL_Color) { 255, 255, 255, 255 },
        destRect.x + destRect.w + 10,
        destRect.y + destRect.h / 2,
        TextAlignment::LEFT_ALIGNED,
        0.5f
    );

    srcRect.x = DIVISOR_ARROW_WIDTH * colorDivisor.getColor();
    srcRect.w = DIVISOR_ARROW_WIDTH / 2;
    destRect.w = DIVISOR_ARROW_WIDTH / 2;

    SDL_RenderTexture(
        this->renderer,
        texture,
        &srcRect,
        &destRect
    );
}

bool Game::isFast(float ballSpeed)
{
    return ballSpeed >= FAST_BALL_SPEED;
}

void Game::handleMouseWheelEvent(SDL_Event* event)
{
    minibeat minibeats = Ball::toMinibeats(this->beat);
    // Add/Subtract required minibeats and convert back.
    minibeat netChange = this->selectedDivisor.getWorth();
    float direction = -event->wheel.y;
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
}

void Game::handleMouseButtonDownEvent(SDL_Event* event)
{
    std::optional<GameplayLeftPosition> ballPosition = this->ballCanBePlaced();
    if (ballPosition)
    {
        // The ball can be placed. Add the ball.
        float ballX = ballPosition->x - GAMEPLAY_WIDTH / 2;
        Ball* ball = new Ball(this->beat, this->selectedSpeed, ballX);
        // Add the ball.
        addBall(*ball);
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
        case SDLK_EQUALS:
            // Increase spacing between two beats.
            beatSpacing += EDITOR_BEAT_SEPARATION_STEP;
            if (beatSpacing > EDITOR_BEAT_SEPARATION_MAX)
                beatSpacing = EDITOR_BEAT_SEPARATION_MAX;
            break;
        case SDLK_MINUS:
            // Decrease spacing between two beats.
            beatSpacing -= EDITOR_BEAT_SEPARATION_STEP;
            if (beatSpacing < EDITOR_BEAT_SEPARATION_MIN)
                beatSpacing = EDITOR_BEAT_SEPARATION_MIN;
            break;
        case SDLK_L:
            // Open file picker.
            if (!filePickerOpen)
            {
                openLoadFilePicker();
            }
            break;
        case SDLK_S:
            // Save file picker.
            if (!filePickerOpen)
            {
                openSaveFilePicker();
            }
            break;
    }
}

bool Game::handleMenuEvent(SDL_Event* event)
{
    char toAdd = '\0';
    switch (event->type)
    {
        case SDL_EVENT_KEY_DOWN:
            switch (event->key.key)
            {
                case SDLK_B:
                    // Open/close BPM changer.
                    if (gameState == GameState::EDITING_BPM)
                    {
                        gameState = GameState::EDITING_NONE; // BPM menu closed.
                    }
                    else
                    {
                        resetInput();
                        gameState = GameState::EDITING_BPM; // BPM menu open.
                    }
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
                    break;
            }
    }
    // Handle number input.
    if (event->type == SDL_EVENT_KEY_DOWN && inputModeEnabled() == InputMode::NUMERIC)
    {
        SDL_Keycode key = event->key.key;
        char n = '\0';
        if (key >= SDLK_0 && key <= SDLK_9)
            n = (char) (key - SDLK_0) + '0';
        else if (key == SDLK_KP_0)
            n = '0';
        else if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
            n = (char) (key - SDLK_KP_1) + '1';
        else if (key == SDLK_PERIOD || key == SDLK_KP_PERIOD)
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
        else if (key == SDLK_BACKSPACE && inputCursor > 0)
        {
            inputText.erase(inputCursor - 1, 1u);
            inputCursor--;
        }
        else if (key == SDLK_DELETE && inputCursor < inputText.size())
        {
            inputText.erase(inputCursor, 1u);
        }
        if (n)
        {
            inputText.insert(inputCursor, 1u, n);
            inputCursor++;
        }
    }
    return this->gameState != GameState::EDITING_NONE;
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
}

InputMode Game::inputModeEnabled()
{
    if (gameState == GameState::EDITING_BPM) return InputMode::NUMERIC;
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
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            handleMouseButtonDownEvent(event);
            break;
        case SDL_EVENT_KEY_DOWN:
            handleKeyDownEvent(event);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            this->mousePosition[0] = event->motion.x;
            this->mousePosition[1] = event->motion.y;
    }
}

void Game::addBall(const Ball& ball)
{
    // We want to find the position where the ball can be added such
    // that the balls are in ascending order depending on their beat.
    this->balls.insert(
        std::lower_bound(balls.begin(), balls.end(), ball),
        ball
    );
}

void Game::addBpmChange(const BpmChange& bpmChange)
{
    // We want to find the position where the ball can be added such
    // that the balls are in ascending order depending on their beat.
    this->bpmChanges.insert(
        std::lower_bound(bpmChanges.begin(), bpmChanges.end(), bpmChange),
        bpmChange
    );
    // Remove unnecessary changes.
    double lastBpm = NAN;
    for (auto bpmChange = bpmChanges.begin(); bpmChange <= bpmChanges.end();)
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