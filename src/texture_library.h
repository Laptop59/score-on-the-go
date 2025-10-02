#include <SDL3_image/SDL_image.h>
#include <filesystem>

#ifndef SCORE_ON_THE_GO_TEXTURE_LIBRARY
#define SCORE_ON_THE_GO_TEXTURE_LIBRARY

class TextureLibrary
{
    public:
        // Texture of balls to be hit.
        SDL_Texture* balls;

        // Textures of ball mines to be avoided.
        SDL_Texture* mines;

        // Texture of a flash.
        SDL_Texture* flash;

        // Texture of a segment of a tail of a tailed circle.
        SDL_Texture* tail;

        // Texture of squares.
        SDL_Texture* squares;

        // SDL Renderer for creating textures.
        SDL_Renderer* renderer;

        // The base path (from SDL).
        std::filesystem::path basePath;

        // Creates a texture library.
        TextureLibrary(SDL_Renderer* renderer, std::filesystem::path basePath);

        // Deconstructs the texture library.
        ~TextureLibrary();

        // Load textures. This function should only be called once.
        void loadTextures();

        // Create a FRect for use in SDL3.
        SDL_FRect createRect(float x, float y, float w, float h);

    private:
        // Load a texture.
        bool loadTexture(SDL_Texture** texture, const char* path);

        // Unload a texture.
        void unloadTexture(SDL_Texture** texture);
};

#endif