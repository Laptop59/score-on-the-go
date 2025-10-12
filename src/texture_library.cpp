#include <SDL3_image/SDL_image.h>
#include <memory>

#include "texture_library.h"

TextureLibrary::TextureLibrary(SDL_Renderer* renderer, std::filesystem::path basePath)
{
    // Fetch the renderer.
    this->renderer = renderer;
    this->basePath = basePath;
}

void TextureLibrary::loadTextures()
{
    this->loadTexture(&this->balls, "resources/balls.png");
    this->loadTexture(&this->flash, "resources/flash.png");
    this->loadTexture(&this->tail, "resources/tail.png");
    this->loadTexture(&this->pitTail, "resources/pit_tail.png");
    this->loadTexture(&this->mine, "resources/mine.png");
    this->loadTexture(&this->mineFlash, "resources/mine_flash.png");
    this->loadTexture(&this->squares, "resources/squares.png");
}

TextureLibrary::~TextureLibrary()
{
    // Unload textures.
    this->unloadTexture(&this->balls);
    this->unloadTexture(&this->flash);
    this->unloadTexture(&this->tail);
    this->unloadTexture(&this->pitTail);
    this->unloadTexture(&this->mine);
    this->unloadTexture(&this->mineFlash);
    this->unloadTexture(&this->squares);
}

bool TextureLibrary::loadTexture(SDL_Texture** texture, const char* path)
{
    // Try to get a SDL_Texture.
    SDL_Texture* tex = IMG_LoadTexture(this->renderer, (this->basePath / path).string().c_str());
    if (tex)
    {
        *texture = tex;
        return true;
    }
    else
    {
        *texture = NULL;
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Could not load texture from path: %s", path);
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Texture Load Error: %s", SDL_GetError());
        return false;
    }
}

void TextureLibrary::unloadTexture(SDL_Texture** texture)
{
    // Get its pointer.
    if (texture == nullptr) return;
    SDL_Texture* ptr = *texture;
    if (ptr != nullptr)
    {
        // Free the texture.
        SDL_DestroyTexture(ptr);
        *texture = nullptr;
    }
}

SDL_FRect TextureLibrary::createRect(float x, float y, float w, float h)
{
    SDL_FRect rect = { x, y, w, h };
    return rect;
}