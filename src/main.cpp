#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <cmath>
#include <memory>
#include "game.h"

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* fontOutlined;
    std::unique_ptr<Game> game;
    SDL_Time time;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // init the library, here we make a window so we only need the Video capabilities.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Initialisation Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // create a window
    SDL_Window* window = SDL_CreateWindow("Score on the Go", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Window Creation Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // a renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Renderer Creation Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // Show the Window.
    SDL_ShowWindow(window);

    // Now try to initialize font
    if (!TTF_Init())
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL TTF Initialization Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    TTF_Font* font = TTF_OpenFont(FONT_PATH, 32);
    TTF_Font* fontOutlined = TTF_OpenFont(FONT_PATH, 32);
    if (font == nullptr || fontOutlined == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Opening Game Font Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!IMG_Init(IMG_INIT_PNG))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Initializing SDL IMG Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Although it isn't necessary, setup audio initialization here.
    MIX_InitFlags audioFlags = MIX_INIT_MP3;
    MIX_InitFlags previousFlags = Mix_Init(0);
    if (Mix_Init(audioFlags) != audioFlags | previousFlags)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Warning: SDL MIX Error: %s", SDL_GetError());
    }

    // Open an audio device.
    Mix_OpenAudio(0, NULL);

    // Set up the application data
    AppContext* ac = new AppContext();
    ac->window = window;
    ac->renderer = renderer;
    ac->font = font;
    ac->fontOutlined = fontOutlined;
    ac->app_quit = SDL_APP_CONTINUE;
    ac->game = std::unique_ptr<Game> (new Game(renderer, window, font, fontOutlined));
    ac->game->textureLibrary = std::unique_ptr<TextureLibrary> (new TextureLibrary(renderer));
    ac->game->textureLibrary->loadTextures();

    if (!SDL_GetCurrentTime(&ac->time))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Current Time Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Now assign to the appstate.
    *appstate = ac;
    
    SDL_Log("Score on the Go: Welcome! The editor is opened for you.");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    auto* app = (AppContext*) appstate;
    
    if (event->type == SDL_EVENT_QUIT)
    {
        app->app_quit = SDL_APP_SUCCESS;
    }
    else
    {
        app->game->handleEvent(event);
    }
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*) appstate;

    // Update and get delta time.
    SDL_Time newTime = app->time;
    SDL_GetCurrentTime(&newTime);
    int64_t signedDeltaTime = newTime - app->time;
    app->time = newTime;
    uint64_t deltaTime = 0;
    if (signedDeltaTime > 0) deltaTime = signedDeltaTime;
    double deltaSeconds = ((double) deltaTime) / 1.0e9;

    app->game->deltaTimePassed = deltaSeconds;
    app->game->update();
    app->game->render();

    app->game->paddleKeysPressedOnce = 0b0000;
    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = (AppContext*) appstate;

    if (app) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        TTF_CloseFont(app->font);
        TTF_CloseFont(app->fontOutlined);
        app->game.reset();
        delete app;
    }

    Mix_CloseAudio();
    Mix_Quit();
    TTF_Quit();
    SDL_Quit();
    SDL_Log("Score on the Go: Bye!");
}
