#include "game.h"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <string>
#include <iostream>
#include <optional>
#include <algorithm>
#include "assert.h"

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

    this->music = nullptr;
}

Game::~Game()
{
    // Free file filter.
    delete this->fileFilter;

    // Free music.
    if (this->music != nullptr)
    {
        Mix_FreeMusic(this->music);
    }
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

void Game::renderPlaytest()
{
    // Render paddle
    SDL_FRect paddleRect = SDL_FRect {
        GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2 + paddlePosition - PADDLE_WIDTH / 2,
        PADDLE_TOP,
        PADDLE_WIDTH,
        PADDLE_HEIGHT
    };
    SDL_SetRenderDrawColor(this->renderer, 0xFFu, 0xFFu, 0xFFu, 0xFFu);
    SDL_RenderFillRect(this->renderer, &paddleRect);
    // Outline is 5 pixels. However its an odd number, so 4 is good here.
    paddleRect.x += 2;
    paddleRect.y += 2;
    paddleRect.w -= 4;
    paddleRect.h -= 4;
    SDL_SetRenderDrawColor(this->renderer, 0x33u, 0x33u, 0x33u, 0xFFu);
    SDL_RenderFillRect(this->renderer, &paddleRect);

    SDL_Color whiteColor = SDL_Color { 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    float x = GAMEPLAY_OFFSET + GAMEPLAY_WIDTH + 5;
    drawText("Beat: ", whiteColor, x, 25, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText(std::to_string(beat), whiteColor, x, 45, TextAlignment::LEFT_ALIGNED, 0.5f);

    drawText("Seconds: ", whiteColor, x, 75, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText(std::to_string(seconds), whiteColor, x, 95, TextAlignment::LEFT_ALIGNED, 0.5f);

    SDL_FRect srcRect, destRect;

    // Render balls.
    for (auto ball = queuedBalls.begin();
        ball < queuedBalls.end();
        ++ball)
    {
        float renderY = getSignedFallingBallPos(*ball) + BALL_SIZE / 2;
        if (renderY > SCREEN_HEIGHT / 2 + BALL_SIZE / 2) continue; // Off-screen, skip rendering.
        float renderX = ball->x - BALL_SIZE / 2;
        destRect = SDL_FRect {
            renderX + GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2 - renderY,
            BALL_SIZE,
            BALL_SIZE
        };
        minibeat miniBeatsOfBall = ball->at;
        ColorDivisor colorDivisor = ColorDivisor::getColorDivisor(miniBeatsOfBall);
        srcRect = this->textureLibrary->createRect(
            (2 * colorDivisor.getColor() + isFast(ball->speed)) * BALL_SIZE,
            0,
            BALL_SIZE,
            BALL_SIZE
        );
        SDL_RenderTexture(this->renderer, this->textureLibrary->balls, &srcRect, &destRect);
    }
    this->renderFlashesAndJudgement();
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

    SDL_Color white = SDL_Color { 0xFF, 0xFF, 0xFF, 0xFF };
    drawText("Selected", white, 5, 10, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText("Speed:", white, 5, 25, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText(std::to_string(this->selectedSpeed), white, 5, 40, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText("Balls:", white, 5, 55, TextAlignment::LEFT_ALIGNED, 0.5f);
    drawText(std::to_string(this->balls.size()), white, 5, 70, TextAlignment::LEFT_ALIGNED, 0.5f);

    float yy = 85;
    for (auto ball = balls.begin(); ball != balls.end(); ++ball)
    {
        drawText(std::to_string(ball->at), white, 5, yy, TextAlignment::LEFT_ALIGNED, 0.5f);
        yy += 15;
    }

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

    // Now draw lines.
    for (auto ball = this->balls.begin(); ball != this->balls.end(); ++ball)
    {
        if (std::holds_alternative<BallTypeHold>(ball->type))
        {
            float x1 = ball->x, x2;
            minibeat y1 = ball->at, y2;
            BallTypeHold h = std::get<BallTypeHold>(ball->type);
            for (auto point = h.points.begin(); point != h.points.end(); ++point)
            {
                x2 = point->x;
                y2 = ball->at + point->minibeats;
                {
                    // Draw points.
                    float p1 = x1 + GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2;
                    float p2 = EDITOR_SELECTED_BEAT_Y + (Ball::toBeats(y1) - this->beat) * beatSpacing;
                    float p3 = x2 + GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2;
                    float p4 = EDITOR_SELECTED_BEAT_Y + (Ball::toBeats(y2) - this->beat) * beatSpacing;
                    SDL_SetRenderDrawColor(this->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                    SDL_RenderLine(this->renderer, p1, p2, p3, p4);
                }
                x1 = x2;
                y1 = y2;
            }
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

void Game::openLoadMusicPicker()
{
    // Pass the game object to userdata for later.
    this->filePickerOpen = true;
    SDL_ShowOpenFileDialog(
        &callbackLoadMusicPicker,
        this,
        this->window,
        this->fileFilter,
        0,
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

void SDLCALL Game::callbackLoadMusicPicker(void* userdata, const char* const* filelist, int filter)
{
    // Userdata is our game object.
    Game* game = (Game*) userdata;
    game->filePickerOpen = false;
    if (!filelist)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Load Music Picker: %s", SDL_GetError());
    }
    else if (*filelist)
    {
        // Get the first item (we don't care about the other ones)
        const char* file = *filelist;

        // Load the music.
        Mix_Music* music = Mix_LoadMUS(file);

        if (music == NULL)
        {
            SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error with Loading Music: %s", SDL_GetError());
            return;
        }

        // Free the existing music.
        if (game->music != NULL)
        {
            Mix_FreeMusic(game->music);
        }
        game->music = music;
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
        GAMEPLAY_OFFSET - 5,
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

void Game::moveTimesDivisor(float direction)
{
    minibeat minibeats = Ball::toMinibeats(this->beat);
    // Add/Subtract required minibeats and convert back.
    minibeat netChange = this->selectedDivisor.getWorth();
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
        return;
    }
    else return;
    if (std::holds_alternative<BallTypeNormal>(ball.type))
    {
        // Normal --> Hold
        ball.type = BallTypeHold {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            }
        };
    }
    else if (std::holds_alternative<BallTypeHold>(ball.type))
    {
        // Update the hold point.
        ball.type = BallTypeHold {
            { 
                BallTypeTailPoint { ball.x, holdLength }
            }
        };
    }
}

void Game::handleMouseWheelEvent(SDL_Event* event)
{
    moveTimesDivisor(-event->wheel.y);
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
        std::vector<BallTypeTailPoint>* points = nullptr;
        if (std::holds_alternative<BallTypeHold>(ball->type))
        {
            points = &std::get<BallTypeHold>(ball->type).points;
        }
        if (points != nullptr)
        {
            std::vector<BallTypeTailPoint>& vector = *points;
            for (auto point = vector.begin(); point != vector.end(); ++point)
            {
                float mx = mousePosition[0] - GAMEPLAY_OFFSET - GAMEPLAY_WIDTH / 2;
                float dx = mx - point->x;
                if (std::abs(dx) > 10) continue;
                float my = mousePosition[1];
                float dy =
                    my - ((Ball::toBeats(ball->at + point->minibeats) - this->beat) * beatSpacing + EDITOR_SELECTED_BEAT_Y);
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
                Ball* ball = new Ball(this->beat, this->selectedSpeed, ballX);
                // Add the ball.
                ballCheckedForTail = addBall(*ball);
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
            moveTimesDivisor(-1.0f);
            break;
        case SDLK_DOWN:
            moveTimesDivisor(1.0f);
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
        case SDLK_COMMA:
        case SDLK_PERIOD:
            {
                float netChange = event->key.key == SDLK_COMMA ? -0.1f : 0.1f;
                if (event->key.mod & SDL_KMOD_CTRL) netChange /= 10;
                if (event->key.mod & SDL_KMOD_SHIFT) netChange /= 10;
                if (event->key.mod & SDL_KMOD_ALT) netChange /= 100;
                selectedSpeed += netChange;
                if (selectedSpeed <= 0)
                {
                    selectedSpeed = -netChange;
                }
            }
            break;
        case SDLK_L:
            // Open file picker.
            if (!filePickerOpen)
            {
                openLoadFilePicker();
            }
            break;
        case SDLK_M:
            // Open music picker.
            if (!filePickerOpen)
            {
                openLoadMusicPicker();
            }
            break;
        case SDLK_S:
            // Save file picker.
            if (!filePickerOpen)
            {
                openSaveFilePicker();
            }
            break;
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            shouldClonePoint = true;
            break;
    }
}

void Game::startPlayTest(double beat)
{
    gameState = GameState::PLAYTESTING;
    startPlaytestingBeat = beat;
    seconds = getSecondsFromBeat(startPlaytestingBeat);
    // seconds -= 1.5; // Introduce delay for giving the player time to playtest.
    startPlaytestingBeat = getBeatFromSeconds(seconds);
    this->beat = startPlaytestingBeat;
    // Now expand the queued balls (e.g. holds)
    setQueuedBalls(beat);
    if (seconds >= 0)
    {
        Mix_PlayMusic(this->music, 0);
        Mix_SetMusicPosition(seconds);
    }
}

void Game::setQueuedBalls(double startFrom)
{
    queuedBalls.clear();
    queuedBallFlashes.clear();
    auto iterStartFrom = std::lower_bound(balls.begin(), balls.end(), startFrom);
    std::copy(iterStartFrom, balls.end(), std::back_inserter(queuedBalls));
    for (auto it = queuedBalls.begin(); it != queuedBalls.end(); ++it)
    {
        if (std::holds_alternative<BallTypeHold>(it->type))
        {
            // This ball is a hold.
            BallTypeHold data = std::get<BallTypeHold>(it->type);
            minibeat longBallAt = it->at;
            float longBallX = it->x;
            // Create hold nodes from 0.25 to hold end incrementing by 0.25.
            if (data.points.empty()) continue;
            // Get the last point.
            BallTypeTailPoint lastPoint = *data.points.rbegin();
            for (minibeat node = LONG_BALL_NODE_SPACING; node <= lastPoint.minibeats; node += LONG_BALL_NODE_SPACING)
            {
                float x = lastPoint.x;
                // Calculate the appropriate 'x' value.
                float x1 = longBallX, x2;
                minibeat y1 = 0_mb, y2;
                auto point = data.points.begin();
                while (point != data.points.end())
                {
                    x2 = point->x;
                    y2 = point->minibeats;
                    ASSERT(y1 <= y2);
                    if (node <= y2)
                    {
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
                BallTypeHoldNode type;
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
    Mix_HaltMusic();
}

bool Game::handleMenuEvent(SDL_Event* event)
{
    char toAdd = '\0';
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
                    if (gameState == GameState::EDITING_BPM)
                    {
                        gameState = GameState::EDITING_NONE; // BPM menu closed.
                    }
                    else if (gameState == GameState::EDITING_NONE)
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
    // Handle player input here.
    if (gameState == GameState::PLAYING || gameState == GameState::PLAYTESTING)
    {
        int8_t flags = 0;
        switch (event->key.key)
        {
            case SDLK_A:
                flags |= PaddleKeys::LEFT_FAST;
                break;
            case SDLK_S:
                flags |= PaddleKeys::LEFT_SLOW;
                break;
            case SDLK_D:
                flags |= PaddleKeys::RIGHT_SLOW;
                break;
            case SDLK_F:
                flags |= PaddleKeys::RIGHT_FAST;
                break;
        }
        if (event->type == SDL_EVENT_KEY_DOWN)
            // Let's OR the flags.
            this->paddleKeysPressed |= flags;
        else if (event->type == SDL_EVENT_KEY_UP)
            // Let's AND the inverse flags.
            this->paddleKeysPressed &= ~flags;
    }
    return this->gameState != GameState::EDITING_NONE;
}

void Game::update()
{
    if (gameState != GameState::PLAYING && gameState != GameState::PLAYTESTING) return;
    float speed =
        !!(paddleKeysPressed & PaddleKeys::LEFT_FAST) * -3.5f +
        !!(paddleKeysPressed & PaddleKeys::LEFT_SLOW) * -1.0f +
        !!(paddleKeysPressed & PaddleKeys::RIGHT_FAST) * 3.5f +
        !!(paddleKeysPressed & PaddleKeys::RIGHT_SLOW) * 1.0f;
    float change = speed * PADDLE_SPEED * this->deltaTimePassed;
    paddlePosition += change;
    if (std::abs(paddlePosition) > PADDLE_MAX_LEFT - PADDLE_WIDTH / 2)
    {
        paddlePosition = (paddlePosition / std::abs(paddlePosition)) * (PADDLE_MAX_LEFT - PADDLE_WIDTH / 2);
    }
    // Ball handling.
    if (seconds < 0 && seconds + deltaTimePassed >= 0)
    {
        Mix_PlayMusic(this->music, 0);
        Mix_SetMusicPosition(seconds + deltaTimePassed);
    }
    seconds += deltaTimePassed;
    beat = getBeatFromSeconds(seconds);
    updateBalls();
    updateFlashes();
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

void Game::renderFlashesAndJudgement()
{
    BallFlash* latestFlash = NULL;
    for (auto flash = queuedBallFlashes.begin();
        flash < queuedBallFlashes.end();
        ++flash)
    {
        double timeDiff = seconds - flash->secondsWhenHit;
        // From 0..1, preferably. Might not be...
        float alpha = 0.5 - timeDiff * 3.0 / 2.0;
        SDL_Color flashColor = getFlashColor(flash->judgement);
        alpha *= ((float) flashColor.a) / 255.0f;
        if (alpha > 0)
        {
            SDL_SetTextureColorMod(
                textureLibrary->flash,
                flashColor.r,
                flashColor.g,
                flashColor.b
            );
            SDL_SetTextureAlphaModFloat(textureLibrary->flash, alpha);
            float flashSize = BALL_FLASH_SIZE * (1.0f + timeDiff * 3.0f);
            SDL_FRect destRect = SDL_FRect {
                GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2 + flash->x - flashSize / 2,
                GAMEPLAY_HEIGHT / 2 - flash->y - flashSize / 2,
                flashSize,
                flashSize
            };
            SDL_RenderTexture(
                this->renderer,
                this->textureLibrary->flash,
                NULL,
                &destRect
            );
        }
        latestFlash = &*flash;
    }
    if (latestFlash != NULL)
    {
        // Display judgement.
        drawText(
            getText(latestFlash->judgement),
            getTextColor(latestFlash->judgement),
            GAMEPLAY_OFFSET + GAMEPLAY_WIDTH / 2,
            GAMEPLAY_HEIGHT / 2,
            TextAlignment::CENTER_ALIGNED,
            (1.0f + (seconds - latestFlash->secondsWhenHit) * 1.50f)
        );
    }
    // Revert color mods.
    SDL_SetTextureColorMod(textureLibrary->flash, 0xFF, 0xFF, 0xFF);
    SDL_SetTextureAlphaModFloat(textureLibrary->flash, 255.0f);
}

Judgement Game::getJudgementFromDifference(float difference)
{
    if (difference < PADDLE_WIDTH * 0.2)
        return Judgement::PERFECT;
    else if (difference < PADDLE_WIDTH * 0.35)
        return Judgement::GREAT;
    else
        return Judgement::GOOD;
}

void Game::updateBalls()
{
    for (auto ball = queuedBalls.begin();
        ball < queuedBalls.end();)
    {
        // Check if the ball intersects the rectangle.
        // Taken from https://stackoverflow.com/questions/401847/circle-rectangle-collision-detection-intersection

        float cy = getSignedFallingBallPos(*ball);

        if (cy <= SCREEN_HEIGHT / -2 - BALL_SIZE / 2)
        {
            // Remove the ball and count it as a miss.
            queuedBallFlashes.push_back(BallFlash {
                seconds,
                Judgement::MISS,
                0.0f,
                0.0f
            });
            ball = queuedBalls.erase(ball);
            continue;
        }

        float cx = ball->x;
        float cr = BALL_SIZE / 2;

        SDL_FRect rect = SDL_FRect {
            paddlePosition - PADDLE_WIDTH / 2,
            SCREEN_HEIGHT / 2 - PADDLE_TOP - PADDLE_HEIGHT,
            PADDLE_WIDTH,
            PADDLE_HEIGHT  
        };

        float rx = rect.x + rect.w / 2;
        float ry = rect.y + rect.h / 2;

        float dx = std::abs(rx - cx);
        float dy = std::abs(ry - cy);

        if (dx > rect.w / 2 + cr || dy > rect.h / 2 + cr)
        {
            // Definitely not touching.
            ++ball;
            continue;
        }

        bool isTouching = dx <= (rect.w / 2) || dy <= (rect.h / 2);

        if (!isTouching)
        {
            float cornerDistanceSq =
                std::pow(dx - rect.w / 2, 2.0f) + std::pow(dy - rect.h / 2, 2.0f);
            isTouching = cornerDistanceSq <= cr * cr;
        }

        if (isTouching)
        {
            Judgement judgement = getJudgementFromDifference(std::abs(paddlePosition - ball->x));
            queuedBallFlashes.push_back(BallFlash {
                seconds,
                judgement,
                cx,
                cy
            });
            ball = queuedBalls.erase(ball);
        }
        else
            ++ball;
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
            this->mousePosition[0] = event->motion.x;
            this->mousePosition[1] = event->motion.y;
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
    }
    std::vector<BallTypeTailPoint>::iterator point = selectedPoint.value();
    point->x = this->mousePosition[0] - GAMEPLAY_OFFSET - GAMEPLAY_WIDTH / 2;
    double ballBeats = this->beat + (this->mousePosition[1] - EDITOR_SELECTED_BEAT_Y) / beatSpacing;
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
    if (!bpmChanges.empty())
    {
        BpmChange firstChange = bpmChanges.at(0);
        if (firstChange.beat == 0.0)
            currentBpm = firstChange.bpm;
    }

    double currentSeconds = 0.0;
    double currentBeat = 0.0;
    double lastBeat = 0.0;

    for (auto bpmChange = bpmChanges.begin(); bpmChange < bpmChanges.end(); ++bpmChange)
    {
        double timeBeforeNext = (bpmChange->beat - lastBeat) * 60.0 / currentBpm;
        // Check if the total time now exceeds the seconds.
        if (currentSeconds + timeBeforeNext >= seconds)
        {
            double remainingSeconds = seconds - currentSeconds;
            currentBeat += remainingSeconds * currentBpm / 60.0;
            currentSeconds = seconds;
            break;
        }

        // Otherwise, update the current beat and move the time forward.
        currentBeat += bpmChange->beat - lastBeat;
        currentSeconds += timeBeforeNext;
        
        // Update the current BPM.
        currentBpm = bpmChange->bpm;
        lastBeat = bpmChange->beat;
    }

    double remainingSeconds = seconds - currentSeconds;
    currentBeat += remainingSeconds * currentBpm / 60.0;

    return currentBeat;
}

double Game::getSecondsFromBeat(double beat)
{
    double currentBpm = DEFAULT_BPM;
    if (!bpmChanges.empty())
    {
        BpmChange firstChange = bpmChanges.at(0);
        if (firstChange.beat == 0.0)
            currentBpm = firstChange.bpm;
    }

    double currentSeconds = 0.0;
    double beatsRemaining = beat;
    double lastBeat = 0.0;

    for (auto bpmChange = bpmChanges.begin(); bpmChange < bpmChanges.end(); ++bpmChange)
    {
        // Find the beats between two changes.
        double beatsBetweenChanges = bpmChange->beat - lastBeat;

        if (beatsRemaining <= beatsBetweenChanges) {
            currentSeconds += (beatsRemaining / currentBpm) * 60.0;
            break;
        }

        // Otherwise, accumulate the seconds for the entire section and reduce the remaining beats.
        currentSeconds += (beatsBetweenChanges / currentBpm) * 60.0;
        beatsRemaining -= beatsBetweenChanges;

        // Update the current BPM.
        currentBpm = bpmChange->bpm;
        lastBeat = bpmChange->beat;
    }

    if (beatsRemaining > 0) {
        currentSeconds += (beatsRemaining / currentBpm) * 60.0;
    }

    return currentSeconds;
}

float Game::getSignedFallingBallPos(const Ball& ball)
{
    double miniBeats = ball.at;
    float speed = ball.speed;

    double differenceInBeats =
        (double) (miniBeats - beat * MINIBEATS_PER_BEAT) / MINIBEATS_PER_BEAT;
    double amplifiedDifference = differenceInBeats * speed;

    float y = amplifiedDifference * BALL_SPEED;
    y += PADDLE_TOP_SIGNED + BALL_SIZE / 2;

    return y;
}