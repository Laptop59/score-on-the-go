// Helper file for assertion.
#include <SDL3/SDL_assert.h>

#define ASSERT(cond) SDL_assert(cond)
#define TO_FALSE(...) (false)
#define UNEXPECTED(...) ASSERT(TO_FALSE(...))