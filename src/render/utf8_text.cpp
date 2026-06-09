#include "render/utf8_text.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

struct SDL_Surface;
struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace czgba::render {
namespace {

using TTF_InitFn = int (*)();
using TTF_QuitFn = void (*)();
using TTF_OpenFontIndexFn = TTF_Font* (*)(const char*, int, long);
using TTF_CloseFontFn = void (*)(TTF_Font*);
using TTF_SizeUTF8Fn = int (*)(TTF_Font*, const char*, int*, int*);
using TTF_RenderUTF8_BlendedFn = SDL_Surface* (*)(TTF_Font*, const char*, SDL_Color);
using TTF_SetFontHintingFn = void (*)(TTF_Font*, int);

constexpr int kTtfHintingLight = 1;
constexpr int kFontPx = 12;

std::string getenv_string(const char* name)
{
    if (const char* value = std::getenv(name)) {
        return value;
    }
    return {};
}

std::vector<std::filesystem::path> font_candidates()
{
    std::vector<std::filesystem::path> paths;

    const auto configured = getenv_string("CARDPUTER_ZERO_GBA_CJK_FONT");
    if (!configured.empty()) {
        paths.emplace_back(configured);
    }

#ifdef _WIN32
    paths.emplace_back("C:/Windows/Fonts/NotoSansSC-VF.ttf");
    paths.emplace_back("C:/Windows/Fonts/msyh.ttc");
    paths.emplace_back("C:/Windows/Fonts/simhei.ttf");
    paths.emplace_back("C:/Windows/Fonts/simsun.ttc");
#else
    paths.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    paths.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc");
    paths.emplace_back("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf");
    paths.emplace_back("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
    paths.emplace_back("/usr/share/fonts/truetype/arphic/uming.ttc");
#endif

    return paths;
}

std::vector<const char*> library_candidates()
{
#ifdef _WIN32
    return {"SDL2_ttf.dll", "libSDL2_ttf-2.0-0.dll"};
#elif defined(__APPLE__)
    return {"libSDL2_ttf-2.0.0.dylib", "libSDL2_ttf.dylib"};
#else
    return {"libSDL2_ttf-2.0.so.0", "libSDL2_ttf-2.0.so"};
#endif
}

template <typename Fn>
Fn load_function(void* library, const char* name)
{
    return reinterpret_cast<Fn>(SDL_LoadFunction(library, name));
}

std::vector<std::string_view> utf8_prefixes(const std::string& text)
{
    std::vector<std::string_view> prefixes;
    prefixes.reserve(text.size());

    std::size_t next = 0;
    while (next < text.size()) {
        const auto byte = static_cast<unsigned char>(text[next]);
        std::size_t width = 1;
        if ((byte & 0x80u) == 0) {
            width = 1;
        } else if ((byte & 0xe0u) == 0xc0u) {
            width = 2;
        } else if ((byte & 0xf0u) == 0xe0u) {
            width = 3;
        } else if ((byte & 0xf8u) == 0xf0u) {
            width = 4;
        }

        if (next + width > text.size()) {
            break;
        }
        next += width;
        prefixes.emplace_back(text.data(), next);
    }

    return prefixes;
}

} // namespace

struct Utf8Text::Impl {
    void* library = nullptr;
    TTF_Font* font = nullptr;
    TTF_InitFn init = nullptr;
    TTF_QuitFn quit = nullptr;
    TTF_OpenFontIndexFn open_font_index = nullptr;
    TTF_CloseFontFn close_font = nullptr;
    TTF_SizeUTF8Fn size_utf8 = nullptr;
    TTF_RenderUTF8_BlendedFn render_utf8_blended = nullptr;
    TTF_SetFontHintingFn set_font_hinting = nullptr;
    bool initialized = false;

    Impl()
    {
        for (const auto* candidate : library_candidates()) {
            library = SDL_LoadObject(candidate);
            if (library != nullptr) {
                break;
            }
        }
        if (library == nullptr) {
            return;
        }

        init = load_function<TTF_InitFn>(library, "TTF_Init");
        quit = load_function<TTF_QuitFn>(library, "TTF_Quit");
        open_font_index = load_function<TTF_OpenFontIndexFn>(library, "TTF_OpenFontIndex");
        close_font = load_function<TTF_CloseFontFn>(library, "TTF_CloseFont");
        size_utf8 = load_function<TTF_SizeUTF8Fn>(library, "TTF_SizeUTF8");
        render_utf8_blended = load_function<TTF_RenderUTF8_BlendedFn>(library, "TTF_RenderUTF8_Blended");
        set_font_hinting = load_function<TTF_SetFontHintingFn>(library, "TTF_SetFontHinting");

        if (init == nullptr || quit == nullptr || open_font_index == nullptr || close_font == nullptr ||
            size_utf8 == nullptr || render_utf8_blended == nullptr) {
            return;
        }

        if (init() != 0) {
            return;
        }
        initialized = true;

        for (const auto& path : font_candidates()) {
            if (!std::filesystem::exists(path)) {
                continue;
            }

            font = open_font_index(path.string().c_str(), kFontPx, 0);
            if (font != nullptr) {
                break;
            }
        }

        if (font != nullptr && set_font_hinting != nullptr) {
            set_font_hinting(font, kTtfHintingLight);
        }
    }

    ~Impl()
    {
        if (font != nullptr && close_font != nullptr) {
            close_font(font);
            font = nullptr;
        }
        if (initialized && quit != nullptr) {
            quit();
            initialized = false;
        }
        if (library != nullptr) {
            SDL_UnloadObject(library);
            library = nullptr;
        }
    }
};

Utf8Text::Utf8Text()
    : impl_(std::make_unique<Impl>())
{
}

Utf8Text::~Utf8Text() = default;

bool Utf8Text::available() const
{
    return impl_ != nullptr && impl_->font != nullptr;
}

int Utf8Text::text_width(const std::string& text) const
{
    if (!available() || text.empty()) {
        return 0;
    }

    int width = 0;
    if (impl_->size_utf8(impl_->font, text.c_str(), &width, nullptr) != 0) {
        return 0;
    }
    return width;
}

void Utf8Text::draw_text(Canvas& canvas, int x, int y, const std::string& text, Canvas::Pixel color) const
{
    if (!available() || text.empty()) {
        return;
    }

    const SDL_Color sdl_color{
        static_cast<Uint8>((color >> 16) & 0xff),
        static_cast<Uint8>((color >> 8) & 0xff),
        static_cast<Uint8>(color & 0xff),
        255};

    SDL_Surface* surface = impl_->render_utf8_blended(impl_->font, text.c_str(), sdl_color);
    if (surface == nullptr) {
        return;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (converted == nullptr) {
        return;
    }

    const auto* pixels = static_cast<const std::uint32_t*>(converted->pixels);
    const int pitch_pixels = converted->pitch / static_cast<int>(sizeof(std::uint32_t));
    for (int py = 0; py < converted->h; ++py) {
        for (int px = 0; px < converted->w; ++px) {
            const auto src = pixels[static_cast<std::size_t>(py) * pitch_pixels + px];
            const auto alpha = static_cast<unsigned int>((src >> 24) & 0xff);
            if (alpha < 32) {
                continue;
            }
            canvas.set_pixel(x + px, y + py, color);
        }
    }

    SDL_FreeSurface(converted);
}

std::string Utf8Text::ellipsize_to_width(const std::string& text, int max_width) const
{
    if (!available() || text_width(text) <= max_width) {
        return text;
    }

    constexpr std::string_view ellipsis = "...";
    const int ellipsis_width = text_width(std::string(ellipsis));
    if (ellipsis_width >= max_width) {
        return std::string(ellipsis);
    }

    std::string result;
    for (const auto prefix : utf8_prefixes(text)) {
        std::string candidate(prefix);
        candidate += ellipsis;
        if (text_width(candidate) > max_width) {
            break;
        }
        result = std::move(candidate);
    }

    if (result.empty()) {
        return std::string(ellipsis);
    }
    return result;
}

} // namespace czgba::render
