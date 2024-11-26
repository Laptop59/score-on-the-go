#include <SDL3_image/SDL_image.h>

#ifndef SCORE_ON_THE_GO_TEXTURE_LIBRARY
#define SCORE_ON_THE_GO_TEXTURE_LIBRARY

class TextureLibrary
{
    public:
        // Texture of balls to be hit.
        SDL_Texture* balls;

        // Texture of arrows to specify the color divisor.
        SDL_Texture* divisorArrows;

        // Texture of a flash.
        SDL_Texture* flash;

        // SDL Renderer for creating textures.
        SDL_Renderer* renderer;

        // Creates a texture library.
        TextureLibrary(SDL_Renderer* renderer);

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