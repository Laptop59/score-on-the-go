#define SDL_MAIN_USE_CALLBACKS  // This is necessary for the new callbacks API.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <cmath>
#include <memory>
#include "game.h"
#include <filesystem>

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* fontOutlined;
    std::unique_ptr<Game> game;
    SDL_Time time;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
    MIX_Mixer* mixer;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Initialize the library with VIDEO capabilities.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Initialisation Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // Create a window and the renderer.
    SDL_Renderer* renderer;
    SDL_Window* window;

    int width = SCREEN_WIDTH, height = SCREEN_HEIGHT;

    SDL_DisplayID displayId = SDL_GetPrimaryDisplay();
    if (displayId)
    {
        SDL_Rect rect;
        SDL_GetDisplayUsableBounds(displayId, &rect);
        int scaleX = rect.w / SCREEN_WIDTH;
        int scaleY = rect.h / SCREEN_HEIGHT;
        int minScale = std::min(scaleX, scaleY);
        if (minScale > 1)
        {
            width *= minScale;
            height *= minScale;
        }
    }

    SDL_Log("Attempting to use %d by %d as window resolution.", width, height);
    if (!SDL_CreateWindowAndRenderer(NAME, width, height, SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Window/Renderer Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // Show the Window.
    SDL_ShowWindow(window);

    #if __EMSCRIPTEN__
        // For web, set this hint as not doing so for the web will cause an error (NULL does not work there.)
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "portal");
    #endif

    // Determine the base path.
    #if __ANDROID__
        std::filesystem::path basePath = "";   // on Android we do not want to use basepath. Instead, assets are available at the root directory.
    #else
        auto basePathPtr = SDL_GetBasePath();
        if (not basePathPtr) {
            SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Could not find the base path.");
            return SDL_APP_FAILURE;
        }
        std::filesystem::path basePath = basePathPtr;
        basePath = basePath.parent_path();
    #endif

    // Now try to initialize font
    if (!TTF_Init())
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL TTF Initialization Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto fontCPath = (basePath / FONT_PATH).string();
    TTF_Font* font = TTF_OpenFont(fontCPath.c_str(), 64);
    TTF_Font* fontOutlined = TTF_OpenFont(fontCPath.c_str(), 64);
    if (font == nullptr || fontOutlined == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Opening Game Font Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Setup audio initialization here.
    if (!MIX_Init())
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Warning: SDL MIX could not initialize: %s", SDL_GetError());
    }

    // Set up the application data
    AppContext* ac = new AppContext();

    // Open an audio device.
    ac->mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    ac->window = window;
    ac->renderer = renderer;
    ac->font = font;
    ac->fontOutlined = fontOutlined;
    ac->app_quit = SDL_APP_CONTINUE;
    ac->game = std::unique_ptr<Game> (new Game(renderer, window, font, fontOutlined, ac->mixer));
    ac->game->textureLibrary = std::unique_ptr<TextureLibrary> (new TextureLibrary(renderer, basePath));
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
        // Save preferences.
        app->game->savePrefs();
        app->game.reset();
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        TTF_CloseFont(app->font);
        TTF_CloseFont(app->fontOutlined);
        MIX_DestroyMixer(app->mixer);
        delete app;
    }

    MIX_Quit();
    TTF_Quit();
    SDL_Quit();
    SDL_Log("Score on the Go: Bye!");
}

extern "C" {
    void Game_loadPrefs(Game* game) {
        game->loadPrefs();
    }
}
