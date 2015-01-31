#include "mathvm.h"

#include <string>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef MATHVM_WITH_SDL
#include <SDL/SDL.h>
#endif

using namespace std;

namespace mathvm {

char* loadFile(const char* file) {
	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		return 0;
	}
	struct stat statBuf;
	if (fstat(fd, &statBuf) != 0) {
		return 0;
	}

	off_t size = statBuf.st_size;
	uint8_t* result = new uint8_t[size + 1];

	int rv = read(fd, result, size);
	assert(rv == size);
	result[size] = '\0';
	close(fd);

	return (char*)result;
}

void positionToLineOffset(const string& text,
						  uint32_t position, uint32_t& line, uint32_t& offset) {
	uint32_t pos = 0, max_pos = text.length();
	uint32_t current_line = 1, line_start = 0;
	while (pos < max_pos) {
		while (pos < max_pos && text[pos] != '\n') {
			pos++;
		}
		if (pos >= position) {
			line = current_line;
			offset = position - line_start;
			return;
		}
		assert(text[pos] == '\n' || pos == max_pos);
		pos++; // Skip newline.
		line_start = pos;
		current_line++;
	}
}

const char* typeToName(VarType type) {
	switch (type) {
		case VT_INT: return "int";
		case VT_DOUBLE: return "double";
		case VT_STRING: return "string";
		case VT_VOID: return "void";
		default: return "invalid";
	}
}


VarType nameToType(const string& typeName) {
	if (typeName == "int") {
		return VT_INT;
	}
	if (typeName == "double") {
	   return VT_DOUBLE;
	}
	if (typeName == "string") {
		return VT_STRING;
	}
	if (typeName == "void") {
		return VT_VOID;
	}
	return VT_INVALID;
}

const char* bytecodeName(Instruction insn, size_t* length) {
    static const struct {
        const char* name;
        Instruction insn;
        size_t length;
    } names[] = {
#define BC_NAME(b, d, l) {#b, BC_##b, l},
        FOR_BYTECODES(BC_NAME)
    };

    if (insn >= BC_INVALID && insn < BC_LAST) {
        if (length != 0) {
            *length = names[insn].length;
        }
        return names[insn].name;
    }

    assert(false);
    return 0;
}

extern "C" void unsafe_setMem(void* base, int64_t offset, int64_t value, int64_t width) {
	uint8_t* ptr = (uint8_t*)base + offset;
	switch(width) {
		case 1:
			*(uint8_t*)ptr = value;
			break;
		case 2:
			*(uint16_t*)ptr = value;
			break;
		case 4:
			*(uint32_t*)ptr = value;
			break;
		case 8:
			*(uint64_t*)ptr = value;
			break;
		default:
			assert(false);
	}
}

extern "C" int64_t unsafe_getMem(void* base, int64_t offset, int64_t width) {
	uint8_t* ptr = (uint8_t*)base + offset;
	switch(width) {
		case 1:
			return *(uint8_t*)ptr;
		case 2:
			return *(uint16_t*)ptr;
		case 4:
			return *(uint32_t*)ptr;
		case 8:
			return *(uint64_t*)ptr;
		default:
			assert(false);
			return 0;
	}
}

#ifdef MATHVM_WITH_SDL
static SDL_Surface * sdl_screen = 0;

extern "C" int64_t unsafe_videoInitFramebuffer(int64_t width, int64_t height, int64_t bpp, int64_t fullscreen) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	int flags = SDL_HWSURFACE;
	if (fullscreen) {
		flags |= SDL_FULLSCREEN;
	}
	if (!(sdl_screen = SDL_SetVideoMode(width, height, bpp, flags))) {
		SDL_Quit();
		return 0;
	}

	return 1;
}

extern "C" void unsafe_videoDeinitFramebuffer() {
	SDL_Quit();
}

extern "C" int64_t unsafe_videoCheckEvent() {
	SDL_Event event;
	if (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
			case SDL_KEYDOWN:
				return 1;
		}
	}
	return 0;
}

extern "C" void* unsafe_videoGrabFramebuffer() {
	if (SDL_MUSTLOCK(sdl_screen)) {
		if (SDL_LockSurface(sdl_screen) < 0) {
			return 0;
		}
	}
	return sdl_screen->pixels;
}

extern "C" void unsafe_videoReleaseFramebuffer() {
	if (SDL_MUSTLOCK(sdl_screen)) {
		SDL_UnlockSurface(sdl_screen);
	}
	SDL_Flip(sdl_screen);
}

extern "C" int64_t native_timestamp() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

#endif // MATHVM_WITH_SDL

} // namespace mathvm
