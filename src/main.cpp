#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <memory>
#include "game.h"

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    std::unique_ptr<Game> game;
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
    if (font == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Opening Game Font Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!IMG_Init(IMG_INIT_PNG))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Initializing SDL IMG Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Set up the application data
    AppContext* ac = new AppContext();
    ac->window = window;
    ac->renderer = renderer;
    ac->font = font;
    ac->app_quit = SDL_APP_CONTINUE;
    ac->game = std::unique_ptr<Game> (new Game(renderer, window, font));
    ac->game->textureLibrary = std::unique_ptr<TextureLibrary> (new TextureLibrary(renderer));
    ac->game->textureLibrary->loadTextures();

    // Now assign to the appstate.
    *appstate = ac;
    
    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    auto* app = (AppContext*)appstate;
    
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
    auto* app = (AppContext*)appstate;

    app->game->render();

    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = (AppContext*) appstate;

    if (app) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        TTF_CloseFont(app->font);
        app->game.reset();
        delete app;
    }

    TTF_Quit();
    SDL_Quit();
    SDL_Log("Application quit successfully!");
}
