#include "stdafx.h"
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <mutex>
#include <utility>
#include <array>
#include <iterator>
#include <cstdlib>
#include <map>
#include <sstream>
#include <wincodec.h>
#include <shellapi.h>
#include <foobar2000/SDK/coreDarkMode.h>
#include <uxtheme.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace fs = std::filesystem;

DECLARE_COMPONENT_VERSION(
    "NowPlaying Copy & Artwork",
    "1.1.3",
    "Fixes help-window line breaks and enables readable wrapped text."
);
VALIDATE_COMPONENT_FILENAME("foo_nowplaying_copy.dll");

namespace {
    HBRUSH nowplaying_page_background_brush(bool dark) {
        static HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        return dark ? darkBrush : GetSysColorBrush(COLOR_WINDOW);
    }

    LRESULT paint_nowplaying_page_background(HWND wnd, HDC dc, bool dark) {
        RECT rc{};
        GetClientRect(wnd, &rc);
        FillRect(dc, &rc, nowplaying_page_background_brush(dark));
        return 1;
    }

    const GUID guid_nowplaying_command = { 0x523edcf3, 0x1e06, 0x42a5, { 0xaf, 0xe8, 0x1b, 0xa8, 0x1c, 0x53, 0x6f, 0x94 } };
    const GUID guid_nowplaying_history_command = { 0x9ee42bf4, 0xf088, 0x4db5, { 0x9e, 0x93, 0x6a, 0x50, 0x36, 0x0a, 0x17, 0x59 } };
    const GUID guid_nowplaying_help_command = { 0x9b54d6ac, 0x4f2d, 0x48a6, { 0xa7, 0x11, 0xb2, 0x30, 0x3b, 0x73, 0x0d, 0x26 } };

    const GUID guid_preferences_page = { 0x245c613a, 0xa545, 0x4b42, { 0xb1, 0x4f, 0xc3, 0xc8, 0x3f, 0x81, 0xce, 0x75 } };
    const GUID guid_cfg_post_format = { 0xfbc42113, 0x5fea, 0x4b4e, { 0xa1, 0xfd, 0x6b, 0x43, 0xf8, 0xf7, 0xf8, 0x66 } };
    const GUID guid_cfg_output_directory = { 0xa6198fe2, 0x237b, 0x4839, { 0x86, 0xb5, 0x78, 0xe7, 0x8d, 0xb9, 0x45, 0x31 } };
    const GUID guid_cfg_save_text = { 0xc1c79d80, 0x3642, 0x439a, { 0xb5, 0x43, 0x6c, 0xaa, 0x66, 0x04, 0x40, 0x52 } };
    const GUID guid_cfg_save_artwork = { 0x1eef54f6, 0xd5b6, 0x4dd5, { 0xa6, 0xf0, 0x1f, 0x7c, 0x0f, 0x9c, 0x3e, 0xc3 } };
    const GUID guid_cfg_search_zip = { 0xfcaacae5, 0xbe00, 0x4e3e, { 0x87, 0x4f, 0xb0, 0xb5, 0xd6, 0xb0, 0xc7, 0xba } };
    const GUID guid_cfg_show_completion = { 0x1a507bc1, 0xff32, 0x4d41, { 0xba, 0x4e, 0x2f, 0x09, 0x19, 0x58, 0x09, 0x8a } };
    const GUID guid_cfg_show_preview = { 0xd7607d4a, 0x9c5b, 0x42a6, { 0x94, 0x28, 0x63, 0xf5, 0x19, 0x1e, 0x16, 0x5d } };

    const GUID guid_cfg_optimize_artwork = { 0x7f6895e0, 0x6b9d, 0x43fc, { 0xa7, 0x5a, 0x4c, 0x66, 0xe1, 0x03, 0x41, 0x75 } };
    const GUID guid_cfg_artwork_post_max_size = { 0x2cbfdf24, 0x0122, 0x4b36, { 0xbd, 0x06, 0x2c, 0x98, 0x43, 0x66, 0xe6, 0x9a } };
    const GUID guid_cfg_artwork_post_format = { 0x216222f0, 0xd8e4, 0x4107, { 0x95, 0xf8, 0xef, 0x8f, 0xc8, 0xad, 0x0f, 0x99 } };
    const GUID guid_cfg_artwork_jpeg_quality = { 0x1d92cb35, 0xc0e7, 0x4cdf, { 0x83, 0xa6, 0xaa, 0x17, 0x21, 0x67, 0x62, 0x4f } };
    const GUID guid_cfg_artwork_square_mode = { 0xdb2854d1, 0xb748, 0x463d, { 0x9b, 0x67, 0x8f, 0xb4, 0x1a, 0x15, 0xb3, 0xd1 } };
    const GUID guid_cfg_history_limit = { 0x30ee8497, 0xc2cb, 0x495d, { 0x90, 0x04, 0x4c, 0x1a, 0x8d, 0x4c, 0x85, 0x57 } };
    const GUID guid_cfg_history_display_limit = { 0x73c7bc20, 0x18c6, 0x4b36, { 0xa1, 0x56, 0xc5, 0xf4, 0x42, 0x55, 0x6c, 0xb9 } };

    const GUID guid_cfg_template_name_1 = { 0xa8839664, 0x7235, 0x4ab2, { 0x84, 0x25, 0x13, 0xf0, 0xbe, 0x9c, 0xf1, 0x11 } };
    const GUID guid_cfg_template_name_2 = { 0x07139d21, 0xd0c6, 0x4c2e, { 0xb9, 0xf2, 0x32, 0x4c, 0xd4, 0x6d, 0xc3, 0x2d } };
    const GUID guid_cfg_template_name_3 = { 0x8067b3ed, 0xb9ed, 0x4b40, { 0xaa, 0xb5, 0x14, 0xf8, 0x11, 0xb3, 0x43, 0x75 } };
    const GUID guid_cfg_template_name_4 = { 0xb4f2b23b, 0x1693, 0x4737, { 0xa0, 0x61, 0x41, 0x4f, 0xe1, 0x40, 0x66, 0xc0 } };
    const GUID guid_cfg_template_name_5 = { 0x43e5e5ae, 0x3d6f, 0x4da1, { 0x90, 0xea, 0x7e, 0x0a, 0x51, 0xe7, 0xa2, 0x3e } };
    const GUID guid_cfg_template_format_2 = { 0xee60d7c7, 0x211d, 0x4a71, { 0xb6, 0xf2, 0xa8, 0xc8, 0x6b, 0xfe, 0x64, 0x7f } };
    const GUID guid_cfg_template_format_3 = { 0x7910d203, 0xc969, 0x407d, { 0x8c, 0xc6, 0x8a, 0xe8, 0x05, 0x0e, 0xd5, 0xca } };
    const GUID guid_cfg_template_format_4 = { 0x7ded8454, 0x582f, 0x4e86, { 0x86, 0x0f, 0x45, 0x2b, 0x94, 0xe1, 0x7b, 0xc4 } };
    const GUID guid_cfg_template_format_5 = { 0x8ee5c8d9, 0x8f00, 0x4650, { 0x99, 0x6b, 0xc5, 0x5a, 0x9c, 0x62, 0x47, 0x82 } };
    const GUID guid_cfg_selected_template = { 0x0bc890ac, 0xd0a6, 0x4312, { 0xb9, 0xb8, 0x99, 0x12, 0xb7, 0x3e, 0xd9, 0xd4 } };
    const GUID guid_cfg_template_schema_version = { 0x6844de34, 0xe328, 0x46a1, { 0x98, 0xf7, 0x52, 0x5a, 0xc4, 0x0c, 0x3c, 0x4a } };
    const GUID guid_cfg_template_collection = { 0x9a4a3017, 0x46c6, 0x4ed1, { 0x8f, 0x2a, 0x7c, 0x74, 0x8c, 0x31, 0x11, 0x42 } };

    constexpr size_t template_min_count = 1;
    constexpr size_t template_default_count = 5;
    constexpr size_t template_max_count = 20;
    constexpr const char* default_post_format = "#NowPlaying %title% - %artist%";

    static cfg_string g_cfg_post_format(guid_cfg_post_format, default_post_format);
    static cfg_string g_cfg_output_directory(guid_cfg_output_directory, "");
    static cfg_bool g_cfg_save_text(guid_cfg_save_text, true);
    static cfg_bool g_cfg_save_artwork(guid_cfg_save_artwork, true);
    static cfg_bool g_cfg_search_zip(guid_cfg_search_zip, true);
    static cfg_bool g_cfg_show_completion(guid_cfg_show_completion, true);
    static cfg_bool g_cfg_show_preview(guid_cfg_show_preview, true);

    static cfg_bool g_cfg_optimize_artwork(guid_cfg_optimize_artwork, true);
    static cfg_string g_cfg_artwork_post_max_size(guid_cfg_artwork_post_max_size, "2048");
    static cfg_string g_cfg_artwork_post_format(guid_cfg_artwork_post_format, "jpeg");
    static cfg_string g_cfg_artwork_jpeg_quality(guid_cfg_artwork_jpeg_quality, "90");
    static cfg_string g_cfg_artwork_square_mode(guid_cfg_artwork_square_mode, "0");
    static cfg_string g_cfg_history_limit(guid_cfg_history_limit, "100");
    static cfg_string g_cfg_history_display_limit(guid_cfg_history_display_limit, "100");

    static cfg_string g_cfg_template_name_1(guid_cfg_template_name_1, "シンプル");
    static cfg_string g_cfg_template_name_2(guid_cfg_template_name_2, "改行タイプ");
    static cfg_string g_cfg_template_name_3(guid_cfg_template_name_3, "アルバム情報");
    static cfg_string g_cfg_template_name_4(guid_cfg_template_name_4, "引用タイプ");
    static cfg_string g_cfg_template_name_5(guid_cfg_template_name_5, "自由設定");
    static cfg_string g_cfg_template_format_2(guid_cfg_template_format_2, "♪ %title%\r\n%artist%\r\n\r\n#NowPlaying");
    static cfg_string g_cfg_template_format_3(guid_cfg_template_format_3, "🎧 %title%\r\nArtist：%artist%\r\nAlbum：%album%\r\n\r\n#NowPlaying");
    static cfg_string g_cfg_template_format_4(guid_cfg_template_format_4, "%artist%「%title%」を再生中🎧\r\n\r\n#NowPlaying");
    static cfg_string g_cfg_template_format_5(guid_cfg_template_format_5, default_post_format);
    static cfg_string g_cfg_selected_template(guid_cfg_selected_template, "0");
    static cfg_string g_cfg_template_schema_version(guid_cfg_template_schema_version, "");
    static cfg_string g_cfg_template_collection(guid_cfg_template_collection, "");

    std::wstring utf8_to_wide(const char* value) {
        if (value == nullptr || *value == 0) return {};
        const int count = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
        if (count <= 1) return {};
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value, -1, result.data(), count);
        result.resize(static_cast<size_t>(count - 1));
        return result;
    }

    std::string wide_to_utf8(const std::wstring& value) {
        if (value.empty()) return {};
        const int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1) return {};
        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), count, nullptr, nullptr);
        result.resize(static_cast<size_t>(count - 1));
        return result;
    }


    struct artwork_post_settings {
        bool enabled = true;
        UINT maxSize = 2048;
        bool png = false;
        UINT jpegQuality = 90;
        int squareMode = 0; // 0 = keep aspect, 1 = crop center square
    };

    struct artwork_dimensions {
        UINT width = 0;
        UINT height = 0;
    };

    UINT parse_uint_or_default(const pfc::string8& textValue, UINT fallbackValue, UINT minValue, UINT maxValue) {
        try {
            const unsigned long parsed = std::stoul(std::string(textValue.c_str()));
            if (parsed < minValue) return minValue;
            if (parsed > maxValue) return maxValue;
            return static_cast<UINT>(parsed);
        } catch (...) {
            return fallbackValue;
        }
    }

    UINT parse_uint_text_or_default(
        const std::wstring& textValue,
        UINT fallbackValue,
        UINT minValue,
        UINT maxValue
    ) {
        try {
            const unsigned long parsed = std::stoul(textValue);
            if (parsed < minValue) return minValue;
            if (parsed > maxValue) return maxValue;
            return static_cast<UINT>(parsed);
        } catch (...) {
            return fallbackValue;
        }
    }

    int parse_int_or_default(const pfc::string8& textValue, int fallbackValue, int minValue, int maxValue) {
        try {
            const int parsed = std::stoi(std::string(textValue.c_str()));
            if (parsed < minValue) return minValue;
            if (parsed > maxValue) return maxValue;
            return parsed;
        } catch (...) {
            return fallbackValue;
        }
    }

    artwork_post_settings configured_artwork_post_settings() {
        artwork_post_settings settings;
        settings.enabled = g_cfg_optimize_artwork.get();
        settings.maxSize = parse_uint_or_default(g_cfg_artwork_post_max_size.get(), 2048, 256, 4096);
        const std::wstring format = utf8_to_wide(g_cfg_artwork_post_format.get().c_str());
        settings.png = _wcsicmp(format.c_str(), L"png") == 0;
        settings.jpegQuality = parse_uint_or_default(g_cfg_artwork_jpeg_quality.get(), 90, 40, 100);
        settings.squareMode = parse_int_or_default(g_cfg_artwork_square_mode.get(), 0, 0, 1);
        return settings;
    }

    const wchar_t* artwork_square_mode_label(int mode) {
        return mode == 1 ? L"中央を正方形に切り抜く" : L"そのまま縮小";
    }

    std::wstring format_file_size(uintmax_t bytes) {
        if (bytes >= 1024 * 1024) {
            const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
            wchar_t buffer[64]{};
            swprintf_s(buffer, L"%.2f MB", mb);
            return buffer;
        }
        if (bytes >= 1024) {
            const double kb = static_cast<double>(bytes) / 1024.0;
            wchar_t buffer[64]{};
            swprintf_s(buffer, L"%.1f KB", kb);
            return buffer;
        }
        return std::to_wstring(bytes) + L" B";
    }

    bool read_image_dimensions(const fs::path& path, artwork_dimensions& dims) {
        dims = {};
        HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool shouldUninitialize = initializeResult == S_OK || initializeResult == S_FALSE;
        IWICImagingFactory* factory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr)) {
            hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        }
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) hr = frame->GetSize(&dims.width, &dims.height);
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (factory) factory->Release();
        if (shouldUninitialize) CoUninitialize();
        return SUCCEEDED(hr) && dims.width > 0 && dims.height > 0;
    }

    std::wstring describe_artwork_file(
        const fs::path& path,
        const wchar_t* label
    ) {
        std::wstring result = label;
        result += L"：";

        artwork_dimensions dims;
        if (read_image_dimensions(path, dims)) {
            result += std::to_wstring(dims.width);
            result += L" × ";
            result += std::to_wstring(dims.height);
        } else {
            result += L"解像度不明";
        }

        std::error_code ec;
        const uintmax_t bytes = fs::file_size(path, ec);
        if (!ec) {
            result += L" / ";
            result += format_file_size(bytes);
        }
        return result;
    }

    HBRUSH artwork_background_brush(bool dark) {
        static HBRUSH lightBrush = CreateSolidBrush(RGB(255, 255, 255));
        static HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        return dark ? darkBrush : lightBrush;
    }

    COLORREF artwork_placeholder_text_color(bool dark) {
        return dark ? RGB(175, 175, 175) : GetSysColor(COLOR_GRAYTEXT);
    }

    const char* default_template_name(size_t index) {
        switch (index) {
        case 0: return "シンプル";
        case 1: return "改行タイプ";
        case 2: return "アルバム情報";
        case 3: return "引用タイプ";
        default: return "自由設定";
        }
    }

    const char* default_template_format(size_t index) {
        switch (index) {
        case 0: return default_post_format;
        case 1: return "♪ %title%\r\n%artist%\r\n\r\n#NowPlaying";
        case 2: return "🎧 %title%\r\nArtist：%artist%\r\nAlbum：%album%\r\n\r\n#NowPlaying";
        case 3: return "%artist%「%title%」を再生中🎧\r\n\r\n#NowPlaying";
        default: return default_post_format;
        }
    }

    void repair_legacy_template_settings() {
        const pfc::string8 schema = g_cfg_template_schema_version.get();
        if (std::strcmp(schema.c_str(), "2") == 0) return;

        // v0.6.0 could overwrite templates 2 and 3 with the simple format
        // while the user changed selections in the preferences page.
        // Repair only values matching that known corrupted state, preserving
        // any other user-customized formats.
        const pfc::string8 format2 = g_cfg_template_format_2.get();
        if (format2.is_empty() ||
            std::strcmp(format2.c_str(), default_post_format) == 0) {
            g_cfg_template_format_2 = default_template_format(1);
        }

        const pfc::string8 format3 = g_cfg_template_format_3.get();
        if (format3.is_empty() ||
            std::strcmp(format3.c_str(), default_post_format) == 0) {
            g_cfg_template_format_3 = default_template_format(2);
        }

        // Template 4 was normally unaffected, but restore it if empty.
        const pfc::string8 format4 = g_cfg_template_format_4.get();
        if (format4.is_empty()) {
            g_cfg_template_format_4 = default_template_format(3);
        }

        // Template 5 intentionally begins with the simple format.
        const pfc::string8 format5 = g_cfg_template_format_5.get();
        if (format5.is_empty()) {
            g_cfg_template_format_5 = default_template_format(4);
        }

        g_cfg_template_schema_version = "2";
    }

    struct post_template_definition {
        std::wstring name;
        std::wstring format;

        bool operator==(const post_template_definition& other) const {
            return name == other.name && format == other.format;
        }
    };

    int template_hex_value(char value) {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    }

    std::string template_hex_encode(const std::wstring& value) {
        static const char hex[] = "0123456789ABCDEF";
        const std::string utf8 = wide_to_utf8(value);
        std::string encoded;
        encoded.reserve(utf8.size() * 2);
        for (unsigned char byte : utf8) {
            encoded.push_back(hex[(byte >> 4) & 0x0F]);
            encoded.push_back(hex[byte & 0x0F]);
        }
        return encoded;
    }

    bool template_hex_decode(
        const std::string& value,
        std::wstring& decoded
    ) {
        decoded.clear();
        if ((value.size() % 2) != 0) return false;
        std::string utf8;
        utf8.reserve(value.size() / 2);
        for (size_t index = 0; index < value.size(); index += 2) {
            const int high = template_hex_value(value[index]);
            const int low = template_hex_value(value[index + 1]);
            if (high < 0 || low < 0) return false;
            utf8.push_back(static_cast<char>((high << 4) | low));
        }
        decoded = utf8_to_wide(utf8.c_str());
        return true;
    }

    std::vector<post_template_definition> default_template_definitions() {
        std::vector<post_template_definition> templates;
        templates.reserve(template_default_count);
        for (size_t index = 0; index < template_default_count; ++index) {
            templates.push_back({
                utf8_to_wide(default_template_name(index)),
                utf8_to_wide(default_template_format(index))
            });
        }
        return templates;
    }

    std::vector<post_template_definition> legacy_template_definitions() {
        repair_legacy_template_settings();
        std::vector<post_template_definition> templates;
        templates.reserve(template_default_count);

        const pfc::string8 names[] = {
            g_cfg_template_name_1.get(),
            g_cfg_template_name_2.get(),
            g_cfg_template_name_3.get(),
            g_cfg_template_name_4.get(),
            g_cfg_template_name_5.get()
        };
        const pfc::string8 formats[] = {
            g_cfg_post_format.get(),
            g_cfg_template_format_2.get(),
            g_cfg_template_format_3.get(),
            g_cfg_template_format_4.get(),
            g_cfg_template_format_5.get()
        };

        for (size_t index = 0; index < template_default_count; ++index) {
            std::wstring name = utf8_to_wide(names[index].c_str());
            std::wstring format = utf8_to_wide(formats[index].c_str());
            if (name.empty()) name = utf8_to_wide(default_template_name(index));
            if (format.empty()) format = utf8_to_wide(default_template_format(index));
            templates.push_back({std::move(name), std::move(format)});
        }
        return templates;
    }

    std::string serialize_template_definitions(
        const std::vector<post_template_definition>& templates
    ) {
        std::ostringstream stream;
        stream << "NPCAT1\n";
        for (const auto& item : templates) {
            stream << template_hex_encode(item.name)
                << '\t'
                << template_hex_encode(item.format)
                << '\n';
        }
        return stream.str();
    }

    bool deserialize_template_definitions(
        const std::string& serialized,
        std::vector<post_template_definition>& templates
    ) {
        templates.clear();
        std::istringstream stream(serialized);
        std::string header;
        if (!std::getline(stream, header) || header != "NPCAT1") {
            return false;
        }

        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (templates.size() >= template_max_count) return false;

            const size_t tab = line.find('\t');
            if (tab == std::string::npos) return false;

            post_template_definition item;
            if (!template_hex_decode(line.substr(0, tab), item.name) ||
                !template_hex_decode(line.substr(tab + 1), item.format)) {
                return false;
            }
            if (item.name.empty()) {
                item.name = L"テンプレート " +
                    std::to_wstring(templates.size() + 1);
            }
            if (item.format.empty()) {
                item.format = utf8_to_wide(default_post_format);
            }
            templates.push_back(std::move(item));
        }

        return templates.size() >= template_min_count &&
            templates.size() <= template_max_count;
    }

    void sync_legacy_template_settings(
        const std::vector<post_template_definition>& templates
    ) {
        const auto defaults = default_template_definitions();
        for (size_t index = 0; index < template_default_count; ++index) {
            const post_template_definition& item =
                index < templates.size() ? templates[index] : defaults[index];
            const std::string name = wide_to_utf8(item.name);
            const std::string format = wide_to_utf8(item.format);
            switch (index) {
            case 0:
                g_cfg_template_name_1 = name.c_str();
                g_cfg_post_format = format.c_str();
                break;
            case 1:
                g_cfg_template_name_2 = name.c_str();
                g_cfg_template_format_2 = format.c_str();
                break;
            case 2:
                g_cfg_template_name_3 = name.c_str();
                g_cfg_template_format_3 = format.c_str();
                break;
            case 3:
                g_cfg_template_name_4 = name.c_str();
                g_cfg_template_format_4 = format.c_str();
                break;
            case 4:
                g_cfg_template_name_5 = name.c_str();
                g_cfg_template_format_5 = format.c_str();
                break;
            }
        }
    }

    void set_configured_template_definitions(
        std::vector<post_template_definition> templates
    ) {
        if (templates.empty()) {
            templates = default_template_definitions();
        }
        if (templates.size() > template_max_count) {
            templates.resize(template_max_count);
        }
        for (size_t index = 0; index < templates.size(); ++index) {
            if (templates[index].name.empty()) {
                templates[index].name = L"テンプレート " +
                    std::to_wstring(index + 1);
            }
            if (templates[index].format.empty()) {
                templates[index].format = utf8_to_wide(default_post_format);
            }
        }

        const std::string serialized =
            serialize_template_definitions(templates);
        g_cfg_template_collection = serialized.c_str();
        sync_legacy_template_settings(templates);
        g_cfg_template_schema_version = "3";
    }

    std::vector<post_template_definition> configured_template_definitions() {
        std::vector<post_template_definition> templates;
        const pfc::string8 stored = g_cfg_template_collection.get();
        if (!stored.is_empty() &&
            deserialize_template_definitions(stored.c_str(), templates)) {
            return templates;
        }

        templates = legacy_template_definitions();
        set_configured_template_definitions(templates);
        return templates;
    }

    pfc::string8 configured_template_name(size_t index) {
        const auto templates = configured_template_definitions();
        pfc::string8 value;
        if (index < templates.size()) {
            const std::string utf8 = wide_to_utf8(templates[index].name);
            value = utf8.c_str();
        }
        return value;
    }

    pfc::string8 configured_template_format(size_t index) {
        const auto templates = configured_template_definitions();
        pfc::string8 value;
        if (index < templates.size()) {
            const std::string utf8 = wide_to_utf8(templates[index].format);
            value = utf8.c_str();
        }
        return value;
    }

    int selected_template_index() {
        const auto templates = configured_template_definitions();
        const pfc::string8 stored = g_cfg_selected_template.get();
        int index = std::atoi(stored.c_str());
        if (index < 0 || index >= static_cast<int>(templates.size())) index = 0;
        return index;
    }

    void set_selected_template_index(int index) {
        const auto templates = configured_template_definitions();
        if (index < 0 || index >= static_cast<int>(templates.size())) index = 0;
        const std::string value = std::to_string(index);
        g_cfg_selected_template = value.c_str();
    }

    std::wstring format_track_message(
        const metadb_handle_ptr& track,
        const std::wstring& formatText
    ) {
        std::wstring effectiveFormat = formatText;
        if (effectiveFormat.empty()) {
            effectiveFormat = utf8_to_wide(default_post_format);
        }

        // The title-format compiler does not reliably preserve literal
        // CR/LF characters inside a format string. Format each line
        // separately, then restore the original line breaks afterwards.
        std::wstring result;
        size_t position = 0;

        while (true) {
            const size_t lineEnd =
                effectiveFormat.find_first_of(L"\r\n", position);
            const std::wstring line =
                lineEnd == std::wstring::npos
                    ? effectiveFormat.substr(position)
                    : effectiveFormat.substr(position, lineEnd - position);

            const std::string lineUtf8 = wide_to_utf8(line);
            if (!lineUtf8.empty()) {
                titleformat_object::ptr formatter;
                static_api_ptr_t<titleformat_compiler>()->compile_safe(
                    formatter, lineUtf8.c_str());

                pfc::string8 formatted;
                track->format_title(nullptr, formatted, formatter, nullptr);
                result += utf8_to_wide(formatted.c_str());
            }

            if (lineEnd == std::wstring::npos) break;

            result += L"\r\n";

            if (effectiveFormat[lineEnd] == L'\r' &&
                lineEnd + 1 < effectiveFormat.size() &&
                effectiveFormat[lineEnd + 1] == L'\n') {
                position = lineEnd + 2;
            } else {
                position = lineEnd + 1;
            }
        }

        return result;
    }

    std::wstring normalize_windows_newlines(
        const std::wstring& text
    ) {
        std::wstring normalized;
        normalized.reserve(text.size() + text.size() / 8);

        for (size_t index = 0; index < text.size(); ++index) {
            const wchar_t ch = text[index];
            if (ch == L'\r') {
                normalized.push_back(L'\r');
                if (index + 1 < text.size() &&
                    text[index + 1] == L'\n') {
                    normalized.push_back(L'\n');
                    ++index;
                } else {
                    normalized.push_back(L'\n');
                }
            } else if (ch == L'\n') {
                normalized.push_back(L'\r');
                normalized.push_back(L'\n');
            } else {
                normalized.push_back(ch);
            }
        }
        return normalized;
    }

    bool copy_text_to_clipboard(const std::wstring& text) {
        if (!OpenClipboard(core_api::get_main_window())) return false;
        EmptyClipboard();
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory == nullptr) {
            CloseClipboard();
            return false;
        }
        void* target = GlobalLock(memory);
        if (target == nullptr) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        memcpy(target, text.c_str(), bytes);
        GlobalUnlock(memory);
        if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    fs::path default_output_directory() {
        PWSTR raw = nullptr;
        fs::path base;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &raw)) && raw != nullptr) {
            base = raw;
            CoTaskMemFree(raw);
        } else {
            base = fs::temp_directory_path();
        }
        return base / L"foobar2000 NowPlaying";
    }

    fs::path output_directory() {
        const pfc::string8 configured = g_cfg_output_directory.get();
        fs::path out;
        if (configured.is_empty()) {
            out = default_output_directory();
        } else {
            out = fs::path(utf8_to_wide(configured.c_str()));
        }

        std::error_code ec;
        fs::create_directories(out, ec);
        return out;
    }

    bool write_utf8_text(const fs::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return file.good();
    }

    std::wstring lower(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return value;
    }

    bool starts_with_ci(const std::wstring& value, const std::wstring& prefix) {
        if (value.size() < prefix.size()) return false;
        return lower(value.substr(0, prefix.size())) == lower(prefix);
    }

    bool is_cover_name(const fs::path& p) {
        const std::wstring stem = lower(p.stem().wstring());
        const std::wstring ext = lower(p.extension().wstring());
        const bool validStem = stem == L"cover" || stem == L"folder" || stem == L"front";
        const bool validExt = ext == L".jpg" || ext == L".jpeg" || ext == L".png";
        return validStem && validExt;
    }

    int cover_rank(const fs::path& p) {
        const std::wstring stem = lower(p.stem().wstring());
        const std::wstring ext = lower(p.extension().wstring());
        int rank = stem == L"cover" ? 0 : stem == L"folder" ? 10 : 20;
        rank += ext == L".jpg" ? 0 : ext == L".jpeg" ? 1 : 2;
        return rank;
    }

    void remove_old_artwork(const fs::path& outDir) {
        std::error_code ec;
        for (const wchar_t* ext : { L".jpg", L".jpeg", L".png", L".gif", L".webp", L".bmp", L".tif", L".tiff", L".bin" }) {
            fs::path candidate = outDir / L"artwork";
            candidate += ext;
            fs::remove(candidate, ec);
            ec.clear();
        }
    }


    std::wstring detect_image_extension(const void* data, size_t size) {
        if (data == nullptr || size == 0) return L".bin";
        const auto* bytes = static_cast<const unsigned char*>(data);

        if (size >= 3 &&
            bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
            return L".jpg";
        }
        if (size >= 8 &&
            bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
            bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
            return L".png";
        }
        if (size >= 6 &&
            bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' &&
            bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
            return L".gif";
        }
        if (size >= 12 &&
            bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
            bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
            return L".webp";
        }
        if (size >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
            return L".bmp";
        }
        if (size >= 4 &&
            ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 0x2A && bytes[3] == 0x00) ||
             (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0x00 && bytes[3] == 0x2A))) {
            return L".tif";
        }
        return L".bin";
    }

    bool write_binary_file(const fs::path& path, const void* data, size_t size) {
        if (data == nullptr || size == 0) return false;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.good();
    }

    bool export_embedded_cover(
        const metadb_handle_ptr& track,
        const fs::path& outDir,
        fs::path& exported,
        std::wstring& errorText
    ) {
        errorText.clear();
        try {
            abort_callback_dummy abort;
            album_art_extractor_instance_ptr extractor =
                album_art_extractor::g_open(file_ptr(), track->get_path(), abort);

            album_art_data_ptr data =
                extractor->query(album_art_ids::cover_front, abort);

            if (data.is_empty() || data->get_ptr() == nullptr || data->get_size() == 0) {
                errorText = L"埋め込みフロントカバーのデータが空でした。";
                return false;
            }

            const std::wstring extension =
                detect_image_extension(data->get_ptr(), data->get_size());

            remove_old_artwork(outDir);
            exported = outDir / (L"artwork" + extension);

            if (!write_binary_file(
                    exported,
                    data->get_ptr(),
                    static_cast<size_t>(data->get_size()))) {
                errorText = L"埋め込み画像をファイルへ保存できませんでした。";
                return false;
            }

            return true;
        }
        catch (const exception_album_art_not_found&) {
            errorText = L"埋め込みフロントカバーはありません。";
        }
        catch (const std::exception& e) {
            errorText = L"埋め込み画像の取得エラー：";
            errorText += utf8_to_wide(e.what());
        }
        catch (...) {
            errorText = L"埋め込み画像の取得中に不明なエラーが発生しました。";
        }
        return false;
    }

    bool export_folder_cover(const fs::path& audioPath, const fs::path& outDir, fs::path& exported) {
        std::error_code ec;
        const fs::path dir = audioPath.parent_path();
        if (!fs::is_directory(dir, ec)) return false;
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec) && is_cover_name(entry.path())) found.push_back(entry.path());
        }
        if (found.empty()) return false;
        std::sort(found.begin(), found.end(),
            [](const fs::path& a, const fs::path& b) { return cover_rank(a) < cover_rank(b); });
        exported = outDir / (L"artwork" + lower(found.front().extension().wstring()));
        fs::copy_file(found.front(), exported, fs::copy_options::overwrite_existing, ec);
        return !ec;
    }

    std::wstring quote_command_argument(const std::wstring& value) {
        std::wstring result = L"\"";
        size_t backslashes = 0;
        for (wchar_t c : value) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'"') {
                result.append(backslashes * 2 + 1, L'\\');
                result.push_back(L'"');
                backslashes = 0;
            } else {
                result.append(backslashes, L'\\');
                backslashes = 0;
                result.push_back(c);
            }
        }
        result.append(backslashes * 2, L'\\');
        result.push_back(L'"');
        return result;
    }

    bool run_hidden_and_wait(
        const std::wstring& commandLine,
        DWORD& exitCode,
        DWORD timeoutMilliseconds = 30000
    ) {
        exitCode = static_cast<DWORD>(-1);
        std::vector<wchar_t> writable(commandLine.begin(), commandLine.end());
        writable.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, writable.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            exitCode = GetLastError();
            return false;
        }

        const DWORD waitResult = WaitForSingleObject(
            pi.hProcess,
            timeoutMilliseconds);
        if (waitResult == WAIT_OBJECT_0) {
            GetExitCodeProcess(pi.hProcess, &exitCode);
        } else {
            exitCode = waitResult;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return waitResult == WAIT_OBJECT_0 && exitCode == 0;
    }

    fs::path create_unique_temp_directory(
        const wchar_t* prefix
    ) {
        std::error_code ec;
        const fs::path base = fs::temp_directory_path(ec);
        if (ec) return {};

        wchar_t temporaryFile[MAX_PATH]{};
        const std::wstring prefixText =
            prefix == nullptr ? L"npc" : prefix;
        wchar_t shortPrefix[4]{
            L'n',
            L'p',
            L'c',
            L'\0'
        };
        for (size_t index = 0;
             index < 3 && index < prefixText.size();
             ++index) {
            shortPrefix[index] = prefixText[index];
        }

        if (GetTempFileNameW(
                base.c_str(),
                shortPrefix,
                0,
                temporaryFile) == 0) {
            return {};
        }

        DeleteFileW(temporaryFile);
        const fs::path directory = temporaryFile;
        fs::create_directories(directory, ec);
        if (ec) return {};
        return directory;
    }

    bool create_zip_from_directory(
        const fs::path& sourceDirectory,
        const fs::path& destinationZip,
        DWORD& processExitCode
    ) {
        processExitCode = static_cast<DWORD>(-1);
        std::error_code ec;
        if (!fs::is_directory(sourceDirectory, ec)) {
            processExitCode = ERROR_PATH_NOT_FOUND;
            return false;
        }

        const fs::path scriptDirectory =
            create_unique_temp_directory(L"npz");
        if (scriptDirectory.empty()) {
            processExitCode = ERROR_CANNOT_MAKE;
            return false;
        }

        const fs::path scriptPath =
            scriptDirectory / L"create-history-backup.ps1";
        const std::wstring script =
            L"param([string]$source,[string]$destination)\r\n"
            L"$ErrorActionPreference='Stop'\r\n"
            L"Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
            L"if([IO.File]::Exists($destination)){[IO.File]::Delete($destination)}\r\n"
            L"[IO.Compression.ZipFile]::CreateFromDirectory("
                L"$source,$destination,"
                L"[IO.Compression.CompressionLevel]::Optimal,$false)\r\n";

        if (!write_utf8_text(
                scriptPath,
                wide_to_utf8(script))) {
            fs::remove_all(scriptDirectory, ec);
            processExitCode = ERROR_WRITE_FAULT;
            return false;
        }

        const std::wstring command =
            L"powershell.exe -NoLogo -NoProfile -NonInteractive "
            L"-ExecutionPolicy Bypass -File " +
            quote_command_argument(scriptPath.wstring()) +
            L" -source " +
            quote_command_argument(sourceDirectory.wstring()) +
            L" -destination " +
            quote_command_argument(destinationZip.wstring());

        const bool ok = run_hidden_and_wait(
            command,
            processExitCode,
            120000);
        fs::remove_all(scriptDirectory, ec);
        return ok;
    }

    bool extract_zip_to_directory(
        const fs::path& sourceZip,
        const fs::path& destinationDirectory,
        DWORD& processExitCode
    ) {
        processExitCode = static_cast<DWORD>(-1);
        std::error_code ec;
        if (!fs::is_regular_file(sourceZip, ec)) {
            processExitCode = ERROR_FILE_NOT_FOUND;
            return false;
        }

        const fs::path scriptDirectory =
            create_unique_temp_directory(L"npx");
        if (scriptDirectory.empty()) {
            processExitCode = ERROR_CANNOT_MAKE;
            return false;
        }

        const fs::path scriptPath =
            scriptDirectory / L"extract-history-backup.ps1";
        const std::wstring script =
            L"param([string]$zip,[string]$destination)\r\n"
            L"$ErrorActionPreference='Stop'\r\n"
            L"Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
            L"if([IO.Directory]::Exists($destination)){"
                L"[IO.Directory]::Delete($destination,$true)}\r\n"
            L"[IO.Directory]::CreateDirectory($destination)|Out-Null\r\n"
            L"$root=[IO.Path]::GetFullPath($destination+[IO.Path]::DirectorySeparatorChar)\r\n"
            L"$archive=[IO.Compression.ZipFile]::OpenRead($zip)\r\n"
            L"try {\r\n"
            L" $entries=@($archive.Entries)\r\n"
            L" if($entries.Count -gt 5000){throw 'Too many entries'}\r\n"
            L" [Int64]$total=0\r\n"
            L" foreach($entry in $entries){$total+=$entry.Length}\r\n"
            L" if($total -gt 2147483648){throw 'Backup is too large'}\r\n"
            L" foreach($entry in $entries){\r\n"
            L"  $target=[IO.Path]::GetFullPath([IO.Path]::Combine($root,$entry.FullName))\r\n"
            L"  if(-not $target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){"
                L"throw 'Unsafe path'}\r\n"
            L"  if([string]::IsNullOrEmpty($entry.Name)){"
                L"[IO.Directory]::CreateDirectory($target)|Out-Null;continue}\r\n"
            L"  $parent=[IO.Path]::GetDirectoryName($target)\r\n"
            L"  if(-not [string]::IsNullOrEmpty($parent)){"
                L"[IO.Directory]::CreateDirectory($parent)|Out-Null}\r\n"
            L"  [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)\r\n"
            L" }\r\n"
            L"} finally {$archive.Dispose()}\r\n";

        if (!write_utf8_text(
                scriptPath,
                wide_to_utf8(script))) {
            fs::remove_all(scriptDirectory, ec);
            processExitCode = ERROR_WRITE_FAULT;
            return false;
        }

        const std::wstring command =
            L"powershell.exe -NoLogo -NoProfile -NonInteractive "
            L"-ExecutionPolicy Bypass -File " +
            quote_command_argument(scriptPath.wstring()) +
            L" -zip " +
            quote_command_argument(sourceZip.wstring()) +
            L" -destination " +
            quote_command_argument(destinationDirectory.wstring());

        const bool ok = run_hidden_and_wait(
            command,
            processExitCode,
            120000);
        fs::remove_all(scriptDirectory, ec);
        return ok;
    }

    bool validate_history_backup_manifest(
        const fs::path& manifestPath,
        std::wstring& errorText
    ) {
        errorText.clear();
        std::ifstream file(manifestPath, std::ios::binary);
        if (!file) {
            errorText =
                L"バックアップ情報ファイルが見つかりません。";
            return false;
        }

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        if (content.size() > 65536) {
            errorText =
                L"バックアップ情報ファイルのサイズが不正です。";
            return false;
        }
        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        if (content.find(
                "FileType=NowPlaying Copy & Artwork History Backup") ==
                std::string::npos) {
            errorText =
                L"NowPlaying投稿履歴のバックアップではありません。";
            return false;
        }
        if (content.find("Version=1") == std::string::npos) {
            errorText =
                L"対応していない履歴バックアップのバージョンです。";
            return false;
        }
        return true;
    }

    bool safe_history_backup_relative_path(
        const fs::path& path
    ) {
        if (path.empty() ||
            path.is_absolute() ||
            path.has_root_name() ||
            path.has_root_directory()) {
            return false;
        }

        const fs::path normalized =
            path.lexically_normal();
        for (const auto& part : normalized) {
            if (part == L"..") return false;
        }

        const auto first = normalized.begin();
        return first != normalized.end() &&
            lower(first->wstring()) == L"artwork";
    }

    std::wstring normalize_local_file_path(std::wstring value) {
        const std::wstring lowered = lower(value);
        const size_t fileScheme = lowered.rfind(L"file://");
        if (fileScheme != std::wstring::npos) {
            value = value.substr(fileScheme + 7);
        } else {
            const size_t lastPipe = value.rfind(L'|');
            if (lastPipe != std::wstring::npos) value = value.substr(lastPipe + 1);
        }

        while (value.size() >= 3 && value.front() == L'/' &&
               ((value[1] >= L'A' && value[1] <= L'Z') ||
                (value[1] >= L'a' && value[1] <= L'z')) &&
               value[2] == L':') {
            value.erase(value.begin());
        }

        std::replace(value.begin(), value.end(), L'/', L'\\');
        return value;
    }

    bool parse_zip_location(const std::wstring& rawPath, fs::path& zipPath, std::wstring& innerPath) {
        const std::wstring lowered = lower(rawPath);
        const size_t marker = lowered.rfind(L".zip|");
        if (marker == std::wstring::npos) return false;

        std::wstring archivePart = rawPath.substr(0, marker + 4);
        innerPath = rawPath.substr(marker + 5);

        archivePart = normalize_local_file_path(archivePart);
        while (!innerPath.empty() && (innerPath.front() == L'\\' || innerPath.front() == L'/')) {
            innerPath.erase(innerPath.begin());
        }
        std::replace(innerPath.begin(), innerPath.end(), L'\\', L'/');

        zipPath = fs::path(archivePart);
        return !archivePart.empty() && !innerPath.empty();
    }

    bool export_zip_cover(
        const fs::path& zipPath,
        const std::wstring& innerAudioPath,
        const fs::path& outDir,
        fs::path& exported,
        DWORD& processExitCode
    ) {
        std::error_code ec;
        processExitCode = static_cast<DWORD>(-1);
        if (!fs::is_regular_file(zipPath, ec)) {
            processExitCode = ERROR_FILE_NOT_FOUND;
            return false;
        }

        remove_old_artwork(outDir);

        const fs::path scriptPath = fs::temp_directory_path() / L"fb2k_nowplaying_extract.ps1";
        const fs::path resultBase = outDir / L"artwork";

        const std::wstring script =
            L"param([string]$zip,[string]$inner,[string]$outbase)\r\n"
            L"$ErrorActionPreference='Stop'\r\n"
            L"Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
            L"$z=[IO.Compression.ZipFile]::OpenRead($zip)\r\n"
            L"try {\r\n"
            L" $inner=$inner.Replace('\\','/')\r\n"
            L" $dir=[IO.Path]::GetDirectoryName($inner)\r\n"
            L" if($null -eq $dir){$dir=''} else {$dir=$dir.Replace('\\','/')}\r\n"
            L" $names=@('cover.jpg','cover.jpeg','cover.png','folder.jpg','folder.jpeg','folder.png','front.jpg','front.jpeg','front.png')\r\n"
            L" $entries=@($z.Entries | Where-Object { $names -contains ([IO.Path]::GetFileName($_.FullName).ToLowerInvariant()) })\r\n"
            L" $pick=$entries | Where-Object { $d=[IO.Path]::GetDirectoryName($_.FullName); if($null -eq $d){$d=''}; $d.Replace('\\','/') -eq $dir } | Sort-Object { [array]::IndexOf($names,[IO.Path]::GetFileName($_.FullName).ToLowerInvariant()) } | Select-Object -First 1\r\n"
            L" if(-not $pick){$pick=$entries | Where-Object { $d=[IO.Path]::GetDirectoryName($_.FullName); [string]::IsNullOrEmpty($d) } | Sort-Object { [array]::IndexOf($names,[IO.Path]::GetFileName($_.FullName).ToLowerInvariant()) } | Select-Object -First 1}\r\n"
            L" if(-not $pick){$pick=$entries | Sort-Object { [array]::IndexOf($names,[IO.Path]::GetFileName($_.FullName).ToLowerInvariant()) } | Select-Object -First 1}\r\n"
            L" if(-not $pick){exit 2}\r\n"
            L" $ext=[IO.Path]::GetExtension($pick.Name).ToLowerInvariant()\r\n"
            L" $dest=$outbase+$ext\r\n"
            L" $src=$pick.Open()\r\n"
            L" try {\r\n"
            L"  $dst=[IO.File]::Create($dest)\r\n"
            L"  try {$src.CopyTo($dst)} finally {$dst.Dispose()}\r\n"
            L" } finally {$src.Dispose()}\r\n"
            L" exit 0\r\n"
            L"} finally {$z.Dispose()}\r\n";

        if (!write_utf8_text(scriptPath, wide_to_utf8(script))) {
            processExitCode = ERROR_WRITE_FAULT;
            return false;
        }

        const std::wstring command =
            L"powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
            quote_command_argument(scriptPath.wstring()) +
            L" -zip " + quote_command_argument(zipPath.wstring()) +
            L" -inner " + quote_command_argument(innerAudioPath) +
            L" -outbase " + quote_command_argument(resultBase.wstring());

        const bool processOK = run_hidden_and_wait(command, processExitCode);
        fs::remove(scriptPath, ec);

        for (const wchar_t* ext : { L".jpg", L".jpeg", L".png" }) {
            fs::path candidate = resultBase;
            candidate += ext;
            ec.clear();
            if (fs::is_regular_file(candidate, ec)) {
                exported = candidate;
                return processOK;
            }
        }
        return false;
    }

    void show_message(const std::wstring& text, UINT icon = MB_ICONINFORMATION) {
        MessageBoxW(core_api::get_main_window(), text.c_str(),
            L"NowPlaying Copy & Artwork", MB_OK | icon);
    }


    size_t unicode_character_count(const std::wstring& text) {
        size_t count = 0;
        for (size_t index = 0; index < text.size(); ++index) {
            const wchar_t value = text[index];
            if (value >= 0xD800 && value <= 0xDBFF &&
                index + 1 < text.size()) {
                const wchar_t next = text[index + 1];
                if (next >= 0xDC00 && next <= 0xDFFF) {
                    ++index;
                }
            }
            ++count;
        }
        return count;
    }


    bool remove_old_post_artwork(const fs::path& outDir) {
        std::error_code ec;
        bool removedAny = false;
        for (const wchar_t* ext : { L".jpg", L".jpeg", L".png" }) {
            fs::path candidate = outDir / L"artwork-post";
            candidate += ext;
            removedAny = fs::remove(candidate, ec) || removedAny;
            ec.clear();
        }
        return removedAny;
    }

    bool optimize_artwork_for_post(
        const fs::path& inputPath,
        const fs::path& outDir,
        const artwork_post_settings& settings,
        fs::path& optimizedPath,
        std::wstring& detailText
    ) {
        optimizedPath.clear();
        detailText.clear();
        if (!settings.enabled) return false;

        HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool shouldUninitialize = initializeResult == S_OK || initializeResult == S_FALSE;

        IWICImagingFactory* factory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICBitmapClipper* clipper = nullptr;
        IWICBitmapScaler* scaler = nullptr;
        IWICFormatConverter* converter = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frameEncode = nullptr;
        IPropertyBag2* propertyBag = nullptr;
        bool success = false;

        UINT srcWidth = 0;
        UINT srcHeight = 0;
        UINT workingWidth = 0;
        UINT workingHeight = 0;
        UINT targetWidth = 0;
        UINT targetHeight = 0;

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr)) {
            hr = factory->CreateDecoderFromFilename(inputPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        }
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) hr = frame->GetSize(&srcWidth, &srcHeight);
        workingWidth = srcWidth;
        workingHeight = srcHeight;

        IWICBitmapSource* workingSource = frame;
        if (SUCCEEDED(hr) && settings.squareMode == 1 && srcWidth > 0 && srcHeight > 0) {
            const UINT side = srcWidth < srcHeight ? srcWidth : srcHeight;
            WICRect rc{};
            rc.X = static_cast<INT>((srcWidth - side) / 2);
            rc.Y = static_cast<INT>((srcHeight - side) / 2);
            rc.Width = static_cast<INT>(side);
            rc.Height = static_cast<INT>(side);
            hr = factory->CreateBitmapClipper(&clipper);
            if (SUCCEEDED(hr)) hr = clipper->Initialize(frame, &rc);
            if (SUCCEEDED(hr)) {
                workingSource = clipper;
                workingWidth = side;
                workingHeight = side;
            }
        }

        if (SUCCEEDED(hr) && workingWidth > 0 && workingHeight > 0) {
            double scale = static_cast<double>(settings.maxSize) / static_cast<double>(workingWidth > workingHeight ? workingWidth : workingHeight);
            if (scale > 1.0) scale = 1.0;
            targetWidth = static_cast<UINT>(workingWidth * scale + 0.5);
            targetHeight = static_cast<UINT>(workingHeight * scale + 0.5);
            if (targetWidth == 0) targetWidth = 1;
            if (targetHeight == 0) targetHeight = 1;
            hr = factory->CreateBitmapScaler(&scaler);
        }
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(workingSource, targetWidth, targetHeight, WICBitmapInterpolationModeFant);
        }
        if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);

        const GUID encoderClsid = settings.png ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg;
        const GUID targetPixelFormat = settings.png ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;

        if (SUCCEEDED(hr)) {
            hr = converter->Initialize(
                scaler,
                targetPixelFormat,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom);
        }

        if (SUCCEEDED(hr)) {
            remove_old_post_artwork(outDir);
            optimizedPath = outDir / (settings.png ? L"artwork-post.png" : L"artwork-post.jpg");
            hr = factory->CreateStream(&stream);
        }
        if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(optimizedPath.c_str(), GENERIC_WRITE);
        if (SUCCEEDED(hr)) hr = factory->CreateEncoder(encoderClsid, nullptr, &encoder);
        if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frameEncode, &propertyBag);
        if (SUCCEEDED(hr) && propertyBag != nullptr && !settings.png) {
            PROPBAG2 option{};
            option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_R4;
            value.fltVal = static_cast<float>(settings.jpegQuality) / 100.0f;
            propertyBag->Write(1, &option, &value);
            VariantClear(&value);
        }
        if (SUCCEEDED(hr)) hr = frameEncode->Initialize(propertyBag);
        if (SUCCEEDED(hr)) hr = frameEncode->SetSize(targetWidth, targetHeight);
        if (SUCCEEDED(hr)) {
            WICPixelFormatGUID pixelFormat = targetPixelFormat;
            hr = frameEncode->SetPixelFormat(&pixelFormat);
        }
        if (SUCCEEDED(hr)) hr = frameEncode->WriteSource(converter, nullptr);
        if (SUCCEEDED(hr)) hr = frameEncode->Commit();
        if (SUCCEEDED(hr)) hr = encoder->Commit();

        success = SUCCEEDED(hr);
        if (success) {
            // The encoder stream is still open here. Reopening artwork-post
            // through WIC can therefore fail with a sharing violation.
            // Use the dimensions already confirmed during encoding instead.
            detailText = L"元画像：";
            detailText += std::to_wstring(srcWidth);
            detailText += L" × ";
            detailText += std::to_wstring(srcHeight);

            std::error_code sourceSizeError;
            const uintmax_t sourceBytes =
                fs::file_size(inputPath, sourceSizeError);
            if (!sourceSizeError) {
                detailText += L" / ";
                detailText += format_file_size(sourceBytes);
            }

            detailText += L"\r\n投稿用：";
            detailText += std::to_wstring(targetWidth);
            detailText += L" × ";
            detailText += std::to_wstring(targetHeight);

            std::error_code postSizeError;
            const uintmax_t postBytes =
                fs::file_size(optimizedPath, postSizeError);
            if (!postSizeError) {
                detailText += L" / ";
                detailText += format_file_size(postBytes);
            }

            detailText += L"\r\n";
            detailText += settings.png ? L"PNG" : L"JPEG";
            if (!settings.png) {
                detailText += L"・画質";
                detailText += std::to_wstring(settings.jpegQuality);
            }
            detailText += L"・";
            detailText += artwork_square_mode_label(settings.squareMode);
            detailText += L"・最大";
            detailText += std::to_wstring(settings.maxSize);
            detailText += L"px";
        } else {
            optimizedPath.clear();
        }

        if (propertyBag) propertyBag->Release();
        if (frameEncode) frameEncode->Release();
        if (encoder) encoder->Release();
        if (stream) stream->Release();
        if (converter) converter->Release();
        if (scaler) scaler->Release();
        if (clipper) clipper->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (factory) factory->Release();
        if (shouldUninitialize) CoUninitialize();
        return success;
    }

    HBITMAP load_bitmap_scaled(
        const fs::path& path,
        UINT maximumWidth,
        UINT maximumHeight
    ) {
        HRESULT initializeResult =
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool shouldUninitialize =
            initializeResult == S_OK || initializeResult == S_FALSE;

        IWICImagingFactory* factory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICBitmapScaler* scaler = nullptr;
        IWICFormatConverter* converter = nullptr;
        HBITMAP bitmap = nullptr;

        HRESULT result = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));

        if (SUCCEEDED(result)) {
            result = factory->CreateDecoderFromFilename(
                path.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder);
        }
        if (SUCCEEDED(result)) {
            result = decoder->GetFrame(0, &frame);
        }

        UINT sourceWidth = 0;
        UINT sourceHeight = 0;
        if (SUCCEEDED(result)) {
            result = frame->GetSize(&sourceWidth, &sourceHeight);
        }

        UINT targetWidth = sourceWidth;
        UINT targetHeight = sourceHeight;
        if (SUCCEEDED(result) &&
            sourceWidth > 0 && sourceHeight > 0) {
            const double widthScale =
                static_cast<double>(maximumWidth) /
                static_cast<double>(sourceWidth);
            const double heightScale =
                static_cast<double>(maximumHeight) /
                static_cast<double>(sourceHeight);
            double scale =
                widthScale < heightScale ? widthScale : heightScale;
            if (scale > 1.0) scale = 1.0;

            targetWidth = static_cast<UINT>(
                static_cast<double>(sourceWidth) * scale + 0.5);
            targetHeight = static_cast<UINT>(
                static_cast<double>(sourceHeight) * scale + 0.5);
            if (targetWidth == 0) targetWidth = 1;
            if (targetHeight == 0) targetHeight = 1;

            result = factory->CreateBitmapScaler(&scaler);
        }

        if (SUCCEEDED(result)) {
            result = scaler->Initialize(
                frame,
                targetWidth,
                targetHeight,
                WICBitmapInterpolationModeFant);
        }
        if (SUCCEEDED(result)) {
            result = factory->CreateFormatConverter(&converter);
        }
        if (SUCCEEDED(result)) {
            result = converter->Initialize(
                scaler,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom);
        }

        void* pixels = nullptr;
        if (SUCCEEDED(result)) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
            info.bmiHeader.biHeight = -static_cast<LONG>(targetHeight);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            HDC screen = GetDC(nullptr);
            bitmap = CreateDIBSection(
                screen,
                &info,
                DIB_RGB_COLORS,
                &pixels,
                nullptr,
                0);
            ReleaseDC(nullptr, screen);

            if (bitmap == nullptr || pixels == nullptr) {
                result = E_OUTOFMEMORY;
            }
        }

        if (SUCCEEDED(result)) {
            const UINT stride = targetWidth * 4;
            const UINT bufferSize = stride * targetHeight;
            result = converter->CopyPixels(
                nullptr,
                stride,
                bufferSize,
                static_cast<BYTE*>(pixels));
        }

        if (FAILED(result) && bitmap != nullptr) {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }

        if (converter != nullptr) converter->Release();
        if (scaler != nullptr) scaler->Release();
        if (frame != nullptr) frame->Release();
        if (decoder != nullptr) decoder->Release();
        if (factory != nullptr) factory->Release();

        if (shouldUninitialize) CoUninitialize();
        return bitmap;
    }

    std::wstring get_window_text_string(HWND wnd);

    HGLOBAL create_clipboard_dib(HBITMAP bitmap) {
        if (bitmap == nullptr) return nullptr;

        BITMAP bm{};
        if (GetObjectW(bitmap, sizeof(bm), &bm) != sizeof(bm)) return nullptr;
        if (bm.bmWidth <= 0 || bm.bmHeight == 0) return nullptr;

        const int width = bm.bmWidth;
        const int height = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        const WORD bitCount = 32;
        const DWORD rowBytes = static_cast<DWORD>(width) * 4;
        const DWORD imageBytes = rowBytes * static_cast<DWORD>(height);
        const SIZE_T totalBytes = sizeof(BITMAPINFOHEADER) + imageBytes;

        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
        if (memory == nullptr) return nullptr;

        void* locked = GlobalLock(memory);
        if (locked == nullptr) {
            GlobalFree(memory);
            return nullptr;
        }

        auto* header = static_cast<BITMAPINFOHEADER*>(locked);
        ZeroMemory(header, sizeof(BITMAPINFOHEADER));
        header->biSize = sizeof(BITMAPINFOHEADER);
        header->biWidth = width;
        header->biHeight = -height; // top-down DIB
        header->biPlanes = 1;
        header->biBitCount = bitCount;
        header->biCompression = BI_RGB;
        header->biSizeImage = imageBytes;

        BITMAPINFO bmi{};
        bmi.bmiHeader = *header;

        HDC screenDC = GetDC(nullptr);
        if (screenDC == nullptr) {
            GlobalUnlock(memory);
            GlobalFree(memory);
            return nullptr;
        }

        void* bits = static_cast<unsigned char*>(locked) + sizeof(BITMAPINFOHEADER);
        const int scanLines = GetDIBits(
            screenDC,
            bitmap,
            0,
            static_cast<UINT>(height),
            bits,
            &bmi,
            DIB_RGB_COLORS);
        ReleaseDC(nullptr, screenDC);

        if (scanLines == 0) {
            GlobalUnlock(memory);
            GlobalFree(memory);
            return nullptr;
        }

        GlobalUnlock(memory);
        return memory;
    }

    bool copy_artwork_to_clipboard(const fs::path& artworkPath, HWND owner) {
        if (artworkPath.empty()) return false;

        HBITMAP bitmap = load_bitmap_scaled(artworkPath, 4096, 4096);
        if (bitmap == nullptr) return false;

        HGLOBAL dib = create_clipboard_dib(bitmap);

        if (!OpenClipboard(owner)) {
            if (dib != nullptr) GlobalFree(dib);
            DeleteObject(bitmap);
            return false;
        }

        EmptyClipboard();

        bool success = false;
        bool bitmapOwnedByClipboard = false;
        bool dibOwnedByClipboard = false;

        if (dib != nullptr && SetClipboardData(CF_DIB, dib) != nullptr) {
            dibOwnedByClipboard = true;
            success = true;
        }

        if (SetClipboardData(CF_BITMAP, bitmap) != nullptr) {
            bitmapOwnedByClipboard = true;
            success = true;
        }

        CloseClipboard();

        if (!dibOwnedByClipboard && dib != nullptr) {
            GlobalFree(dib);
        }
        if (!bitmapOwnedByClipboard) {
            DeleteObject(bitmap);
        }

        return success;
    }

    bool is_ascii_alnum(unsigned char value) {
        return (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9');
    }

    std::wstring percent_encode_utf8(const std::wstring& value) {
        const std::string utf8 = wide_to_utf8(value);
        static const wchar_t hex[] = L"0123456789ABCDEF";
        std::wstring encoded;
        encoded.reserve(utf8.size() * 3);

        for (unsigned char byte : utf8) {
            if (is_ascii_alnum(byte) ||
                byte == '-' || byte == '_' || byte == '.' || byte == '~') {
                encoded.push_back(static_cast<wchar_t>(byte));
            } else {
                encoded.push_back(L'%');
                encoded.push_back(hex[(byte >> 4) & 0x0F]);
                encoded.push_back(hex[byte & 0x0F]);
            }
        }
        return encoded;
    }

    bool starts_with_at(
        const std::wstring& text,
        size_t offset,
        const wchar_t* prefix) {
        const size_t length = std::wcslen(prefix);
        if (offset + length > text.size()) return false;
        return text.compare(offset, length, prefix) == 0;
    }

    size_t x_weight_for_codepoint(unsigned int codepoint) {
        // Approximation of the public twitter-text weighting ranges.
        if (codepoint <= 0x10FF ||
            (codepoint >= 0x2000 && codepoint <= 0x200D) ||
            (codepoint >= 0x2010 && codepoint <= 0x201F) ||
            (codepoint >= 0x2032 && codepoint <= 0x2037)) {
            return 1;
        }
        return 2;
    }

    size_t x_weighted_length(const std::wstring& text) {
        size_t result = 0;
        for (size_t index = 0; index < text.size();) {
            if (starts_with_at(text, index, L"https://") ||
                starts_with_at(text, index, L"http://")) {
                size_t end = index;
                while (end < text.size() &&
                    !std::iswspace(static_cast<wint_t>(text[end]))) {
                    ++end;
                }
                result += 23;
                index = end;
                continue;
            }

            unsigned int codepoint = static_cast<unsigned int>(text[index]);
            size_t consumed = 1;
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
                index + 1 < text.size()) {
                const unsigned int low =
                    static_cast<unsigned int>(text[index + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codepoint = 0x10000 +
                        ((codepoint - 0xD800) << 10) +
                        (low - 0xDC00);
                    consumed = 2;
                }
            }
            result += x_weight_for_codepoint(codepoint);
            index += consumed;
        }
        return result;
    }


    struct nowplaying_history_record {
        std::wstring timestamp;
        std::wstring title;
        std::wstring artist;
        std::wstring templateName;
        std::wstring text;
        fs::path artworkPath;
        fs::path outputDirectory;
        bool pinned = false;
    };

    constexpr UINT history_limit_default = 100;
    constexpr UINT history_limit_minimum = 10;
    constexpr UINT history_limit_maximum = 1000;

    size_t configured_history_limit() {
        return static_cast<size_t>(parse_uint_or_default(
            g_cfg_history_limit.get(),
            history_limit_default,
            history_limit_minimum,
            history_limit_maximum));
    }

    int configured_history_display_limit() {
        const int value = parse_int_or_default(
            g_cfg_history_display_limit.get(),
            100,
            0,
            500);
        switch (value) {
        case 0:
        case 50:
        case 100:
        case 200:
        case 500:
            return value;
        default:
            return 100;
        }
    }

    int configured_history_display_limit_index() {
        switch (configured_history_display_limit()) {
        case 0: return 0;
        case 50: return 1;
        case 100: return 2;
        case 200: return 3;
        case 500: return 4;
        default: return 2;
        }
    }

    int history_display_limit_from_index(LRESULT index) {
        switch (index) {
        case 0: return 0;
        case 1: return 50;
        case 2: return 100;
        case 3: return 200;
        case 4: return 500;
        default: return 100;
        }
    }

    std::wstring configured_history_display_limit_label() {
        const int limit = configured_history_display_limit();
        return limit == 0
            ? L"すべて"
            : std::to_wstring(limit) + L"件";
    }

    fs::path history_file_path(const fs::path& outputDirectory) {
        return outputDirectory / L"nowplaying-history.tsv";
    }

    std::wstring current_local_timestamp() {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t buffer[64]{};
        swprintf_s(
            buffer,
            L"%04u-%02u-%02u %02u:%02u:%02u",
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond);
        return buffer;
    }

    int history_hex_value(wchar_t value) {
        if (value >= L'0' && value <= L'9') return value - L'0';
        if (value >= L'A' && value <= L'F') return value - L'A' + 10;
        if (value >= L'a' && value <= L'f') return value - L'a' + 10;
        return -1;
    }

    std::wstring percent_decode_utf8(const std::wstring& value) {
        std::string bytes;
        bytes.reserve(value.size());

        for (size_t index = 0; index < value.size();) {
            if (value[index] == L'%' && index + 2 < value.size()) {
                const int high = history_hex_value(value[index + 1]);
                const int low = history_hex_value(value[index + 2]);
                if (high >= 0 && low >= 0) {
                    bytes.push_back(static_cast<char>((high << 4) | low));
                    index += 3;
                    continue;
                }
            }

            if (value[index] <= 0x7F) {
                bytes.push_back(static_cast<char>(value[index]));
            } else {
                bytes += wide_to_utf8(std::wstring(1, value[index]));
            }
            ++index;
        }

        return utf8_to_wide(bytes.c_str());
    }

    std::vector<std::string> split_history_line(const std::string& line) {
        std::vector<std::string> fields;
        size_t position = 0;
        while (true) {
            const size_t tab = line.find('\t', position);
            if (tab == std::string::npos) {
                fields.push_back(line.substr(position));
                break;
            }
            fields.push_back(line.substr(position, tab - position));
            position = tab + 1;
        }
        return fields;
    }

    std::wstring decode_history_field(const std::string& field) {
        return percent_decode_utf8(utf8_to_wide(field.c_str()));
    }

    std::vector<nowplaying_history_record> load_history_records(
        const fs::path& outputDirectory
    ) {
        std::vector<nowplaying_history_record> records;
        std::ifstream file(history_file_path(outputDirectory), std::ios::binary);
        if (!file) return records;

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        size_t position = 0;
        while (position <= content.size()) {
            size_t lineEnd = content.find('\n', position);
            std::string line =
                lineEnd == std::string::npos
                    ? content.substr(position)
                    : content.substr(position, lineEnd - position);

            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (!line.empty()) {
                const auto fields = split_history_line(line);
                if (fields.size() >= 7) {
                    nowplaying_history_record record;
                    record.timestamp = decode_history_field(fields[0]);
                    record.title = decode_history_field(fields[1]);
                    record.artist = decode_history_field(fields[2]);
                    record.templateName = decode_history_field(fields[3]);
                    record.text = decode_history_field(fields[4]);
                    record.artworkPath =
                        fs::path(decode_history_field(fields[5]));
                    record.outputDirectory =
                        fs::path(decode_history_field(fields[6]));
                    if (fields.size() >= 8) {
                        const std::wstring pinnedValue =
                            lower(decode_history_field(fields[7]));
                        record.pinned =
                            pinnedValue == L"1" ||
                            pinnedValue == L"true" ||
                            pinnedValue == L"yes";
                    }
                    records.push_back(std::move(record));
                }
            }

            if (lineEnd == std::string::npos) break;
            position = lineEnd + 1;
        }

        return records;
    }

    bool write_history_records(
        const fs::path& outputDirectory,
        const std::vector<nowplaying_history_record>& records
    ) {
        std::error_code ec;
        fs::create_directories(outputDirectory, ec);
        if (ec) return false;

        const fs::path finalPath =
            history_file_path(outputDirectory);
        fs::path temporaryPath = finalPath;
        temporaryPath += L".tmp";

        {
            std::ofstream file(
                temporaryPath,
                std::ios::binary | std::ios::trunc);
            if (!file) return false;

            const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            file.write(
                reinterpret_cast<const char*>(bom),
                sizeof(bom));

            for (const auto& record : records) {
                std::wstring line;
                line += percent_encode_utf8(record.timestamp);
                line += L"\t";
                line += percent_encode_utf8(record.title);
                line += L"\t";
                line += percent_encode_utf8(record.artist);
                line += L"\t";
                line += percent_encode_utf8(record.templateName);
                line += L"\t";
                line += percent_encode_utf8(record.text);
                line += L"\t";
                line += percent_encode_utf8(
                    record.artworkPath.wstring());
                line += L"\t";
                line += percent_encode_utf8(
                    record.outputDirectory.wstring());
                line += L"\t";
                line += record.pinned ? L"1" : L"0";
                line += L"\r\n";

                const std::string utf8Line =
                    wide_to_utf8(line);
                file.write(
                    utf8Line.data(),
                    static_cast<std::streamsize>(
                        utf8Line.size()));
            }

            file.flush();
            if (!file.good()) {
                file.close();
                fs::remove(temporaryPath, ec);
                return false;
            }
        }

        if (!MoveFileExW(
                temporaryPath.c_str(),
                finalPath.c_str(),
                MOVEFILE_REPLACE_EXISTING |
                    MOVEFILE_WRITE_THROUGH)) {
            fs::remove(temporaryPath, ec);
            return false;
        }

        return true;
    }

    fs::path archive_history_artwork(
        const fs::path& outputDirectory,
        const fs::path& sourcePath,
        const std::wstring& timestamp
    ) {
        if (sourcePath.empty()) return {};

        std::error_code ec;
        if (!fs::is_regular_file(sourcePath, ec)) return {};

        const fs::path archiveDirectory =
            outputDirectory / L"nowplaying-history-artwork";
        fs::create_directories(archiveDirectory, ec);
        if (ec) return {};

        std::wstring baseName;
        for (wchar_t value : timestamp) {
            if ((value >= L'0' && value <= L'9') ||
                (value >= L'A' && value <= L'Z') ||
                (value >= L'a' && value <= L'z')) {
                baseName.push_back(value);
            } else {
                baseName.push_back(L'-');
            }
        }
        if (baseName.empty()) baseName = L"artwork";

        std::wstring extension = lower(sourcePath.extension().wstring());
        if (extension.empty() || extension.size() > 8) {
            extension = L".jpg";
        }

        for (int suffix = 0; suffix < 1000; ++suffix) {
            std::wstring fileName = baseName;
            if (suffix > 0) {
                fileName += L"-";
                fileName += std::to_wstring(suffix + 1);
            }
            fileName += extension;

            const fs::path destination =
                archiveDirectory / fileName;
            ec.clear();
            if (fs::exists(destination, ec)) continue;

            ec.clear();
            if (fs::copy_file(
                    sourcePath,
                    destination,
                    fs::copy_options::none,
                    ec)) {
                return destination;
            }
            if (ec) return {};
        }

        return {};
    }

    bool history_records_reference_artwork(
        const std::vector<nowplaying_history_record>& records,
        const fs::path& artworkPath
    ) {
        if (artworkPath.empty()) return false;
        for (const auto& record : records) {
            if (record.artworkPath == artworkPath) {
                return true;
            }
        }
        return false;
    }

    void trim_history_records_preserving_pinned(
        std::vector<nowplaying_history_record>& records,
        std::vector<fs::path>& removedArtwork
    ) {
        const size_t normalHistoryLimit =
            configured_history_limit();
        size_t keptNormalEntries = 0;

        std::vector<nowplaying_history_record> kept;
        kept.reserve(records.size());

        for (auto& record : records) {
            if (record.pinned ||
                keptNormalEntries < normalHistoryLimit) {
                if (!record.pinned) {
                    ++keptNormalEntries;
                }
                kept.push_back(std::move(record));
            } else if (!record.artworkPath.empty()) {
                removedArtwork.push_back(record.artworkPath);
            }
        }

        records = std::move(kept);
    }

    void remove_unreferenced_trimmed_artwork(
        const std::vector<fs::path>& candidates,
        const std::vector<nowplaying_history_record>& keptRecords
    ) {
        for (const auto& candidate : candidates) {
            if (candidate.empty() ||
                candidate.parent_path().filename() !=
                    L"nowplaying-history-artwork" ||
                history_records_reference_artwork(
                    keptRecords,
                    candidate)) {
                continue;
            }

            std::error_code removeError;
            fs::remove(candidate, removeError);
        }
    }

    bool append_history_record(
        const fs::path& outputDirectory,
        nowplaying_history_record record
    ) {
        const fs::path archivedArtwork =
            archive_history_artwork(
                outputDirectory,
                record.artworkPath,
                record.timestamp);
        if (!archivedArtwork.empty()) {
            record.artworkPath = archivedArtwork;
        }

        auto records = load_history_records(outputDirectory);
        records.insert(records.begin(), std::move(record));

        std::vector<fs::path> removedArtwork;
        trim_history_records_preserving_pinned(
            records,
            removedArtwork);

        if (!write_history_records(
                outputDirectory,
                records)) {
            return false;
        }

        remove_unreferenced_trimmed_artwork(
            removedArtwork,
            records);
        return true;
    }

    bool open_history_record_in_preview(
        const nowplaying_history_record& record,
        const fs::path& outputDirectory);

    class nowplaying_history_window {
    public:
        static bool open(const fs::path& outputDirectory) {
            auto* instance =
                new nowplaying_history_window(outputDirectory);
            if (!instance->create()) {
                delete instance;
                return false;
            }
            return true;
        }

    private:
        enum : int {
            IDC_HISTORY_LIST = 3101,
            IDC_HISTORY_TEXT = 3102,
            IDC_HISTORY_COPY_TEXT = 3103,
            IDC_HISTORY_COPY_IMAGE = 3104,
            IDC_HISTORY_OPEN_X = 3105,
            IDC_HISTORY_COPY_IMAGE_OPEN_X = 3106,
            IDC_HISTORY_OPEN_FOLDER = 3107,
            IDC_HISTORY_DELETE = 3108,
            IDC_HISTORY_CLEAR = 3109,
            IDC_HISTORY_CLOSE = 3110,
            IDC_HISTORY_SEARCH = 3111,
            IDC_HISTORY_RELOAD = 3112,
            IDC_HISTORY_EXPORT = 3113,
            IDC_HISTORY_IMPORT = 3114,
            IDC_HISTORY_PREVIEW = 3115,
            IDC_HISTORY_PIN = 3116,
            IDC_HISTORY_PINNED_ONLY = 3117
        };

        explicit nowplaying_history_window(fs::path outputDirectory)
            : m_outputDirectory(std::move(outputDirectory)) {
        }

        static const wchar_t* window_class_name() {
            return L"FooNowPlayingHistoryWindow";
        }

        static void register_window_class() {
            static std::once_flag once;
            std::call_once(once, [] {
                WNDCLASSEXW windowClass{};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.lpfnWndProc = window_proc;
                windowClass.hInstance = core_api::get_my_instance();
                windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
                windowClass.hbrBackground = nullptr;
                windowClass.lpszClassName = window_class_name();
                RegisterClassExW(&windowClass);
            });
        }

        bool create() {
            register_window_class();

            constexpr int clientWidth = 860;
            constexpr int clientHeight = 540;
            RECT windowRect{ 0, 0, clientWidth, clientHeight };
            AdjustWindowRectEx(
                &windowRect,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                    WS_MINIMIZEBOX | WS_THICKFRAME,
                FALSE,
                WS_EX_DLGMODALFRAME);

            const int width = windowRect.right - windowRect.left;
            const int height = windowRect.bottom - windowRect.top;
            HWND parent = core_api::get_main_window();
            RECT parentRect{};
            GetWindowRect(parent, &parentRect);
            const int x = parentRect.left +
                ((parentRect.right - parentRect.left) - width) / 2;
            const int y = parentRect.top +
                ((parentRect.bottom - parentRect.top) - height) / 2;

            m_wnd = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                window_class_name(),
                L"NowPlaying投稿履歴",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                    WS_MINIMIZEBOX | WS_THICKFRAME,
                x, y, width, height,
                parent, nullptr, core_api::get_my_instance(), this);
            if (m_wnd == nullptr) return false;

            m_creationComplete = true;
            create_controls();
            m_dark.AddDialogWithControls(m_wnd);
            reload_records();

            ShowWindow(m_wnd, SW_SHOWNORMAL);
            UpdateWindow(m_wnd);
            SetForegroundWindow(m_wnd);
            return true;
        }

        static LRESULT CALLBACK window_proc(
            HWND wnd, UINT message, WPARAM wp, LPARAM lp
        ) {
            nowplaying_history_window* self =
                reinterpret_cast<nowplaying_history_window*>(
                    GetWindowLongPtrW(wnd, GWLP_USERDATA));

            if (message == WM_NCCREATE) {
                const auto* create =
                    reinterpret_cast<CREATESTRUCTW*>(lp);
                self = static_cast<nowplaying_history_window*>(
                    create->lpCreateParams);
                self->m_wnd = wnd;
                SetWindowLongPtrW(
                    wnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }

            if (self == nullptr) {
                return DefWindowProcW(wnd, message, wp, lp);
            }

            const LRESULT result =
                self->on_message(wnd, message, wp, lp);
            if (message == WM_NCDESTROY) {
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                self->m_wnd = nullptr;
                if (self->m_creationComplete) delete self;
            }
            return result;
        }

        LRESULT on_message(
            HWND wnd, UINT message, WPARAM wp, LPARAM lp
        ) {
            switch (message) {
            case WM_ERASEBKGND:
                return paint_nowplaying_page_background(
                    wnd,
                    reinterpret_cast<HDC>(wp),
                    static_cast<bool>(m_dark));

            case WM_SIZE:
                layout_controls();
                return 0;

            case WM_COMMAND: {
                const int id = LOWORD(wp);
                const int notification = HIWORD(wp);

                if (id == IDC_HISTORY_LIST &&
                    notification == LBN_SELCHANGE) {
                    update_selected_record();
                    return 0;
                }
                if (id == IDC_HISTORY_LIST &&
                    notification == LBN_DBLCLK) {
                    open_selected_in_preview();
                    return 0;
                }
                if (id == IDC_HISTORY_SEARCH &&
                    notification == EN_CHANGE) {
                    rebuild_history_list();
                    return 0;
                }
                if (id == IDC_HISTORY_PINNED_ONLY &&
                    notification == BN_CLICKED) {
                    rebuild_history_list();
                    return 0;
                }
                if (id == IDC_HISTORY_RELOAD &&
                    notification == BN_CLICKED) {
                    reload_records();
                    set_status(L"投稿履歴を再読み込みしました。");
                    return 0;
                }
                if (id == IDC_HISTORY_EXPORT &&
                    notification == BN_CLICKED) {
                    export_history_backup();
                    return 0;
                }
                if (id == IDC_HISTORY_IMPORT &&
                    notification == BN_CLICKED) {
                    import_history_backup();
                    return 0;
                }
                if (id == IDC_HISTORY_PREVIEW &&
                    notification == BN_CLICKED) {
                    open_selected_in_preview();
                    return 0;
                }
                if (id == IDC_HISTORY_PIN &&
                    notification == BN_CLICKED) {
                    toggle_selected_pin();
                    return 0;
                }
                if (id == IDC_HISTORY_COPY_TEXT &&
                    notification == BN_CLICKED) {
                    copy_selected_text();
                    return 0;
                }
                if (id == IDC_HISTORY_COPY_IMAGE &&
                    notification == BN_CLICKED) {
                    copy_selected_image();
                    return 0;
                }
                if (id == IDC_HISTORY_OPEN_X &&
                    notification == BN_CLICKED) {
                    open_selected_in_x(false);
                    return 0;
                }
                if (id == IDC_HISTORY_COPY_IMAGE_OPEN_X &&
                    notification == BN_CLICKED) {
                    open_selected_in_x(true);
                    return 0;
                }
                if (id == IDC_HISTORY_OPEN_FOLDER &&
                    notification == BN_CLICKED) {
                    open_selected_folder();
                    return 0;
                }
                if (id == IDC_HISTORY_DELETE &&
                    notification == BN_CLICKED) {
                    delete_selected_record();
                    return 0;
                }
                if (id == IDC_HISTORY_CLEAR &&
                    notification == BN_CLICKED) {
                    clear_records();
                    return 0;
                }
                if (id == IDC_HISTORY_CLOSE &&
                    notification == BN_CLICKED) {
                    DestroyWindow(wnd);
                    return 0;
                }
                break;
            }

            case WM_CLOSE:
                DestroyWindow(wnd);
                return 0;
            }

            return DefWindowProcW(wnd, message, wp, lp);
        }

        HWND create_control(
            DWORD exStyle,
            const wchar_t* className,
            const wchar_t* text,
            DWORD style,
            int id
        ) {
            HWND control = CreateWindowExW(
                exStyle,
                className,
                text,
                WS_CHILD | WS_VISIBLE | style,
                0, 0, 0, 0,
                m_wnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                core_api::get_my_instance(),
                nullptr);

            if (control != nullptr) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        GetStockObject(DEFAULT_GUI_FONT)),
                    TRUE);
            }
            return control;
        }

        void create_controls() {
            m_listLabel = create_control(
                0,
                L"STATIC",
                L"投稿履歴",
                0,
                0);
            m_searchEdit = create_control(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_TABSTOP | ES_AUTOHSCROLL,
                IDC_HISTORY_SEARCH);
            SendMessageW(
                m_searchEdit,
                EM_SETCUEBANNER,
                TRUE,
                reinterpret_cast<LPARAM>(
                    L"曲名・アーティスト・投稿文などを検索"));
            m_reloadButton = create_control(
                0,
                L"BUTTON",
                L"再読込",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_RELOAD);
            m_pinnedOnlyCheck = create_control(
                0,
                L"BUTTON",
                L"★ ピン留めのみ表示",
                WS_TABSTOP | BS_AUTOCHECKBOX,
                IDC_HISTORY_PINNED_ONLY);
            m_list = create_control(
                WS_EX_CLIENTEDGE,
                L"LISTBOX",
                L"",
                WS_TABSTOP | LBS_NOTIFY | LBS_EXTENDEDSEL |
                    LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
                IDC_HISTORY_LIST);
            m_exportHistoryButton = create_control(
                0,
                L"BUTTON",
                L"履歴を書き出す...",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_EXPORT);
            m_importHistoryButton = create_control(
                0,
                L"BUTTON",
                L"履歴を読み込む...",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_IMPORT);

            m_detailLabel = create_control(
                0,
                L"STATIC",
                L"選択した履歴（Ctrl／Shiftで複数選択）",
                0,
                0);
            m_previewButton = create_control(
                0,
                L"BUTTON",
                L"プレビューで開く",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_PREVIEW);
            m_metadataLabel = create_control(
                0,
                L"STATIC",
                L"",
                0,
                0);
            m_textEdit = create_control(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                    ES_WANTRETURN | WS_VSCROLL,
                IDC_HISTORY_TEXT);
            m_pathLabel = create_control(
                0,
                L"STATIC",
                L"",
                SS_PATHELLIPSIS,
                0);

            m_copyTextButton = create_control(
                0,
                L"BUTTON",
                L"文章をコピー",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_COPY_TEXT);
            m_copyImageButton = create_control(
                0,
                L"BUTTON",
                L"画像をコピー",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_COPY_IMAGE);
            m_openXButton = create_control(
                0,
                L"BUTTON",
                L"Xで再投稿",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_OPEN_X);
            m_copyImageOpenXButton = create_control(
                0,
                L"BUTTON",
                L"画像をコピーしてXを開く",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_COPY_IMAGE_OPEN_X);
            m_openFolderButton = create_control(
                0,
                L"BUTTON",
                L"保存フォルダを開く",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_OPEN_FOLDER);
            m_pinButton = create_control(
                0,
                L"BUTTON",
                L"ピン留め",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_PIN);
            m_deleteButton = create_control(
                0,
                L"BUTTON",
                L"選択履歴を削除",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_DELETE);
            m_clearButton = create_control(
                0,
                L"BUTTON",
                L"すべて削除",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_CLEAR);
            m_closeButton = create_control(
                0,
                L"BUTTON",
                L"閉じる",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HISTORY_CLOSE);
            m_statusLabel = create_control(
                0,
                L"STATIC",
                L"",
                0,
                0);

            layout_controls();
        }

        void layout_controls() {
            if (m_wnd == nullptr || m_list == nullptr) return;

            RECT client{};
            GetClientRect(m_wnd, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            const int margin = 14;
            const int gap = 14;
            const int listWidth = max(250, width * 36 / 100);
            const int rightX = margin + listWidth + gap;
            const int rightWidth = width - rightX - margin;

            MoveWindow(
                m_listLabel,
                margin, margin,
                listWidth, 22,
                TRUE);

            const int reloadWidth = 68;
            const int searchGap = 6;
            MoveWindow(
                m_searchEdit,
                margin, margin + 24,
                max(100, listWidth - reloadWidth - searchGap),
                24,
                TRUE);
            MoveWindow(
                m_reloadButton,
                margin + listWidth - reloadWidth,
                margin + 24,
                reloadWidth,
                24,
                TRUE);
            MoveWindow(
                m_pinnedOnlyCheck,
                margin,
                margin + 52,
                listWidth,
                22,
                TRUE);
            const int historyBackupButtonY =
                height - 72;
            const int historyBackupButtonGap = 6;
            const int historyBackupButtonWidth =
                max(100, (listWidth - historyBackupButtonGap) / 2);

            MoveWindow(
                m_list,
                margin, margin + 78,
                listWidth, max(100, height - 184),
                TRUE);
            MoveWindow(
                m_exportHistoryButton,
                margin,
                historyBackupButtonY,
                historyBackupButtonWidth,
                28,
                TRUE);
            MoveWindow(
                m_importHistoryButton,
                margin + historyBackupButtonWidth +
                    historyBackupButtonGap,
                historyBackupButtonY,
                max(1, listWidth -
                    historyBackupButtonWidth -
                    historyBackupButtonGap),
                28,
                TRUE);

            const int previewButtonWidth = 132;
            MoveWindow(
                m_detailLabel,
                rightX, margin,
                max(1, rightWidth -
                    previewButtonWidth - 8),
                22,
                TRUE);
            MoveWindow(
                m_previewButton,
                rightX + rightWidth -
                    previewButtonWidth,
                margin - 2,
                previewButtonWidth,
                26,
                TRUE);
            MoveWindow(
                m_metadataLabel,
                rightX, margin + 24,
                rightWidth, 48,
                TRUE);
            MoveWindow(
                m_textEdit,
                rightX, margin + 76,
                rightWidth, max(120, height - 246),
                TRUE);
            MoveWindow(
                m_pathLabel,
                rightX, height - 154,
                rightWidth, 38,
                TRUE);

            const int buttonHeight = 28;
            const int buttonGap = 7;
            int buttonX = rightX;
            const int firstRowY = height - 108;
            const int secondRowY = height - 72;

            MoveWindow(
                m_copyTextButton,
                buttonX, firstRowY,
                104, buttonHeight,
                TRUE);
            buttonX += 104 + buttonGap;
            MoveWindow(
                m_copyImageButton,
                buttonX, firstRowY,
                104, buttonHeight,
                TRUE);
            buttonX += 104 + buttonGap;
            MoveWindow(
                m_openXButton,
                buttonX, firstRowY,
                100, buttonHeight,
                TRUE);
            buttonX += 100 + buttonGap;
            MoveWindow(
                m_copyImageOpenXButton,
                buttonX, firstRowY,
                max(160, rightWidth - (buttonX - rightX)),
                buttonHeight,
                TRUE);

            buttonX = rightX;
            MoveWindow(
                m_openFolderButton,
                buttonX, secondRowY,
                116, buttonHeight,
                TRUE);
            buttonX += 116 + buttonGap;
            MoveWindow(
                m_pinButton,
                buttonX, secondRowY,
                100, buttonHeight,
                TRUE);
            buttonX += 100 + buttonGap;
            MoveWindow(
                m_deleteButton,
                buttonX, secondRowY,
                104, buttonHeight,
                TRUE);
            buttonX += 104 + buttonGap;
            MoveWindow(
                m_clearButton,
                buttonX, secondRowY,
                78, buttonHeight,
                TRUE);
            MoveWindow(
                m_closeButton,
                width - margin - 82,
                secondRowY,
                82, buttonHeight,
                TRUE);

            MoveWindow(
                m_statusLabel,
                margin, height - 34,
                width - margin * 2, 22,
                TRUE);
        }

        bool record_matches_search(
            const nowplaying_history_record& record,
            const std::wstring& query
        ) const {
            if (query.empty()) return true;

            std::wstring searchable;
            searchable.reserve(
                record.timestamp.size() +
                record.title.size() +
                record.artist.size() +
                record.templateName.size() +
                record.text.size() + 16);
            searchable += record.timestamp;
            searchable += L"\n";
            searchable += record.title;
            searchable += L"\n";
            searchable += record.artist;
            searchable += L"\n";
            searchable += record.templateName;
            searchable += L"\n";
            searchable += record.text;

            return lower(searchable).find(query) !=
                std::wstring::npos;
        }

        void rebuild_history_list() {
            const int previouslySelected = selected_index();
            const std::wstring query =
                lower(get_window_text_string(m_searchEdit));
            const bool searching = !query.empty();
            const bool pinnedOnly =
                SendMessageW(
                    m_pinnedOnlyCheck,
                    BM_GETCHECK,
                    0,
                    0) == BST_CHECKED;
            const bool filtering = searching || pinnedOnly;
            const int displayLimit =
                configured_history_display_limit();

            m_visibleIndices.clear();
            SendMessageW(m_list, LB_RESETCONTENT, 0, 0);

            int newSelection = -1;
            for (size_t index = 0;
                 index < m_records.size();
                 ++index) {
                const auto& record = m_records[index];
                if (!record_matches_search(record, query)) {
                    continue;
                }
                if (pinnedOnly && !record.pinned) {
                    continue;
                }
                if (!filtering &&
                    displayLimit > 0 &&
                    m_visibleIndices.size() >=
                        static_cast<size_t>(displayLimit)) {
                    break;
                }

                if (static_cast<int>(index) ==
                    previouslySelected) {
                    newSelection =
                        static_cast<int>(
                            m_visibleIndices.size());
                }
                m_visibleIndices.push_back(index);

                std::wstring label =
                    record.pinned ? L"★ [" : L"[";
                label += record.timestamp;
                label += L"] ";
                if (!record.artist.empty()) {
                    label += record.artist;
                    label += L" - ";
                }
                label += record.title.empty()
                    ? L"（タイトル不明）"
                    : record.title;

                SendMessageW(
                    m_list,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        label.c_str()));
            }

            const size_t pinnedCount =
                static_cast<size_t>(std::count_if(
                    m_records.begin(),
                    m_records.end(),
                    [](const nowplaying_history_record& record) {
                        return record.pinned;
                    }));

            std::wstring listTitle =
                L"投稿履歴（表示 ";
            listTitle +=
                std::to_wstring(m_visibleIndices.size());
            listTitle += L" / 保存 ";
            listTitle += std::to_wstring(m_records.size());
            listTitle += L"件・★";
            listTitle += std::to_wstring(pinnedCount);
            listTitle += L"件・通常履歴上限 ";
            listTitle += std::to_wstring(configured_history_limit());
            listTitle += L"件・表示上限 ";
            listTitle += configured_history_display_limit_label();
            if (searching) {
                listTitle += L"・検索は全履歴対象";
            }
            if (pinnedOnly) {
                listTitle += L"・ピン留めのみ";
            }
            listTitle += L"）";
            SetWindowTextW(
                m_listLabel,
                listTitle.c_str());

            if (!m_visibleIndices.empty()) {
                if (newSelection < 0) newSelection = 0;
                SendMessageW(
                    m_list,
                    LB_SETSEL,
                    TRUE,
                    newSelection);
                SendMessageW(
                    m_list,
                    LB_SETCARETINDEX,
                    newSelection,
                    FALSE);
            }

            update_selected_record();
        }

        void reload_records() {
            m_records =
                load_history_records(m_outputDirectory);
            rebuild_history_list();

            std::wstring status =
                L"履歴ファイル：";
            status +=
                history_file_path(
                    m_outputDirectory).wstring();
            SetWindowTextW(
                m_statusLabel,
                status.c_str());
        }

        std::vector<int> selected_indices() const {
            std::vector<int> records;

            const LRESULT selectedCount = SendMessageW(
                m_list,
                LB_GETSELCOUNT,
                0,
                0);
            if (selectedCount == LB_ERR || selectedCount <= 0) {
                return records;
            }

            std::vector<int> selectedVisibleIndices(
                static_cast<size_t>(selectedCount));
            const LRESULT copied = SendMessageW(
                m_list,
                LB_GETSELITEMS,
                selectedCount,
                reinterpret_cast<LPARAM>(
                    selectedVisibleIndices.data()));
            if (copied == LB_ERR || copied <= 0) {
                return records;
            }

            records.reserve(static_cast<size_t>(copied));
            for (LRESULT item = 0; item < copied; ++item) {
                const int visibleIndex =
                    selectedVisibleIndices[static_cast<size_t>(item)];
                if (visibleIndex < 0) continue;

                const size_t visibleIndexValue =
                    static_cast<size_t>(visibleIndex);
                if (visibleIndexValue >= m_visibleIndices.size()) {
                    continue;
                }

                const size_t recordIndex =
                    m_visibleIndices[visibleIndexValue];
                if (recordIndex >= m_records.size()) {
                    continue;
                }

                records.push_back(static_cast<int>(recordIndex));
            }

            return records;
        }

        int selected_index() const {
            const auto selected = selected_indices();
            if (selected.size() != 1) return -1;
            return selected.front();
        }

        void update_selected_record() {
            const auto selectedIndices = selected_indices();
            const size_t selectionCount = selectedIndices.size();
            const bool selected = selectionCount > 0;

            if (!selected) {
                SetWindowTextW(
                    m_metadataLabel,
                    m_records.empty()
                        ? L"投稿履歴はまだありません。"
                        : (m_visibleIndices.empty()
                            ? L"検索条件に一致する履歴はありません。"
                            : L"履歴を選択してください。"));
                SetWindowTextW(m_textEdit, L"");
                SetWindowTextW(m_pathLabel, L"");
            } else if (selectionCount == 1) {
                const auto& record =
                    m_records[static_cast<size_t>(selectedIndices.front())];

                std::wstring metadata =
                    L"日時：" + record.timestamp;
                metadata += L"\r\nテンプレート：";
                metadata += record.templateName.empty()
                    ? L"（不明）"
                    : record.templateName;
                metadata += record.pinned
                    ? L"　★ピン留め中"
                    : L"　ピン留めなし";
                SetWindowTextW(
                    m_metadataLabel,
                    metadata.c_str());
                SetWindowTextW(
                    m_textEdit,
                    record.text.c_str());

                std::wstring pathText = L"画像：";
                if (record.artworkPath.empty()) {
                    pathText += L"なし";
                } else {
                    pathText += record.artworkPath.wstring();
                    std::error_code artworkError;
                    if (!fs::is_regular_file(
                            record.artworkPath,
                            artworkError)) {
                        pathText += L"（ファイルが見つかりません）";
                    }
                }
                SetWindowTextW(
                    m_pathLabel,
                    pathText.c_str());
            } else {
                size_t selectedPinnedCount = 0;
                for (const int selectedIndex : selectedIndices) {
                    if (selectedIndex >= 0 &&
                        static_cast<size_t>(selectedIndex) <
                            m_records.size() &&
                        m_records[static_cast<size_t>(selectedIndex)]
                            .pinned) {
                        ++selectedPinnedCount;
                    }
                }

                std::wstring metadata =
                    std::to_wstring(selectionCount);
                metadata += L"件選択中（★";
                metadata +=
                    std::to_wstring(selectedPinnedCount);
                metadata += L"件）\r\n";
                metadata += L"文章コピー・ピン留め・削除を一括操作できます。";
                SetWindowTextW(
                    m_metadataLabel,
                    metadata.c_str());

                std::wstring combinedText;
                for (size_t i = 0; i < selectionCount; ++i) {
                    const auto& record =
                        m_records[
                            static_cast<size_t>(selectedIndices[i])];
                    if (!combinedText.empty()) {
                        combinedText += L"\r\n\r\n";
                    }
                    combinedText += record.text;
                }
                SetWindowTextW(
                    m_textEdit,
                    combinedText.c_str());
                SetWindowTextW(
                    m_pathLabel,
                    L"画像：複数選択中のため個別表示しません。");
            }

            const BOOL anySelected = selected ? TRUE : FALSE;
            const BOOL singleSelected =
                selectionCount == 1 ? TRUE : FALSE;

            EnableWindow(m_copyTextButton, anySelected);
            EnableWindow(m_deleteButton, anySelected);
            EnableWindow(m_pinButton, anySelected);
            EnableWindow(m_previewButton, singleSelected);

            bool allSelectedPinned = selected;
            for (const int selectedIndex : selectedIndices) {
                if (selectedIndex < 0 ||
                    static_cast<size_t>(selectedIndex) >=
                        m_records.size() ||
                    !m_records[static_cast<size_t>(selectedIndex)]
                        .pinned) {
                    allSelectedPinned = false;
                    break;
                }
            }
            SetWindowTextW(
                m_pinButton,
                allSelectedPinned
                    ? L"ピン留め解除"
                    : L"ピン留め");
            EnableWindow(m_openXButton, singleSelected);
            EnableWindow(m_openFolderButton, singleSelected);

            BOOL hasImage = FALSE;
            if (singleSelected) {
                std::error_code ec;
                const auto& artwork =
                    m_records[
                        static_cast<size_t>(selectedIndices.front())]
                        .artworkPath;
                hasImage =
                    !artwork.empty() && fs::is_regular_file(artwork, ec)
                        ? TRUE
                        : FALSE;
            }
            EnableWindow(m_copyImageButton, hasImage);
            EnableWindow(m_copyImageOpenXButton, hasImage);
            EnableWindow(
                m_clearButton,
                m_records.empty() ? FALSE : TRUE);
        }

        void set_status(const std::wstring& text) {
            SetWindowTextW(m_statusLabel, text.c_str());
        }

        void toggle_selected_pin() {
            const auto selected = selected_indices();
            if (selected.empty()) {
                set_status(
                    L"ピン留めする履歴を選択してください。");
                return;
            }

            bool allPinned = true;
            for (const int index : selected) {
                if (index < 0 ||
                    static_cast<size_t>(index) >=
                        m_records.size() ||
                    !m_records[static_cast<size_t>(index)]
                        .pinned) {
                    allPinned = false;
                    break;
                }
            }

            const bool newPinnedState = !allPinned;
            auto updatedRecords = m_records;
            for (const int index : selected) {
                if (index >= 0 &&
                    static_cast<size_t>(index) <
                        updatedRecords.size()) {
                    updatedRecords[static_cast<size_t>(index)]
                        .pinned = newPinnedState;
                }
            }

            std::vector<fs::path> removedArtwork;
            trim_history_records_preserving_pinned(
                updatedRecords,
                removedArtwork);

            if (!write_history_records(
                    m_outputDirectory,
                    updatedRecords)) {
                set_status(
                    L"ピン留め状態を保存できませんでした。");
                return;
            }

            m_records = std::move(updatedRecords);
            remove_unreferenced_trimmed_artwork(
                removedArtwork,
                m_records);
            rebuild_history_list();

            std::wstring status =
                std::to_wstring(selected.size());
            status += newPinnedState
                ? L"件をピン留めしました。"
                : L"件のピン留めを解除しました。";
            set_status(status);
        }

        void open_selected_in_preview() {
            const auto selected = selected_indices();
            if (selected.size() != 1) {
                set_status(
                    L"プレビューで開く履歴を1件だけ選択してください。");
                return;
            }

            const int index = selected.front();
            if (index < 0 ||
                static_cast<size_t>(index) >=
                    m_records.size()) {
                set_status(
                    L"選択した投稿履歴を読み込めませんでした。");
                return;
            }

            if (open_history_record_in_preview(
                    m_records[static_cast<size_t>(index)],
                    m_outputDirectory)) {
                set_status(
                    L"選択した投稿履歴をプレビューで開きました。");
            } else {
                set_status(
                    L"投稿プレビューを開けませんでした。");
            }
        }

        void copy_selected_text() {
            const auto selected = selected_indices();
            if (selected.empty()) return;

            std::wstring text;
            if (selected.size() == 1) {
                text = get_window_text_string(m_textEdit);
            } else {
                for (size_t i = 0; i < selected.size(); ++i) {
                    const auto& record =
                        m_records[static_cast<size_t>(selected[i])];
                    if (!text.empty()) {
                        text += L"\r\n\r\n";
                    }
                    text += record.text;
                }
            }

            const bool ok = copy_text_to_clipboard(text);
            set_status(
                ok
                    ? (selected.size() == 1
                        ? L"履歴の文章をクリップボードへコピーしました。"
                        : (std::to_wstring(selected.size()) +
                            L"件の文章をまとめてクリップボードへコピーしました。"))
                    : L"文章のコピーに失敗しました。");
        }

        void copy_selected_image() {
            const auto selected = selected_indices();
            if (selected.size() != 1) {
                set_status(L"画像のコピーは1件だけ選択してください。");
                return;
            }
            const int index = selected.front();

            const auto& path =
                m_records[
                    static_cast<size_t>(index)].artworkPath;
            std::error_code ec;
            if (path.empty() ||
                !fs::is_regular_file(path, ec)) {
                set_status(
                    L"履歴画像が見つかりません。");
                return;
            }

            const bool ok =
                copy_artwork_to_clipboard(path, m_wnd);
            set_status(
                ok
                    ? L"履歴の画像をクリップボードへコピーしました。"
                    : L"画像のコピーに失敗しました。");
        }

        bool confirm_x_length(const std::wstring& text) {
            const size_t weighted = x_weighted_length(text);
            if (weighted <= 280) return true;

            const std::wstring warning =
                L"X換算の目安が280を超えています（" +
                std::to_wstring(weighted) +
                L"）。そのままXの投稿画面を開きますか？";

            return MessageBoxW(
                m_wnd,
                warning.c_str(),
                L"NowPlaying Copy & Artwork",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
        }

        void open_selected_in_x(bool copyImageFirst) {
            const auto selected = selected_indices();
            if (selected.size() != 1) {
                set_status(L"Xで再投稿する場合は1件だけ選択してください。");
                return;
            }
            const int index = selected.front();

            const std::wstring text =
                get_window_text_string(m_textEdit);
            if (!confirm_x_length(text)) {
                set_status(L"X投稿画面を開く操作を中止しました。");
                return;
            }

            if (copyImageFirst) {
                const auto& path =
                    m_records[
                        static_cast<size_t>(index)].artworkPath;
                std::error_code ec;
                if (path.empty() ||
                    !fs::is_regular_file(path, ec)) {
                    set_status(
                        L"履歴画像が見つかりません。");
                    return;
                }
                if (!copy_artwork_to_clipboard(
                        path,
                        m_wnd)) {
                    set_status(
                        L"画像のコピーに失敗しました。");
                    return;
                }
            }

            const std::wstring url =
                L"https://x.com/intent/post?text=" +
                percent_encode_utf8(text);
            const HINSTANCE result = ShellExecuteW(
                m_wnd,
                L"open",
                url.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);

            const bool opened =
                reinterpret_cast<INT_PTR>(result) > 32;
            set_status(
                opened
                    ? (copyImageFirst
                        ? L"画像をコピーしてXの投稿画面を開きました。Ctrl＋Vで画像を貼り付けてください。"
                        : L"履歴の文章を入力した状態でXの投稿画面を開きました。")
                    : L"Xの投稿画面を開けませんでした。");
        }

        void open_selected_folder() {
            const auto selected = selected_indices();
            if (selected.size() != 1) {
                set_status(L"保存フォルダを開く場合は1件だけ選択してください。");
                return;
            }
            const int index = selected.front();

            const auto& record =
                m_records[static_cast<size_t>(index)];
            fs::path folder = record.outputDirectory;
            if (folder.empty() && !record.artworkPath.empty()) {
                folder = record.artworkPath.parent_path();
            }
            if (folder.empty()) folder = m_outputDirectory;

            ShellExecuteW(
                m_wnd,
                L"open",
                folder.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
        }

        void delete_selected_record() {
            auto selected = selected_indices();
            if (selected.empty()) return;

            const std::wstring confirmText =
                selected.size() == 1
                    ? L"選択した投稿履歴を削除しますか？"
                    : (std::to_wstring(selected.size()) +
                        L"件の投稿履歴を削除しますか？");

            if (MessageBoxW(
                    m_wnd,
                    confirmText.c_str(),
                    L"NowPlaying Copy & Artwork",
                    MB_YESNO | MB_ICONQUESTION |
                        MB_DEFBUTTON2) != IDYES) {
                return;
            }

            std::sort(selected.begin(), selected.end());
            std::vector<fs::path> removedArtworks;

            for (auto it = selected.rbegin(); it != selected.rend(); ++it) {
                const size_t recordIndex =
                    static_cast<size_t>(*it);
                if (recordIndex >= m_records.size()) {
                    continue;
                }

                const fs::path removedArtwork =
                    m_records[recordIndex].artworkPath;
                if (!removedArtwork.empty()) {
                    bool existsInVector = false;
                    for (const auto& savedPath : removedArtworks) {
                        if (savedPath == removedArtwork) {
                            existsInVector = true;
                            break;
                        }
                    }
                    if (!existsInVector) {
                        removedArtworks.push_back(removedArtwork);
                    }
                }

                m_records.erase(
                    m_records.begin() +
                    static_cast<std::ptrdiff_t>(recordIndex));
            }

            for (const auto& removedArtwork : removedArtworks) {
                if (!removedArtwork.empty() &&
                    removedArtwork.parent_path().filename() ==
                        L"nowplaying-history-artwork") {
                    bool stillUsed = false;
                    for (const auto& record : m_records) {
                        if (record.artworkPath == removedArtwork) {
                            stillUsed = true;
                            break;
                        }
                    }
                    if (!stillUsed) {
                        std::error_code removeError;
                        fs::remove(removedArtwork, removeError);
                    }
                }
            }

            if (!write_history_records(
                    m_outputDirectory,
                    m_records)) {
                set_status(L"履歴ファイルの更新に失敗しました。");
                return;
            }

            rebuild_history_list();
            set_status(
                selected.size() == 1
                    ? L"選択した投稿履歴を削除しました。"
                    : (std::to_wstring(selected.size()) +
                        L"件の投稿履歴を削除しました。"));
        }

        bool choose_history_backup_file(
            bool save,
            fs::path& selectedPath
        ) {
            wchar_t filePath[32768]{};
            if (save) {
                std::wstring defaultName =
                    L"NowPlaying-History-";
                const std::wstring timestamp =
                    current_local_timestamp();
                for (wchar_t value : timestamp) {
                    defaultName.push_back(
                        (value >= L'0' && value <= L'9')
                            ? value
                            : L'-');
                }
                defaultName += L".zip";
                std::wcsncpy(
                    filePath,
                    defaultName.c_str(),
                    std::size(filePath) - 1);
            }

            const wchar_t filter[] =
                L"NowPlaying履歴バックアップ (*.zip)\0*.zip\0"
                L"すべてのファイル (*.*)\0*.*\0\0";

            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = m_wnd;
            dialog.hInstance =
                core_api::get_my_instance();
            dialog.lpstrFilter = filter;
            dialog.nFilterIndex = 1;
            dialog.lpstrFile = filePath;
            dialog.nMaxFile =
                static_cast<DWORD>(std::size(filePath));
            dialog.lpstrInitialDir =
                m_outputDirectory.empty()
                    ? nullptr
                    : m_outputDirectory.c_str();
            dialog.lpstrDefExt = L"zip";
            dialog.Flags =
                OFN_EXPLORER |
                OFN_NOCHANGEDIR |
                OFN_PATHMUSTEXIST |
                (save
                    ? OFN_OVERWRITEPROMPT
                    : OFN_FILEMUSTEXIST);

            const BOOL accepted =
                save
                    ? GetSaveFileNameW(&dialog)
                    : GetOpenFileNameW(&dialog);
            if (!accepted) return false;

            selectedPath = fs::path(filePath);
            return true;
        }

        fs::path copy_artwork_to_backup(
            const fs::path& sourcePath,
            const fs::path& artworkDirectory,
            size_t recordIndex
        ) {
            std::error_code ec;
            if (sourcePath.empty() ||
                !fs::is_regular_file(sourcePath, ec)) {
                return {};
            }

            fs::create_directories(
                artworkDirectory,
                ec);
            if (ec) return {};

            std::wstring extension =
                lower(sourcePath.extension().wstring());
            if (extension.empty() ||
                extension.size() > 12) {
                extension = L".jpg";
            }

            std::wstring baseName =
                sourcePath.stem().wstring();
            if (baseName.empty()) {
                baseName =
                    L"artwork-" +
                    std::to_wstring(recordIndex + 1);
            }

            for (int suffix = 0;
                 suffix < 10000;
                 ++suffix) {
                std::wstring fileName = baseName;
                if (suffix > 0) {
                    fileName += L"-";
                    fileName +=
                        std::to_wstring(suffix + 1);
                }
                fileName += extension;

                const fs::path destination =
                    artworkDirectory / fileName;
                ec.clear();
                if (fs::exists(destination, ec)) {
                    continue;
                }

                ec.clear();
                if (fs::copy_file(
                        sourcePath,
                        destination,
                        fs::copy_options::none,
                        ec)) {
                    return fs::path(L"artwork") /
                        fileName;
                }
                return {};
            }

            return {};
        }

        bool history_record_is_duplicate(
            const nowplaying_history_record& left,
            const nowplaying_history_record& right
        ) const {
            return
                left.timestamp == right.timestamp &&
                left.title == right.title &&
                left.text == right.text;
        }

        bool history_artwork_is_referenced(
            const fs::path& artworkPath,
            const std::vector<nowplaying_history_record>& records
        ) const {
            if (artworkPath.empty()) return false;
            for (const auto& record : records) {
                if (record.artworkPath == artworkPath) {
                    return true;
                }
            }
            return false;
        }

        void remove_unused_history_artwork(
            const std::vector<fs::path>& candidates,
            const std::vector<nowplaying_history_record>& keptRecords
        ) {
            for (const auto& candidate : candidates) {
                if (candidate.empty() ||
                    candidate.parent_path().filename() !=
                        L"nowplaying-history-artwork" ||
                    history_artwork_is_referenced(
                        candidate,
                        keptRecords)) {
                    continue;
                }

                std::error_code removeError;
                fs::remove(candidate, removeError);
            }
        }

        void export_history_backup() {
            fs::path destinationZip;
            if (!choose_history_backup_file(
                    true,
                    destinationZip)) {
                return;
            }

            const fs::path stagingDirectory =
                create_unique_temp_directory(L"nph");
            if (stagingDirectory.empty()) {
                set_status(
                    L"バックアップ用の一時フォルダーを作成できませんでした。");
                return;
            }

            std::error_code ec;
            const fs::path artworkDirectory =
                stagingDirectory / L"artwork";
            std::vector<nowplaying_history_record>
                portableRecords = m_records;
            size_t missingArtworkCount = 0;

            for (size_t index = 0;
                 index < portableRecords.size();
                 ++index) {
                const fs::path portableArtwork =
                    copy_artwork_to_backup(
                        portableRecords[index].artworkPath,
                        artworkDirectory,
                        index);
                if (!portableRecords[index].artworkPath.empty() &&
                    portableArtwork.empty()) {
                    ++missingArtworkCount;
                }
                portableRecords[index].artworkPath =
                    portableArtwork;
                portableRecords[index].outputDirectory.clear();
            }

            if (!write_history_records(
                    stagingDirectory,
                    portableRecords)) {
                fs::remove_all(stagingDirectory, ec);
                set_status(
                    L"バックアップ用の履歴ファイルを作成できませんでした。");
                return;
            }

            std::ostringstream manifest;
            manifest <<
                "[NowPlayingHistoryBackup]\r\n";
            manifest <<
                "FileType=NowPlaying Copy & Artwork History Backup\r\n";
            manifest << "Version=1\r\n";
            manifest << "Created="
                << wide_to_utf8(
                    current_local_timestamp())
                << "\r\n";
            manifest << "RecordCount="
                << portableRecords.size()
                << "\r\n";
            manifest << "MissingArtworkCount="
                << missingArtworkCount
                << "\r\n";

            if (!write_utf8_text(
                    stagingDirectory /
                        L"nowplaying-history-backup.ini",
                    manifest.str())) {
                fs::remove_all(stagingDirectory, ec);
                set_status(
                    L"バックアップ情報ファイルを作成できませんでした。");
                return;
            }

            DWORD exitCode = 0;
            const bool created =
                create_zip_from_directory(
                    stagingDirectory,
                    destinationZip,
                    exitCode);
            fs::remove_all(stagingDirectory, ec);

            if (!created) {
                std::wstring error =
                    L"履歴バックアップの作成に失敗しました。";
                error += L" エラーコード：";
                error += std::to_wstring(exitCode);
                set_status(error);
                return;
            }

            std::wstring message =
                std::to_wstring(portableRecords.size());
            message +=
                L"件の投稿履歴を書き出しました。";
            if (missingArtworkCount > 0) {
                message += L"\r\n";
                message +=
                    std::to_wstring(missingArtworkCount);
                message +=
                    L"件の画像は元ファイルが見つからないため含まれていません。";
            }
            message += L"\r\n\r\n";
            message += destinationZip.wstring();

            MessageBoxW(
                m_wnd,
                message.c_str(),
                L"NowPlaying Copy & Artwork",
                MB_OK | MB_ICONINFORMATION);
            set_status(
                L"投稿履歴のバックアップを書き出しました。");
        }

        void import_history_backup() {
            fs::path sourceZip;
            if (!choose_history_backup_file(
                    false,
                    sourceZip)) {
                return;
            }

            const int importMode = MessageBoxW(
                m_wnd,
                L"履歴をどのように読み込みますか？\r\n\r\n"
                L"［はい］現在の履歴に追加\r\n"
                L"［いいえ］現在の履歴を置き換え\r\n"
                L"［キャンセル］読み込みを中止",
                L"NowPlaying Copy & Artwork",
                MB_YESNOCANCEL |
                    MB_ICONQUESTION |
                    MB_DEFBUTTON1);
            if (importMode == IDCANCEL) return;
            const bool append =
                importMode == IDYES;

            const fs::path extractionDirectory =
                create_unique_temp_directory(L"npi");
            if (extractionDirectory.empty()) {
                set_status(
                    L"読み込み用の一時フォルダーを作成できませんでした。");
                return;
            }

            std::error_code ec;
            DWORD exitCode = 0;
            if (!extract_zip_to_directory(
                    sourceZip,
                    extractionDirectory,
                    exitCode)) {
                fs::remove_all(extractionDirectory, ec);
                std::wstring error =
                    L"履歴バックアップを展開できませんでした。";
                error += L" エラーコード：";
                error += std::to_wstring(exitCode);
                set_status(error);
                return;
            }

            std::wstring manifestError;
            if (!validate_history_backup_manifest(
                    extractionDirectory /
                        L"nowplaying-history-backup.ini",
                    manifestError)) {
                fs::remove_all(extractionDirectory, ec);
                MessageBoxW(
                    m_wnd,
                    manifestError.c_str(),
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONERROR);
                return;
            }

            const fs::path extractedHistoryFile =
                history_file_path(extractionDirectory);
            if (!fs::is_regular_file(
                    extractedHistoryFile,
                    ec)) {
                fs::remove_all(extractionDirectory, ec);
                set_status(
                    L"バックアップ内に履歴ファイルがありません。");
                return;
            }

            const auto importedRecords =
                load_history_records(
                    extractionDirectory);
            std::vector<nowplaying_history_record>
                newRecords =
                    append
                        ? m_records
                        : std::vector<
                            nowplaying_history_record>{};
            std::vector<fs::path> cleanupCandidates;
            std::vector<fs::path> newlyArchivedPaths;
            size_t addedCount = 0;
            size_t duplicateCount = 0;
            size_t restoredPinCount = 0;
            size_t missingArtworkCount = 0;

            for (const auto& imported : importedRecords) {
                bool duplicate = false;
                for (auto& existing : newRecords) {
                    if (history_record_is_duplicate(
                            imported,
                            existing)) {
                        duplicate = true;
                        if (imported.pinned &&
                            !existing.pinned) {
                            existing.pinned = true;
                            ++restoredPinCount;
                        }
                        break;
                    }
                }
                if (duplicate) {
                    ++duplicateCount;
                    continue;
                }

                nowplaying_history_record record =
                    imported;
                record.outputDirectory =
                    m_outputDirectory;

                if (!record.artworkPath.empty()) {
                    if (safe_history_backup_relative_path(
                            record.artworkPath)) {
                        const fs::path sourceArtwork =
                            extractionDirectory /
                            record.artworkPath.lexically_normal();
                        const fs::path archived =
                            archive_history_artwork(
                                m_outputDirectory,
                                sourceArtwork,
                                record.timestamp);
                        if (!archived.empty()) {
                            record.artworkPath = archived;
                            newlyArchivedPaths.push_back(
                                archived);
                        } else {
                            record.artworkPath.clear();
                            ++missingArtworkCount;
                        }
                    } else {
                        record.artworkPath.clear();
                        ++missingArtworkCount;
                    }
                }

                newRecords.push_back(
                    std::move(record));
                ++addedCount;
            }

            std::stable_sort(
                newRecords.begin(),
                newRecords.end(),
                [](const nowplaying_history_record& left,
                   const nowplaying_history_record& right) {
                    return left.timestamp >
                        right.timestamp;
                });

            for (const auto& oldRecord : m_records) {
                if (!oldRecord.artworkPath.empty()) {
                    cleanupCandidates.push_back(
                        oldRecord.artworkPath);
                }
            }
            for (const auto& importedPath :
                 newlyArchivedPaths) {
                cleanupCandidates.push_back(
                    importedPath);
            }

            std::vector<fs::path> trimmedArtwork;
            trim_history_records_preserving_pinned(
                newRecords,
                trimmedArtwork);
            cleanupCandidates.insert(
                cleanupCandidates.end(),
                trimmedArtwork.begin(),
                trimmedArtwork.end());

            if (!write_history_records(
                    m_outputDirectory,
                    newRecords)) {
                remove_unused_history_artwork(
                    newlyArchivedPaths,
                    m_records);
                fs::remove_all(extractionDirectory, ec);
                set_status(
                    L"読み込んだ履歴を保存できませんでした。");
                return;
            }

            m_records = std::move(newRecords);
            remove_unused_history_artwork(
                cleanupCandidates,
                m_records);
            fs::remove_all(extractionDirectory, ec);
            rebuild_history_list();

            std::wstring message =
                append
                    ? L"投稿履歴を追加しました。"
                    : L"投稿履歴を置き換えました。";
            message += L"\r\n\r\n追加：";
            message += std::to_wstring(addedCount);
            message += L"件\r\n重複のため省略：";
            message += std::to_wstring(duplicateCount);
            message += L"件";
            if (restoredPinCount > 0) {
                message += L"\r\n重複履歴へピン留めを復元：";
                message +=
                    std::to_wstring(restoredPinCount);
                message += L"件";
            }
            if (missingArtworkCount > 0) {
                message += L"\r\n画像なし：";
                message +=
                    std::to_wstring(missingArtworkCount);
                message += L"件";
            }
            message += L"\r\n現在の保存件数：";
            message +=
                std::to_wstring(m_records.size());
            message += L"件";

            MessageBoxW(
                m_wnd,
                message.c_str(),
                L"NowPlaying Copy & Artwork",
                MB_OK | MB_ICONINFORMATION);
            set_status(
                L"投稿履歴のバックアップを読み込みました。");
        }

        void clear_records() {
            if (m_records.empty()) return;

            const size_t pinnedCount =
                static_cast<size_t>(std::count_if(
                    m_records.begin(),
                    m_records.end(),
                    [](const nowplaying_history_record& record) {
                        return record.pinned;
                    }));
            std::wstring confirmation =
                L"すべての投稿履歴を削除しますか？";
            if (pinnedCount > 0) {
                confirmation += L"\r\n\r\n★ピン留め中の履歴 ";
                confirmation += std::to_wstring(pinnedCount);
                confirmation += L"件も削除されます。";
            }

            if (MessageBoxW(
                    m_wnd,
                    confirmation.c_str(),
                    L"NowPlaying Copy & Artwork",
                    MB_YESNO | MB_ICONWARNING |
                        MB_DEFBUTTON2) != IDYES) {
                return;
            }

            m_records.clear();

            std::error_code removeError;
            fs::remove_all(
                m_outputDirectory /
                    L"nowplaying-history-artwork",
                removeError);

            if (!write_history_records(
                    m_outputDirectory,
                    m_records)) {
                set_status(L"履歴ファイルの更新に失敗しました。");
                return;
            }
            rebuild_history_list();
            set_status(L"投稿履歴をすべて削除しました。");
        }

        fs::path m_outputDirectory;
        std::vector<nowplaying_history_record> m_records;
        std::vector<size_t> m_visibleIndices;
        HWND m_wnd = nullptr;
        HWND m_listLabel = nullptr;
        HWND m_searchEdit = nullptr;
        HWND m_reloadButton = nullptr;
        HWND m_pinnedOnlyCheck = nullptr;
        HWND m_list = nullptr;
        HWND m_exportHistoryButton = nullptr;
        HWND m_importHistoryButton = nullptr;
        HWND m_detailLabel = nullptr;
        HWND m_previewButton = nullptr;
        HWND m_metadataLabel = nullptr;
        HWND m_textEdit = nullptr;
        HWND m_pathLabel = nullptr;
        HWND m_copyTextButton = nullptr;
        HWND m_copyImageButton = nullptr;
        HWND m_openXButton = nullptr;
        HWND m_copyImageOpenXButton = nullptr;
        HWND m_openFolderButton = nullptr;
        HWND m_pinButton = nullptr;
        HWND m_deleteButton = nullptr;
        HWND m_clearButton = nullptr;
        HWND m_closeButton = nullptr;
        HWND m_statusLabel = nullptr;
        fb2k::CCoreDarkModeHooks m_dark;
        bool m_creationComplete = false;
    };

    struct nowplaying_preview_data {
        metadb_handle_ptr track;
        std::wstring text;
        std::vector<std::wstring> templateNames;
        std::vector<std::wstring> templateFormats;
        int selectedTemplate = 0;
        fs::path outputDirectory;
        fs::path artworkPath;
        std::wstring artworkSource;
        std::wstring optimizationInfo;
        std::wstring trackTitle;
        std::wstring trackArtist;
        bool saveText = true;
        bool showCompletion = true;
        bool allowTemplateSwitch = true;
        std::wstring initialStatus;
    };

    class nowplaying_preview_window {
    public:
        static bool open(nowplaying_preview_data data) {
            auto* instance = new nowplaying_preview_window(std::move(data));
            if (!instance->create()) {
                delete instance;
                return false;
            }
            return true;
        }

    private:
        enum : int {
            IDC_PREVIEW_TEMPLATE = 2101,
            IDC_PREVIEW_ARTWORK = 2102,
            IDC_PREVIEW_TEXT = 2103,
            IDC_PREVIEW_COUNT = 2104,
            IDC_PREVIEW_SOURCE = 2105,
            IDC_PREVIEW_STATUS = 2106,
            IDC_PREVIEW_COPY = 2107,
            IDC_PREVIEW_COPY_IMAGE = 2108,
            IDC_PREVIEW_OPEN_FOLDER = 2109,
            IDC_PREVIEW_COPY_CLOSE = 2110,
            IDC_PREVIEW_CLOSE = 2111,
            IDC_PREVIEW_OPEN_X = 2112,
            IDC_PREVIEW_COPY_IMAGE_OPEN_X = 2113,
            IDC_PREVIEW_HISTORY = 2114
        };

        explicit nowplaying_preview_window(nowplaying_preview_data data)
            : m_data(std::move(data)) {
        }

        ~nowplaying_preview_window() {
            if (m_bitmap != nullptr) DeleteObject(m_bitmap);
        }

        static const wchar_t* window_class_name() {
            return L"FooNowPlayingPostPreviewWindow";
        }

        static void register_window_class() {
            static std::once_flag once;
            std::call_once(once, [] {
                WNDCLASSEXW windowClass{};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.lpfnWndProc = window_proc;
                windowClass.hInstance = core_api::get_my_instance();
                windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
                windowClass.hbrBackground = nullptr;
                windowClass.lpszClassName = window_class_name();
                RegisterClassExW(&windowClass);
            });
        }

        bool create() {
            register_window_class();

            constexpr int clientWidth = 760;
            constexpr int clientHeight = 520;
            RECT windowRect{ 0, 0, clientWidth, clientHeight };
            AdjustWindowRectEx(
                &windowRect,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                FALSE,
                WS_EX_DLGMODALFRAME);

            const int width = windowRect.right - windowRect.left;
            const int height = windowRect.bottom - windowRect.top;
            HWND parent = core_api::get_main_window();
            RECT parentRect{};
            GetWindowRect(parent, &parentRect);
            const int x = parentRect.left +
                ((parentRect.right - parentRect.left) - width) / 2;
            const int y = parentRect.top +
                ((parentRect.bottom - parentRect.top) - height) / 2;

            m_wnd = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                window_class_name(),
                L"NowPlaying投稿プレビュー",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                x, y, width, height,
                parent, nullptr, core_api::get_my_instance(), this);
            if (m_wnd == nullptr) return false;
            m_creationComplete = true;

            create_controls();
            m_dark.AddDialogWithControls(m_wnd);
            InvalidateRect(m_wnd, nullptr, TRUE);
            if (!m_data.artworkPath.empty()) {
                m_bitmap = load_bitmap_scaled(m_data.artworkPath, 220, 220);
            }
            layout_controls();
            update_character_count();

            ShowWindow(m_wnd, SW_SHOWNORMAL);
            UpdateWindow(m_wnd);
            SetForegroundWindow(m_wnd);
            SetFocus(m_textEdit);
            SendMessageW(
                m_textEdit, EM_SETSEL,
                static_cast<WPARAM>(m_data.text.size()),
                static_cast<LPARAM>(m_data.text.size()));
            return true;
        }

        static LRESULT CALLBACK window_proc(
            HWND wnd, UINT message, WPARAM wp, LPARAM lp) {
            nowplaying_preview_window* self =
                reinterpret_cast<nowplaying_preview_window*>(
                    GetWindowLongPtrW(wnd, GWLP_USERDATA));

            if (message == WM_NCCREATE) {
                const auto* create = reinterpret_cast<CREATESTRUCTW*>(lp);
                self = static_cast<nowplaying_preview_window*>(
                    create->lpCreateParams);
                self->m_wnd = wnd;
                SetWindowLongPtrW(
                    wnd, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }

            if (self == nullptr) {
                return DefWindowProcW(wnd, message, wp, lp);
            }

            const LRESULT result = self->on_message(wnd, message, wp, lp);
            if (message == WM_NCDESTROY) {
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                self->m_wnd = nullptr;
                if (self->m_creationComplete) delete self;
            }
            return result;
        }

        LRESULT on_message(HWND wnd, UINT message, WPARAM wp, LPARAM lp) {
            switch (message) {
            case WM_ERASEBKGND:
                return paint_nowplaying_page_background(
                    wnd, reinterpret_cast<HDC>(wp),
                    static_cast<bool>(m_dark));
            case WM_COMMAND: {
                const int id = LOWORD(wp);
                const int notification = HIWORD(wp);

                if (id == IDC_PREVIEW_TEMPLATE &&
                    notification == CBN_SELCHANGE) {
                    switch_template();
                    return 0;
                }
                if (id == IDC_PREVIEW_TEXT && notification == EN_CHANGE) {
                    update_character_count();
                    return 0;
                }
                if (id == IDC_PREVIEW_COPY && notification == BN_CLICKED) {
                    copy_and_save(false);
                    return 0;
                }
                if (id == IDC_PREVIEW_COPY_IMAGE && notification == BN_CLICKED) {
                    copy_image();
                    return 0;
                }
                if (id == IDC_PREVIEW_OPEN_X && notification == BN_CLICKED) {
                    open_x_post(false);
                    return 0;
                }
                if (id == IDC_PREVIEW_COPY_IMAGE_OPEN_X &&
                    notification == BN_CLICKED) {
                    open_x_post(true);
                    return 0;
                }
                if (id == IDC_PREVIEW_OPEN_FOLDER && notification == BN_CLICKED) {
                    ShellExecuteW(
                        wnd, L"open", m_data.outputDirectory.c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                if (id == IDC_PREVIEW_HISTORY && notification == BN_CLICKED) {
                    nowplaying_history_window::open(m_data.outputDirectory);
                    return 0;
                }
                if (id == IDC_PREVIEW_COPY_CLOSE && notification == BN_CLICKED) {
                    copy_and_save(true);
                    return 0;
                }
                if (id == IDC_PREVIEW_CLOSE && notification == BN_CLICKED) {
                    DestroyWindow(wnd);
                    return 0;
                }
                break;
            }
            case WM_DRAWITEM:
                if (wp == IDC_PREVIEW_ARTWORK) {
                    draw_artwork(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
                    return TRUE;
                }
                break;
            case WM_CLOSE:
                DestroyWindow(wnd);
                return 0;
            case WM_SIZE:
                layout_controls();
                return 0;
            }
            return DefWindowProcW(wnd, message, wp, lp);
        }

        HWND create_control(
            DWORD exStyle, const wchar_t* className,
            const wchar_t* text, DWORD style, int id) {
            HWND control = CreateWindowExW(
                exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                0, 0, 0, 0, m_wnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                core_api::get_my_instance(), nullptr);
            if (control != nullptr) {
                SendMessageW(
                    control, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                    TRUE);
            }
            return control;
        }

        void create_controls() {
            m_templateLabel = create_control(
                0, L"STATIC", L"テンプレート", 0, 0);
            m_templateCombo = create_control(
                0, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                IDC_PREVIEW_TEMPLATE);
            for (const auto& name : m_data.templateNames) {
                SendMessageW(
                    m_templateCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(name.c_str()));
            }
            if (m_data.templateNames.empty()) {
                m_data.templateNames.push_back(L"テンプレート");
                m_data.templateFormats.push_back(
                    utf8_to_wide(default_post_format));
            }
            if (m_data.selectedTemplate < 0 ||
                m_data.selectedTemplate >=
                    static_cast<int>(m_data.templateNames.size())) {
                m_data.selectedTemplate = 0;
            }
            SendMessageW(
                m_templateCombo, CB_SETCURSEL,
                m_data.selectedTemplate, 0);
            if (!m_data.allowTemplateSwitch) {
                EnableWindow(m_templateCombo, FALSE);
                SetWindowTextW(
                    m_templateLabel,
                    L"使用テンプレート");
            }

            m_artworkControl = create_control(
                0, L"STATIC", L"", SS_OWNERDRAW | WS_BORDER,
                IDC_PREVIEW_ARTWORK);
            m_sourceLabel = create_control(
                0, L"STATIC", L"", SS_CENTER, IDC_PREVIEW_SOURCE);
            m_textLabel = create_control(
                0, L"STATIC", L"投稿文（その場で編集できます）", 0, 0);
            m_textEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", m_data.text.c_str(),
                WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                    ES_WANTRETURN | WS_VSCROLL,
                IDC_PREVIEW_TEXT);
            m_countLabel = create_control(
                0, L"STATIC", L"", SS_RIGHT, IDC_PREVIEW_COUNT);
            m_copyButton = create_control(
                0, L"BUTTON", L"文章をコピー",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_COPY);
            m_copyImageButton = create_control(
                0, L"BUTTON", L"画像をコピー",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_COPY_IMAGE);
            m_openXButton = create_control(
                0, L"BUTTON", L"Xで投稿",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_OPEN_X);
            m_copyImageOpenXButton = create_control(
                0, L"BUTTON", L"画像をコピーしてXを開く",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_PREVIEW_COPY_IMAGE_OPEN_X);
            m_openFolderButton = create_control(
                0, L"BUTTON", L"保存フォルダを開く",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_OPEN_FOLDER);
            m_historyButton = create_control(
                0, L"BUTTON", L"投稿履歴",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_HISTORY);
            m_copyCloseButton = create_control(
                0, L"BUTTON", L"コピーして閉じる",
                WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_PREVIEW_COPY_CLOSE);
            m_closeButton = create_control(
                0, L"BUTTON", L"閉じる",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_PREVIEW_CLOSE);
            const std::wstring initialStatus =
                m_data.initialStatus.empty()
                    ? L"文章を確認・編集してからコピーしてください。"
                    : m_data.initialStatus;
            m_statusLabel = create_control(
                0, L"STATIC",
                initialStatus.c_str(),
                0, IDC_PREVIEW_STATUS);

            std::wstring sourceText = L"アートワーク：";
            if (m_data.artworkPath.empty()) {
                sourceText += m_data.artworkSource.empty()
                    ? L"なし"
                    : m_data.artworkSource;
            } else {
                sourceText += m_data.artworkSource.empty()
                    ? L"あり"
                    : m_data.artworkSource;
            }
            if (!m_data.optimizationInfo.empty()) {
                sourceText += L"\r\n";
                sourceText += m_data.optimizationInfo;
            }
            SetWindowTextW(m_sourceLabel, sourceText.c_str());
            const BOOL hasArtwork =
                m_data.artworkPath.empty() ? FALSE : TRUE;
            EnableWindow(m_copyImageButton, hasArtwork);
            EnableWindow(m_copyImageOpenXButton, hasArtwork);
        }

        void layout_controls() {
            if (m_wnd == nullptr || m_artworkControl == nullptr) return;

            RECT client{};
            GetClientRect(m_wnd, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            const int margin = 16;
            const int imageSize = 220;
            const int gap = 20;
            const int rightX = margin + imageSize + gap;
            const int rightWidth = width - rightX - margin;

            MoveWindow(m_artworkControl, margin, margin, imageSize, imageSize, TRUE);
            MoveWindow(
                m_sourceLabel, margin, margin + imageSize + 8,
                imageSize, 88, TRUE);

            MoveWindow(m_templateLabel, rightX, margin, 82, 24, TRUE);
            MoveWindow(
                m_templateCombo, rightX + 84, margin - 2,
                rightWidth - 84, 200, TRUE);
            MoveWindow(m_textLabel, rightX, margin + 38, rightWidth, 22, TRUE);
            MoveWindow(
                m_textEdit, rightX, margin + 63,
                rightWidth, 185, TRUE);
            MoveWindow(
                m_countLabel, rightX, margin + 253,
                rightWidth, 22, TRUE);

            const int buttonHeight = 28;
            const int buttonGap = 7;
            const int xButtonY = height - 128;
            const int normalButtonY = height - 92;

            MoveWindow(
                m_openXButton, margin, xButtonY,
                118, buttonHeight, TRUE);
            MoveWindow(
                m_copyImageOpenXButton,
                margin + 118 + buttonGap, xButtonY,
                206, buttonHeight, TRUE);

            int x = margin;
            MoveWindow(
                m_copyButton, x, normalButtonY,
                108, buttonHeight, TRUE);
            x += 108 + buttonGap;
            MoveWindow(
                m_copyImageButton, x, normalButtonY,
                108, buttonHeight, TRUE);
            x += 108 + buttonGap;
            MoveWindow(
                m_openFolderButton, x, normalButtonY,
                136, buttonHeight, TRUE);
            x += 136 + buttonGap;
            MoveWindow(
                m_historyButton, x, normalButtonY,
                96, buttonHeight, TRUE);
            MoveWindow(
                m_copyCloseButton,
                width - margin - 126 - buttonGap - 82,
                normalButtonY, 126, buttonHeight, TRUE);
            MoveWindow(
                m_closeButton, width - margin - 82,
                normalButtonY, 82, buttonHeight, TRUE);
            MoveWindow(
                m_statusLabel, margin, height - 48,
                width - margin * 2, 28, TRUE);
        }

        void draw_artwork(DRAWITEMSTRUCT* item) {
            if (item == nullptr) return;
            FillRect(
                item->hDC, &item->rcItem,
                artwork_background_brush(static_cast<bool>(m_dark)));

            if (m_bitmap == nullptr) {
                RECT textRect = item->rcItem;
                SetBkMode(item->hDC, TRANSPARENT);
                SetTextColor(item->hDC, artwork_placeholder_text_color(static_cast<bool>(m_dark)));
                DrawTextW(
                    item->hDC, L"アートワークなし", -1, &textRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                return;
            }

            BITMAP bitmapInfo{};
            GetObjectW(m_bitmap, sizeof(bitmapInfo), &bitmapInfo);
            const int availableWidth = item->rcItem.right - item->rcItem.left;
            const int availableHeight = item->rcItem.bottom - item->rcItem.top;
            const int x = item->rcItem.left +
                (availableWidth - bitmapInfo.bmWidth) / 2;
            const int y = item->rcItem.top +
                (availableHeight - bitmapInfo.bmHeight) / 2;

            HDC memory = CreateCompatibleDC(item->hDC);
            HGDIOBJ old = SelectObject(memory, m_bitmap);
            BitBlt(
                item->hDC, x, y,
                bitmapInfo.bmWidth, bitmapInfo.bmHeight,
                memory, 0, 0, SRCCOPY);
            SelectObject(memory, old);
            DeleteDC(memory);
        }

        void switch_template() {
            if (!m_data.allowTemplateSwitch ||
                m_data.track.is_empty()) {
                SetWindowTextW(
                    m_statusLabel,
                    L"履歴から開いた投稿文ではテンプレートを切り替えられません。");
                return;
            }

            const LRESULT selected = SendMessageW(
                m_templateCombo, CB_GETCURSEL, 0, 0);
            if (selected == CB_ERR) return;
            const int index = static_cast<int>(selected);
            if (index < 0 ||
                index >= static_cast<int>(m_data.templateFormats.size())) {
                return;
            }

            m_data.selectedTemplate = index;
            set_selected_template_index(index);
            const std::wstring generated = format_track_message(
                m_data.track, m_data.templateFormats[static_cast<size_t>(index)]);
            SetWindowTextW(m_textEdit, generated.c_str());
            SetWindowTextW(
                m_statusLabel,
                L"テンプレートを切り替えました。編集内容は新しい書式で置き換わりました。");
            SetFocus(m_textEdit);
            SendMessageW(
                m_textEdit, EM_SETSEL,
                static_cast<WPARAM>(generated.size()),
                static_cast<LPARAM>(generated.size()));
        }

        void update_character_count() {
            if (m_textEdit == nullptr || m_countLabel == nullptr) return;
            const std::wstring text = get_window_text_string(m_textEdit);
            const size_t count = unicode_character_count(text);
            const size_t weighted = x_weighted_length(text);
            std::wstring label = L"文字数：" + std::to_wstring(count) +
                L"　X換算目安：" + std::to_wstring(weighted) + L" / 280";
            if (weighted > 280) label += L"（超過）";
            SetWindowTextW(m_countLabel, label.c_str());
        }

        bool confirm_x_length(const std::wstring& text) {
            const size_t weighted = x_weighted_length(text);
            if (weighted <= 280) return true;

            const std::wstring warning =
                L"X換算の目安が280を超えています（" +
                std::to_wstring(weighted) +
                L"）。そのままXの投稿画面を開きますか？";
            return MessageBoxW(
                m_wnd,
                warning.c_str(),
                L"NowPlaying Copy & Artwork",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
        }

        bool save_edited_text(const std::wstring& text) {
            if (!m_data.saveText) return true;
            return write_utf8_text(
                m_data.outputDirectory / L"nowplaying.txt",
                wide_to_utf8(text));
        }

        void open_x_post(bool copyImageFirst) {
            const std::wstring text = get_window_text_string(m_textEdit);
            if (!confirm_x_length(text)) {
                SetWindowTextW(m_statusLabel, L"X投稿画面を開く操作を中止しました。");
                return;
            }

            if (copyImageFirst) {
                if (m_data.artworkPath.empty()) {
                    SetWindowTextW(
                        m_statusLabel,
                        L"コピーできるアートワークがありません。");
                    return;
                }
                if (!copy_artwork_to_clipboard(m_data.artworkPath, m_wnd)) {
                    SetWindowTextW(
                        m_statusLabel,
                        L"画像のクリップボードコピーに失敗しました。");
                    show_message(
                        L"画像のクリップボードコピーに失敗しました。",
                        MB_ICONWARNING);
                    return;
                }
            }

            const bool textOK = save_edited_text(text);
            const std::wstring url =
                L"https://x.com/intent/post?text=" +
                percent_encode_utf8(text);
            const HINSTANCE result = ShellExecuteW(
                m_wnd, L"open", url.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL);
            const bool opened =
                reinterpret_cast<INT_PTR>(result) > 32;

            bool historyOK = true;
            if (opened) {
                nowplaying_history_record history;
                history.timestamp = current_local_timestamp();
                history.title = m_data.trackTitle;
                history.artist = m_data.trackArtist;
                const size_t templateIndex =
                    static_cast<size_t>(m_data.selectedTemplate);
                if (templateIndex < m_data.templateNames.size()) {
                    history.templateName =
                        m_data.templateNames[templateIndex];
                }
                history.text = text;
                history.artworkPath = m_data.artworkPath;
                history.outputDirectory = m_data.outputDirectory;
                historyOK = append_history_record(
                    m_data.outputDirectory,
                    std::move(history));
            }

            std::wstring status;
            if (opened) {
                status = copyImageFirst ?
                    L"画像をコピーし、Xの投稿画面を開きました。投稿画面でCtrl＋Vを押してください。" :
                    L"文章を入力した状態でXの投稿画面を開きました。";
                if (historyOK) {
                    status += L" 投稿履歴へ保存しました。";
                } else {
                    status += L" 投稿履歴の保存には失敗しました。";
                }
                if (m_data.saveText && !textOK) {
                    status += L" nowplaying.txtの保存には失敗しました。";
                }
            } else {
                status = L"Xの投稿画面を開けませんでした。";
            }
            SetWindowTextW(m_statusLabel, status.c_str());

            if (!opened || !historyOK || (m_data.saveText && !textOK)) {
                show_message(status, MB_ICONWARNING);
            }
        }

        void copy_image() {
            if (m_data.artworkPath.empty()) {
                SetWindowTextW(m_statusLabel, L"コピーできるアートワークがありません。");
                return;
            }
            const bool ok = copy_artwork_to_clipboard(
                m_data.artworkPath, m_wnd);
            SetWindowTextW(
                m_statusLabel,
                ok ?
                    L"画像をクリップボードへコピーしました。文章を貼り付けた後に画像を貼り付けてください。" :
                    L"画像のクリップボードコピーに失敗しました。");
            if (m_data.showCompletion || !ok) {
                show_message(
                    ok ? L"画像をクリップボードへコピーしました。" :
                         L"画像のクリップボードコピーに失敗しました。",
                    ok ? MB_ICONINFORMATION : MB_ICONWARNING);
            }
        }

        void copy_and_save(bool closeAfter) {
            const std::wstring text = get_window_text_string(m_textEdit);
            const bool clipboardOK = copy_text_to_clipboard(text);
            bool textOK = true;
            if (m_data.saveText) {
                textOK = write_utf8_text(
                    m_data.outputDirectory / L"nowplaying.txt",
                    wide_to_utf8(text));
            }

            std::wstring status = clipboardOK ?
                L"文章をクリップボードへコピーしました。" :
                L"クリップボードへのコピーに失敗しました。";
            if (m_data.saveText) {
                status += textOK ?
                    L" nowplaying.txtも更新しました。" :
                    L" nowplaying.txtの保存に失敗しました。";
            }
            SetWindowTextW(m_statusLabel, status.c_str());

            const bool hasError = !clipboardOK || !textOK;
            if (m_data.showCompletion || hasError) {
                show_message(
                    status,
                    hasError ? MB_ICONWARNING : MB_ICONINFORMATION);
            }
            if (closeAfter && !hasError) DestroyWindow(m_wnd);
        }

        nowplaying_preview_data m_data;
        HWND m_wnd = nullptr;
        HWND m_templateLabel = nullptr;
        HWND m_templateCombo = nullptr;
        HWND m_artworkControl = nullptr;
        HWND m_sourceLabel = nullptr;
        HWND m_textLabel = nullptr;
        HWND m_textEdit = nullptr;
        HWND m_countLabel = nullptr;
        HWND m_copyButton = nullptr;
        HWND m_copyImageButton = nullptr;
        HWND m_openXButton = nullptr;
        HWND m_copyImageOpenXButton = nullptr;
        HWND m_openFolderButton = nullptr;
        HWND m_historyButton = nullptr;
        HWND m_copyCloseButton = nullptr;
        HWND m_closeButton = nullptr;
        HWND m_statusLabel = nullptr;
        HBITMAP m_bitmap = nullptr;
        fb2k::CCoreDarkModeHooks m_dark;
        bool m_creationComplete = false;
    };

    bool open_history_record_in_preview(
        const nowplaying_history_record& record,
        const fs::path& outputDirectory
    ) {
        nowplaying_preview_data previewData;
        previewData.text = record.text;
        previewData.outputDirectory =
            outputDirectory.empty()
                ? record.outputDirectory
                : outputDirectory;
        previewData.trackTitle = record.title;
        previewData.trackArtist = record.artist;
        previewData.saveText = g_cfg_save_text.get();
        previewData.showCompletion =
            g_cfg_show_completion.get();
        previewData.allowTemplateSwitch = false;
        previewData.initialStatus =
            L"投稿履歴を読み込みました。文章を編集して再利用できます。";

        const auto configuredTemplates =
            configured_template_definitions();
        bool matchingTemplateFound = false;
        for (size_t index = 0;
             index < configuredTemplates.size();
             ++index) {
            previewData.templateNames.push_back(
                configuredTemplates[index].name);
            previewData.templateFormats.push_back(
                configuredTemplates[index].format);

            if (!matchingTemplateFound &&
                !record.templateName.empty() &&
                configuredTemplates[index].name ==
                    record.templateName) {
                previewData.selectedTemplate =
                    static_cast<int>(index);
                matchingTemplateFound = true;
            }
        }

        if (previewData.templateNames.empty()) {
            previewData.templateNames.push_back(
                L"（テンプレート不明・履歴）");
            previewData.templateFormats.push_back(record.text);
        } else if (!matchingTemplateFound) {
            previewData.selectedTemplate = 0;
            previewData.templateNames[0] =
                record.templateName.empty()
                    ? L"（テンプレート不明・履歴）"
                    : record.templateName + L"（履歴）";
        }

        std::error_code artworkError;
        if (!record.artworkPath.empty() &&
            fs::is_regular_file(
                record.artworkPath,
                artworkError)) {
            previewData.artworkPath =
                record.artworkPath;
            previewData.artworkSource =
                L"投稿履歴の保存画像";
            previewData.optimizationInfo =
                describe_artwork_file(
                    record.artworkPath,
                    L"履歴画像");
        } else if (!record.artworkPath.empty()) {
            previewData.artworkSource =
                L"履歴画像が見つかりません";
        } else {
            previewData.artworkSource =
                L"なし";
        }

        return nowplaying_preview_window::open(
            std::move(previewData));
    }

    void execute_nowplaying() {
        metadb_handle_ptr track;
        if (!static_api_ptr_t<playback_control>()->get_now_playing(track) ||
            track.is_empty()) {
            show_message(L"現在再生中の曲がありません。", MB_ICONWARNING);
            return;
        }

        const auto configuredTemplates =
            configured_template_definitions();
        int templateIndex = selected_template_index();
        std::vector<std::wstring> templateNames;
        std::vector<std::wstring> templateFormats;
        templateNames.reserve(configuredTemplates.size());
        templateFormats.reserve(configuredTemplates.size());
        for (const auto& item : configuredTemplates) {
            templateNames.push_back(item.name);
            templateFormats.push_back(item.format);
        }
        if (templateFormats.empty()) {
            templateNames.push_back(L"シンプル");
            templateFormats.push_back(
                utf8_to_wide(default_post_format));
            templateIndex = 0;
        }

        const std::wstring message = format_track_message(
            track, templateFormats[static_cast<size_t>(templateIndex)]);
        const std::wstring trackTitle =
            format_track_message(track, L"%title%");
        const std::wstring trackArtist =
            format_track_message(track, L"%artist%");
        const bool saveText = g_cfg_save_text.get();
        const bool saveArtwork = g_cfg_save_artwork.get();
        const bool searchZip = g_cfg_search_zip.get();
        const bool showCompletion = g_cfg_show_completion.get();
        const bool showPreview = g_cfg_show_preview.get();
        const artwork_post_settings artworkPostSettings = configured_artwork_post_settings();

        const fs::path outDir = output_directory();
        const std::wstring rawPath = utf8_to_wide(track->get_path());

        fs::path exported;
        bool artOK = false;
        bool embeddedOK = false;
        bool zipDetected = false;
        fs::path parsedZip;
        std::wstring parsedInner;
        DWORD processExitCode = 0;
        std::wstring embeddedError;
        std::wstring artworkSource = L"なし";
        fs::path artworkForPost;
        std::wstring optimizationInfo;

        if (saveArtwork) {
            embeddedOK = export_embedded_cover(
                track, outDir, exported, embeddedError);
            artOK = embeddedOK;
            if (embeddedOK) artworkSource = L"埋め込みフロントカバー";

            if (!artOK) {
                if (parse_zip_location(rawPath, parsedZip, parsedInner)) {
                    zipDetected = true;
                    if (searchZip) {
                        artOK = export_zip_cover(
                            parsedZip, parsedInner, outDir,
                            exported, processExitCode);
                        if (artOK) artworkSource = L"ZIP内画像";
                    }
                } else {
                    const std::wstring localPath =
                        normalize_local_file_path(rawPath);
                    artOK = export_folder_cover(
                        fs::path(localPath), outDir, exported);
                    if (artOK) artworkSource = L"同じフォルダの画像";
                }
            }
        }

        if (artOK) {
            artworkForPost = exported;
            if (artworkPostSettings.enabled) {
                fs::path optimizedPath;
                std::wstring optimizedInfo;
                if (optimize_artwork_for_post(
                        exported,
                        outDir,
                        artworkPostSettings,
                        optimizedPath,
                        optimizedInfo)) {
                    artworkForPost = optimizedPath;
                    optimizationInfo = optimizedInfo;
                } else {
                    optimizationInfo = L"最適化に失敗したため元画像を使用します。\r\n";
                    optimizationInfo += describe_artwork_file(exported, L"元画像");
                }
            } else {
                optimizationInfo = L"最適化：オフ\r\n";
                optimizationInfo += describe_artwork_file(exported, L"元画像");
            }
        }

        std::wstring debug;
        debug += L"Component version: 1.1.3\r\n";
        debug += L"Raw path: " + rawPath + L"\r\n";
        debug += L"Template index: " +
            std::to_wstring(templateIndex + 1) + L"\r\n";
        debug += L"Embedded artwork success: " +
            std::wstring(embeddedOK ? L"yes" : L"no") + L"\r\n";
        debug += L"Embedded artwork detail: " + embeddedError + L"\r\n";
        debug += L"ZIP detected: " +
            std::wstring(zipDetected ? L"yes" : L"no") + L"\r\n";
        if (zipDetected) {
            debug += L"ZIP path: " + parsedZip.wstring() + L"\r\n";
            debug += L"Inner path: " + parsedInner + L"\r\n";
            debug += L"PowerShell exit code: " +
                std::to_wstring(processExitCode) + L"\r\n";
        }
        debug += L"Artwork source: " + artworkSource + L"\r\n";
        debug += L"Artwork success: " +
            std::wstring(artOK ? L"yes" : L"no") + L"\r\n";
        debug += L"Optimized artwork: " + (artworkForPost.empty() ? std::wstring(L"no") : artworkForPost.wstring()) + L"\r\n";
        debug += L"Optimization info: " + optimizationInfo + L"\r\n";

        const fs::path debugPath = outDir / L"nowplaying-debug.txt";
        if (saveArtwork && !artOK) {
            write_utf8_text(debugPath, wide_to_utf8(debug));
        }

        if (showPreview) {
            nowplaying_preview_data previewData;
            previewData.track = track;
            previewData.text = message;
            previewData.templateNames = templateNames;
            previewData.templateFormats = templateFormats;
            previewData.selectedTemplate = templateIndex;
            previewData.outputDirectory = outDir;
            if (artOK) {
                previewData.artworkPath = artworkForPost.empty() ? exported : artworkForPost;
                previewData.artworkSource = artworkSource;
                previewData.optimizationInfo = optimizationInfo;
            }
            previewData.trackTitle = trackTitle;
            previewData.trackArtist = trackArtist;
            previewData.saveText = saveText;
            previewData.showCompletion = showCompletion;
            if (nowplaying_preview_window::open(std::move(previewData))) return;
        }

        const bool clipboardOK = copy_text_to_clipboard(message);
        const bool textOK = !saveText || write_utf8_text(
            outDir / L"nowplaying.txt", wide_to_utf8(message));

        std::wstring result;
        result += clipboardOK ?
            L"NowPlaying文をクリップボードへコピーしました。" :
            L"クリップボードへのコピーに失敗しました。";
        if (saveText) {
            if (textOK) {
                result += L"\r\n\r\nテキスト：\r\n";
                result += (outDir / L"nowplaying.txt").wstring();
            } else {
                result += L"\r\n\r\nテキストファイルを保存できませんでした。";
            }
        }
        if (saveArtwork) {
            if (artOK) {
                result += L"\r\n\r\nアートワーク（";
                result += artworkSource;
                result += L"）：\r\n";
                result += exported.wstring();
                if (!artworkForPost.empty() && artworkForPost != exported) {
                    result += L"\r\n\r\n投稿用アートワーク：\r\n";
                    result += artworkForPost.wstring();
                    if (!optimizationInfo.empty()) {
                        result += L"\r\n";
                        result += optimizationInfo;
                    }
                }
            } else {
                result += L"\r\n\r\nアートワークは見つかりませんでした。";
                result += L"\r\n\r\n診断ログ：\r\n";
                result += debugPath.wstring();
            }
        }
        const bool hasError = !clipboardOK || (saveText && !textOK);
        if (showCompletion || hasError) {
            show_message(
                result, hasError ? MB_ICONWARNING : MB_ICONINFORMATION);
        }
    }

    class nowplaying_help_window {
    public:
        static bool open() {
            auto* instance = new nowplaying_help_window();
            if (!instance->create()) {
                delete instance;
                return false;
            }
            return true;
        }

    private:
        enum : int {
            IDC_HELP_TEXT = 4101,
            IDC_HELP_COPY = 4102,
            IDC_HELP_CLOSE = 4103
        };

        static const wchar_t* window_class_name() {
            return L"FooNowPlayingHelpWindow";
        }

        static std::wstring help_text() {
            return LR"HELP(NowPlaying Copy & Artwork ヘルプ
バージョン 1.1.2

【1. 基本的な使い方】
1. foobar2000で曲を再生します。
2. ［ファイル］→［NowPlaying投稿を作成］を選びます。
3. 投稿プレビューで文章と画像を確認・編集します。
4. ［文章をコピー］または［画像をコピーしてXを開く］を使います。
5. Xの画面で画像が未添付の場合は Ctrl＋V で貼り付けます。

設定で「投稿前にプレビュー画面を表示する」を無効にすると、
投稿文の作成・コピーをプレビューなしで実行します。


【2. 投稿プレビュー】
・投稿文はプレビュー画面内で自由に編集できます。
・文字数とX換算の目安を表示します。
・［文章をコピー］：表示中の文章をクリップボードへコピーします。
・［画像をコピー］：投稿用画像をクリップボードへコピーします。
・［Xで投稿］：文章を入力したXの投稿画面を開きます。
・［画像をコピーしてXを開く］：画像をコピーしてXを開きます。
・［保存フォルダを開く］：現在の出力先を開きます。
・［投稿履歴］：保存済みの投稿履歴を開きます。


【3. テンプレート】
設定場所：
［基本設定］→［Tools］→［NowPlaying Copy & Artwork］

テンプレートは1～20個まで保存できます。
・［追加］：新しいテンプレートを追加
・［複製］：選択中のテンプレートを複製
・［削除］：選択中のテンプレートを削除
・［↑］［↓］：表示順を変更
・［初期化］：標準の5テンプレートへ戻す

投稿文にはfoobar2000のTitle Formattingを使用できます。
主な例：
%title%        曲名
%artist%       アーティスト
%album%        アルバム名
%date%         年・日付
%tracknumber%  トラック番号
%length%       再生時間

複数行の投稿文も使用できます。
条件分岐など、通常のTitle Formatting構文も利用できます。


【4. アートワークの検索】
おおむね次の優先順で画像を探します。
1. 音源へ埋め込まれたフロントカバー
2. 音源と同じフォルダーの cover / folder / front 画像
3. ZIP内の cover / folder / front 画像
   ※設定でZIP検索を有効にしている場合

画像が見つからない場合でも投稿文は作成できます。

「投稿用アートワークを最適化する」を有効にすると、
元画像とは別に artwork-post.jpg または artwork-post.png を作成します。
・最大サイズ：256～4096px
・形式：JPEG / PNG
・JPEG画質：40～100
・画像処理：縦横比を維持、または中央を正方形に切り抜き


【5. Xへの投稿】
［Xで投稿］は文章をXへ渡します。
画像はWebブラウザーの制約により自動添付できないため、
［画像をコピーしてXを開く］を使った後、X上で Ctrl＋V を押します。

X換算が280を超える場合は確認メッセージを表示します。
最終的な投稿可否はX側の仕様により変わることがあります。


【6. 投稿履歴】
開き方：
［ファイル］→［NowPlaying投稿履歴］

・クリック：1件選択
・Ctrl＋クリック：個別に複数選択
・Shift＋クリック：範囲選択
・ダブルクリック：選択した履歴をプレビューで開く
・検索欄：日時、曲名、アーティスト、テンプレート名、投稿文を検索
・［文章をコピー］：複数選択時は空行区切りでまとめてコピー
・［選択履歴を削除］：複数選択した履歴を一括削除
・画像コピー、X再投稿、フォルダーを開く操作は1件選択時のみ使用可能

履歴からプレビューを開いた場合、投稿文は編集できますが、
元音源の情報が残っていないためテンプレート切り替えは無効になります。


【7. ピン留め】
残しておきたい履歴は［ピン留め］で保護できます。
・ピン留め履歴には一覧で ★ を表示
・複数選択して一括ピン留め／解除が可能
・［★ ピン留めのみ表示］で絞り込み
・ピン留め履歴は通常履歴の保存上限に含まれません
・手動削除と「すべて削除」ではピン留め履歴も削除できます


【8. 履歴の保存上限と表示上限】
履歴保存上限：
通常履歴を実際に保存する件数です。10～1000件で設定できます。
上限を超えると、ピン留めされていない古い履歴から整理します。

履歴表示上限：
履歴画面へ一度に表示する件数です。
保存済み履歴そのものは削除しません。
検索中は表示上限を解除し、保存済みの全履歴を検索します。


【9. 設定のバックアップ】
設定画面の下部にあります。
・［設定を書き出す...］：INIファイルへ保存
・［設定を読み込む...］：INIファイルから復元

保存される内容：
・テンプレート
・出力先
・各チェック項目
・画像最適化設定
・履歴保存上限／表示上限

投稿履歴と履歴画像は設定INIには含まれません。
設定を読み込んだ後は［適用］または［OK］を押してください。


【10. 履歴のバックアップ】
投稿履歴画面の左下にあります。
・［履歴を書き出す...］：履歴と画像を1つのZIPへ保存
・［履歴を読み込む...］：ZIPから復元

読み込み時：
・［はい］現在の履歴へ追加
・［いいえ］現在の履歴を置き換え
・［キャンセル］中止

日時・曲名・投稿文が同じ履歴は重複登録しません。
ピン留め情報もZIPへ保存されます。


【11. 作成される主なファイル】
nowplaying.txt
現在の投稿文です。

artwork-post.jpg / artwork-post.png
投稿向けに最適化した画像です。

nowplaying-history.tsv
投稿履歴の管理データです。

nowplaying-history-artwork
履歴画像を保存するフォルダーです。

nowplaying-history.tsvはExcelやメモ帳で閲覧できますが、
編集して上書きすると履歴を読み込めなくなる可能性があります。


【12. 困ったとき】
・画像が見つからない
  埋め込み画像、同じフォルダーのcover等、ZIP検索設定を確認します。

・Xへ画像が付かない
  ［画像をコピーしてXを開く］の後、Xの投稿画面でCtrl＋Vを押します。

・設定を別PCへ移したい
  設定INIを書き出し、移行先で読み込みます。

・履歴も別PCへ移したい
  履歴画面からZIPを書き出し、移行先で読み込みます。

・設定を元へ戻したい
  テンプレートだけなら［初期化］、
  ページ全体ならfoobar2000設定画面下部の［Reset page］を使います。
)HELP";
        }

        static void register_window_class() {
            static std::once_flag once;
            std::call_once(once, [] {
                WNDCLASSEXW windowClass{};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.lpfnWndProc = window_proc;
                windowClass.hInstance =
                    core_api::get_my_instance();
                windowClass.hCursor =
                    LoadCursorW(nullptr, IDC_ARROW);
                windowClass.hIcon =
                    LoadIconW(nullptr, IDI_INFORMATION);
                windowClass.hbrBackground = nullptr;
                windowClass.lpszClassName =
                    window_class_name();
                RegisterClassExW(&windowClass);
            });
        }

        bool create() {
            register_window_class();

            constexpr int clientWidth = 760;
            constexpr int clientHeight = 620;
            const DWORD style =
                WS_OVERLAPPED | WS_CAPTION |
                WS_SYSMENU | WS_MINIMIZEBOX |
                WS_MAXIMIZEBOX | WS_THICKFRAME;

            RECT windowRect{
                0, 0, clientWidth, clientHeight
            };
            AdjustWindowRectEx(
                &windowRect,
                style,
                FALSE,
                WS_EX_DLGMODALFRAME);

            const int width =
                windowRect.right - windowRect.left;
            const int height =
                windowRect.bottom - windowRect.top;

            HWND parent = core_api::get_main_window();
            RECT parentRect{};
            GetWindowRect(parent, &parentRect);
            const int x =
                parentRect.left +
                ((parentRect.right - parentRect.left) -
                    width) / 2;
            const int y =
                parentRect.top +
                ((parentRect.bottom - parentRect.top) -
                    height) / 2;

            m_wnd = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                window_class_name(),
                L"NowPlaying Copy & Artwork ヘルプ",
                style,
                x, y, width, height,
                parent,
                nullptr,
                core_api::get_my_instance(),
                this);
            if (m_wnd == nullptr) return false;
            m_creationComplete = true;

            create_controls();
            m_dark.AddDialogWithControls(m_wnd);
            layout_controls();
            InvalidateRect(m_wnd, nullptr, TRUE);

            ShowWindow(m_wnd, SW_SHOWNORMAL);
            UpdateWindow(m_wnd);
            SetForegroundWindow(m_wnd);
            SetFocus(m_helpEdit);
            SendMessageW(
                m_helpEdit,
                EM_SETSEL,
                0,
                0);
            SendMessageW(
                m_helpEdit,
                EM_SCROLLCARET,
                0,
                0);
            return true;
        }

        static LRESULT CALLBACK window_proc(
            HWND wnd,
            UINT message,
            WPARAM wp,
            LPARAM lp
        ) {
            nowplaying_help_window* self =
                reinterpret_cast<nowplaying_help_window*>(
                    GetWindowLongPtrW(
                        wnd,
                        GWLP_USERDATA));

            if (message == WM_NCCREATE) {
                const auto* create =
                    reinterpret_cast<CREATESTRUCTW*>(lp);
                self =
                    static_cast<nowplaying_help_window*>(
                        create->lpCreateParams);
                self->m_wnd = wnd;
                SetWindowLongPtrW(
                    wnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }

            if (self == nullptr) {
                return DefWindowProcW(
                    wnd,
                    message,
                    wp,
                    lp);
            }

            const LRESULT result =
                self->on_message(
                    wnd,
                    message,
                    wp,
                    lp);
            if (message == WM_NCDESTROY) {
                SetWindowLongPtrW(
                    wnd,
                    GWLP_USERDATA,
                    0);
                self->m_wnd = nullptr;
                if (self->m_creationComplete) {
                    delete self;
                }
            }
            return result;
        }

        LRESULT on_message(
            HWND wnd,
            UINT message,
            WPARAM wp,
            LPARAM lp
        ) {
            switch (message) {
            case WM_ERASEBKGND:
                return paint_nowplaying_page_background(
                    wnd,
                    reinterpret_cast<HDC>(wp),
                    static_cast<bool>(m_dark));

            case WM_SIZE:
                layout_controls();
                return 0;

            case WM_COMMAND: {
                const int id = LOWORD(wp);
                const int notification = HIWORD(wp);

                if (id == IDC_HELP_COPY &&
                    notification == BN_CLICKED) {
                    const bool copied =
                        copy_text_to_clipboard(
                            normalize_windows_newlines(
                                help_text()));
                    SetWindowTextW(
                        m_statusLabel,
                        copied
                            ? L"ヘルプ内容をクリップボードへコピーしました。"
                            : L"ヘルプ内容をコピーできませんでした。");
                    return 0;
                }

                if (id == IDC_HELP_CLOSE &&
                    notification == BN_CLICKED) {
                    DestroyWindow(wnd);
                    return 0;
                }
                break;
            }

            case WM_CLOSE:
                DestroyWindow(wnd);
                return 0;
            }

            return DefWindowProcW(
                wnd,
                message,
                wp,
                lp);
        }

        HWND create_control(
            DWORD exStyle,
            const wchar_t* className,
            const wchar_t* text,
            DWORD style,
            int id
        ) {
            HWND control = CreateWindowExW(
                exStyle,
                className,
                text,
                WS_CHILD | WS_VISIBLE | style,
                0, 0, 0, 0,
                m_wnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(id)),
                core_api::get_my_instance(),
                nullptr);

            if (control != nullptr) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        GetStockObject(
                            DEFAULT_GUI_FONT)),
                    TRUE);
            }
            return control;
        }

        void create_controls() {
            m_titleLabel = create_control(
                0,
                L"STATIC",
                L"NowPlaying Copy & Artwork ヘルプ　v1.1.2",
                0,
                0);

            const std::wstring text =
                normalize_windows_newlines(help_text());
            m_helpEdit = create_control(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                text.c_str(),
                WS_TABSTOP | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY |
                    WS_VSCROLL,
                IDC_HELP_TEXT);

            m_statusLabel = create_control(
                0,
                L"STATIC",
                L"基本操作からバックアップ方法まで確認できます。",
                0,
                0);

            m_copyButton = create_control(
                0,
                L"BUTTON",
                L"内容をコピー",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HELP_COPY);

            m_closeButton = create_control(
                0,
                L"BUTTON",
                L"閉じる",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HELP_CLOSE);
        }

        void layout_controls() {
            if (m_wnd == nullptr ||
                m_helpEdit == nullptr) {
                return;
            }

            RECT client{};
            GetClientRect(m_wnd, &client);
            const int width =
                client.right - client.left;
            const int height =
                client.bottom - client.top;
            const int margin = 14;

            MoveWindow(
                m_titleLabel,
                margin,
                margin,
                width - margin * 2,
                24,
                TRUE);

            MoveWindow(
                m_helpEdit,
                margin,
                margin + 28,
                width - margin * 2,
                max(120, height - 116),
                TRUE);

            MoveWindow(
                m_statusLabel,
                margin,
                height - 76,
                max(1, width - margin * 2 - 210),
                22,
                TRUE);

            MoveWindow(
                m_copyButton,
                width - margin - 196,
                height - 80,
                112,
                28,
                TRUE);

            MoveWindow(
                m_closeButton,
                width - margin - 78,
                height - 80,
                78,
                28,
                TRUE);
        }

        HWND m_wnd = nullptr;
        HWND m_titleLabel = nullptr;
        HWND m_helpEdit = nullptr;
        HWND m_statusLabel = nullptr;
        HWND m_copyButton = nullptr;
        HWND m_closeButton = nullptr;
        fb2k::CCoreDarkModeHooks m_dark;
        bool m_creationComplete = false;
    };

    enum : int {
        IDC_TEMPLATE_COMBO = 1101,
        IDC_TEMPLATE_NAME = 1102,
        IDC_FORMAT_EDIT = 1103,
        IDC_RESET_TEMPLATES = 1104,
        IDC_OUTPUT_EDIT = 1105,
        IDC_BROWSE_BUTTON = 1106,
        IDC_SAVE_TEXT = 1107,
        IDC_SAVE_ARTWORK = 1108,
        IDC_SEARCH_ZIP = 1109,
        IDC_SHOW_COMPLETION = 1110,
        IDC_SHOW_PREVIEW = 1111,
        IDC_OPTIMIZE_ARTWORK = 1112,
        IDC_POST_MAX_SIZE = 1113,
        IDC_POST_FORMAT = 1114,
        IDC_JPEG_QUALITY = 1115,
        IDC_SQUARE_MODE = 1116,
        IDC_HISTORY_LIMIT = 1117,
        IDC_HISTORY_DISPLAY_LIMIT = 1118,
        IDC_EXPORT_SETTINGS = 1119,
        IDC_IMPORT_SETTINGS = 1120,
        IDC_TEMPLATE_ADD = 1121,
        IDC_TEMPLATE_DUPLICATE = 1122,
        IDC_TEMPLATE_DELETE = 1123,
        IDC_TEMPLATE_UP = 1124,
        IDC_TEMPLATE_DOWN = 1125,
        IDC_HELP_BUTTON = 1126
    };

    std::wstring get_window_text_string(HWND wnd) {
        const int length = GetWindowTextLengthW(wnd);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(wnd, text.data(), length + 1);
        text.resize(static_cast<size_t>(length));
        return text;
    }

    bool checkbox_value(HWND wnd) {
        return SendMessageW(wnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    void set_checkbox_value(HWND wnd, bool value) {
        SendMessageW(
            wnd, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    std::string settings_ini_escape(const std::wstring& value) {
        const std::string utf8 = wide_to_utf8(value);
        std::string escaped;
        escaped.reserve(utf8.size() + 16);

        for (char character : utf8) {
            switch (character) {
            case '\\':
                escaped += "\\\\";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(character);
                break;
            }
        }
        return escaped;
    }

    std::wstring settings_ini_unescape(const std::string& value) {
        std::string utf8;
        utf8.reserve(value.size());

        for (size_t index = 0; index < value.size(); ++index) {
            const char character = value[index];
            if (character != '\\' || index + 1 >= value.size()) {
                utf8.push_back(character);
                continue;
            }

            const char escaped = value[++index];
            switch (escaped) {
            case '\\':
                utf8.push_back('\\');
                break;
            case 'r':
                utf8.push_back('\r');
                break;
            case 'n':
                utf8.push_back('\n');
                break;
            case 't':
                utf8.push_back('\t');
                break;
            default:
                utf8.push_back('\\');
                utf8.push_back(escaped);
                break;
            }
        }

        return utf8_to_wide(utf8.c_str());
    }

    std::string trim_ascii(std::string value) {
        const auto isSpace = [](unsigned char character) {
            return character == ' ' || character == '\t' ||
                character == '\r' || character == '\n';
        };

        while (!value.empty() &&
            isSpace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        while (!value.empty() &&
            isSpace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
        return value;
    }

    bool read_settings_ini(
        const fs::path& path,
        std::map<std::string, std::string>& values,
        std::wstring& errorText
    ) {
        values.clear();
        errorText.clear();

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            errorText = L"設定ファイルを開けませんでした。";
            return false;
        }

        file.seekg(0, std::ios::end);
        const std::streamoff fileSize = file.tellg();
        if (fileSize < 0 || fileSize > 1024 * 1024) {
            errorText =
                L"設定ファイルのサイズが不正です。";
            return false;
        }
        file.seekg(0, std::ios::beg);

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        std::istringstream stream(content);
        std::string line;
        bool inSettingsSection = false;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const std::string trimmed = trim_ascii(line);
            if (trimmed.empty() ||
                trimmed.front() == ';' ||
                trimmed.front() == '#') {
                continue;
            }

            if (trimmed.front() == '[' &&
                trimmed.back() == ']') {
                const std::string section =
                    trim_ascii(trimmed.substr(
                        1,
                        trimmed.size() - 2));
                inSettingsSection =
                    section == "NowPlayingCopyArtwork";
                continue;
            }

            if (!inSettingsSection) continue;

            const size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            const std::string key =
                trim_ascii(line.substr(0, separator));
            const std::string value =
                line.substr(separator + 1);
            if (!key.empty()) {
                values[key] = value;
            }
        }

        const auto fileType = values.find("FileType");
        if (fileType == values.end() ||
            fileType->second !=
                "NowPlaying Copy & Artwork Settings") {
            errorText =
                L"NowPlaying Copy & Artworkの設定ファイルではありません。";
            return false;
        }

        return true;
    }

    int parse_ascii_int(
        const std::string& value,
        int fallback,
        int minimum,
        int maximum
    ) {
        try {
            size_t parsedLength = 0;
            const int parsed = std::stoi(value, &parsedLength);
            if (parsedLength != value.size()) return fallback;
            if (parsed < minimum) return minimum;
            if (parsed > maximum) return maximum;
            return parsed;
        } catch (...) {
            return fallback;
        }
    }

    bool parse_ascii_bool(
        const std::string& value,
        bool fallback
    ) {
        const std::string normalized =
            trim_ascii(value);
        if (normalized == "1" ||
            normalized == "true" ||
            normalized == "yes") {
            return true;
        }
        if (normalized == "0" ||
            normalized == "false" ||
            normalized == "no") {
            return false;
        }
        return fallback;
    }

    std::wstring configured_output_directory_text() {
        const pfc::string8 configured = g_cfg_output_directory.get();
        if (configured.is_empty()) return default_output_directory().wstring();
        return utf8_to_wide(configured.c_str());
    }

    std::wstring configured_post_max_size_text() {
        const pfc::string8 configured = g_cfg_artwork_post_max_size.get();
        return configured.is_empty() ? L"2048" : utf8_to_wide(configured.c_str());
    }

    std::wstring configured_jpeg_quality_text() {
        const pfc::string8 configured = g_cfg_artwork_jpeg_quality.get();
        return configured.is_empty() ? L"90" : utf8_to_wide(configured.c_str());
    }

    int configured_post_format_index() {
        const std::wstring value = lower(utf8_to_wide(g_cfg_artwork_post_format.get().c_str()));
        return value == L"png" ? 1 : 0;
    }

    int configured_square_mode_index() {
        return parse_int_or_default(g_cfg_artwork_square_mode.get(), 0, 0, 1);
    }

    std::wstring configured_history_limit_text() {
        return std::to_wstring(configured_history_limit());
    }

    class nowplaying_preferences_instance : public preferences_page_instance {
    public:
        nowplaying_preferences_instance(
            HWND parent, preferences_page_callback::ptr callback)
            : m_callback(callback) {
            register_window_class();
            RECT rc{};
            GetClientRect(parent, &rc);
            m_wnd = CreateWindowExW(
                WS_EX_CONTROLPARENT, window_class_name(), L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                0, 0, rc.right - rc.left, rc.bottom - rc.top,
                parent, nullptr, core_api::get_my_instance(), this);
            if (m_wnd != nullptr) {
                create_controls();
                m_dark.AddDialogWithControls(m_wnd);
                InvalidateRect(m_wnd, nullptr, TRUE);
                load_from_config();
                layout_controls();
            }
        }

        ~nowplaying_preferences_instance() {
            if (m_wnd != nullptr && IsWindow(m_wnd)) DestroyWindow(m_wnd);
        }

        t_uint32 get_state() override {
            t_uint32 state =
                preferences_state::resettable |
                preferences_state::dark_mode_supported;
            if (has_changed()) state |= preferences_state::changed;
            return state;
        }

        HWND get_wnd() override { return m_wnd; }

        void apply() override {
            save_current_template_fields();
            std::vector<post_template_definition> templates;
            templates.reserve(m_templateNames.size());
            for (size_t index = 0;
                 index < m_templateNames.size();
                 ++index) {
                templates.push_back({
                    m_templateNames[index],
                    m_templateFormats[index]
                });
            }
            set_configured_template_definitions(
                std::move(templates));
            set_selected_template_index(m_selectedTemplate);
            g_cfg_output_directory = wide_to_utf8(
                get_window_text_string(m_outputEdit)).c_str();
            g_cfg_save_text = checkbox_value(m_saveText);
            g_cfg_save_artwork = checkbox_value(m_saveArtwork);
            g_cfg_search_zip = checkbox_value(m_searchZip);
            g_cfg_show_completion = checkbox_value(m_showCompletion);
            g_cfg_show_preview = checkbox_value(m_showPreview);
            g_cfg_optimize_artwork = checkbox_value(m_optimizeArtwork);

            normalize_optimization_numeric_fields();
            const UINT normalizedMaxSize = parse_uint_text_or_default(
                get_window_text_string(m_postMaxSizeEdit), 2048, 256, 4096);
            const UINT normalizedJpegQuality = parse_uint_text_or_default(
                get_window_text_string(m_jpegQualityEdit), 90, 40, 100);

            g_cfg_artwork_post_max_size = std::to_string(normalizedMaxSize).c_str();
            g_cfg_artwork_post_format =
                (SendMessageW(m_postFormatCombo, CB_GETCURSEL, 0, 0) == 1)
                    ? "png" : "jpeg";
            g_cfg_artwork_jpeg_quality = std::to_string(normalizedJpegQuality).c_str();
            const LRESULT squareSelection = SendMessageW(
                m_squareModeCombo, CB_GETCURSEL, 0, 0);
            g_cfg_artwork_square_mode = std::to_string(
                squareSelection == 1 ? 1 : 0).c_str();

            normalize_history_limit_field();
            const UINT normalizedHistoryLimit =
                parse_uint_text_or_default(
                    get_window_text_string(m_historyLimitEdit),
                    history_limit_default,
                    history_limit_minimum,
                    history_limit_maximum);
            g_cfg_history_limit =
                std::to_string(normalizedHistoryLimit).c_str();

            const LRESULT displayLimitSelection = SendMessageW(
                m_historyDisplayLimitCombo,
                CB_GETCURSEL,
                0,
                0);
            g_cfg_history_display_limit =
                std::to_string(
                    history_display_limit_from_index(
                        displayLimitSelection)).c_str();

            g_cfg_template_schema_version = "3";
            notify_state_changed();
        }

        void reset() override {
            m_initializing = true;
            reset_template_arrays();
            m_selectedTemplate = 0;
            populate_template_combo();
            load_current_template_fields();
            SetWindowTextW(
                m_outputEdit, default_output_directory().wstring().c_str());
            set_checkbox_value(m_saveText, true);
            set_checkbox_value(m_saveArtwork, true);
            set_checkbox_value(m_searchZip, true);
            set_checkbox_value(m_showCompletion, true);
            set_checkbox_value(m_showPreview, true);
            set_checkbox_value(m_optimizeArtwork, true);
            SetWindowTextW(m_postMaxSizeEdit, L"2048");
            SendMessageW(m_postFormatCombo, CB_SETCURSEL, 0, 0);
            SetWindowTextW(m_jpegQualityEdit, L"90");
            SendMessageW(m_squareModeCombo, CB_SETCURSEL, 0, 0);
            SetWindowTextW(m_historyLimitEdit, L"100");
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_SETCURSEL,
                2,
                0);
            update_optimization_controls_enabled();
            m_initializing = false;
            notify_state_changed();
        }

    private:
        static const wchar_t* window_class_name() {
            return L"FooNowPlayingCopyPreferencesPage";
        }

        static void register_window_class() {
            static std::once_flag once;
            std::call_once(once, [] {
                WNDCLASSEXW wc{};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = window_proc;
                wc.hInstance = core_api::get_my_instance();
                wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                wc.hbrBackground = nullptr;
                wc.lpszClassName = window_class_name();
                RegisterClassExW(&wc);
            });
        }

        static LRESULT CALLBACK window_proc(
            HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
            nowplaying_preferences_instance* self =
                reinterpret_cast<nowplaying_preferences_instance*>(
                    GetWindowLongPtrW(wnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                self = static_cast<nowplaying_preferences_instance*>(
                    cs->lpCreateParams);
                SetWindowLongPtrW(
                    wnd, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }
            if (self != nullptr) return self->on_message(wnd, msg, wp, lp);
            return DefWindowProcW(wnd, msg, wp, lp);
        }

        LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
            switch (msg) {
            case WM_ERASEBKGND:
                return paint_nowplaying_page_background(
                    wnd, reinterpret_cast<HDC>(wp),
                    static_cast<bool>(m_dark));
            case WM_SIZE:
                layout_controls();
                return 0;
            case WM_COMMAND: {
                const int id = LOWORD(wp);
                const int notification = HIWORD(wp);
                if (id == IDC_TEMPLATE_COMBO &&
                    notification == CBN_SELCHANGE) {
                    save_current_template_fields();
                    const LRESULT selected = SendMessageW(
                        m_templateCombo, CB_GETCURSEL, 0, 0);
                    if (selected != CB_ERR) {
                        m_selectedTemplate = static_cast<int>(selected);
                        load_current_template_fields();
                        notify_state_changed();
                    }
                    return 0;
                }
                if ((id == IDC_TEMPLATE_NAME || id == IDC_FORMAT_EDIT) &&
                    notification == EN_CHANGE) {
                    save_current_template_fields();
                    notify_state_changed();
                    return 0;
                }
                if (id == IDC_TEMPLATE_NAME &&
                    notification == EN_KILLFOCUS) {
                    populate_template_combo();
                    return 0;
                }
                if (id == IDC_TEMPLATE_ADD &&
                    notification == BN_CLICKED) {
                    add_template();
                    return 0;
                }
                if (id == IDC_TEMPLATE_DUPLICATE &&
                    notification == BN_CLICKED) {
                    duplicate_template();
                    return 0;
                }
                if (id == IDC_TEMPLATE_DELETE &&
                    notification == BN_CLICKED) {
                    delete_template();
                    return 0;
                }
                if (id == IDC_TEMPLATE_UP &&
                    notification == BN_CLICKED) {
                    move_template(-1);
                    return 0;
                }
                if (id == IDC_TEMPLATE_DOWN &&
                    notification == BN_CLICKED) {
                    move_template(1);
                    return 0;
                }
                if (id == IDC_RESET_TEMPLATES &&
                    notification == BN_CLICKED) {
                    reset_templates_only();
                    return 0;
                }
                if (id == IDC_BROWSE_BUTTON && notification == BN_CLICKED) {
                    browse_for_folder();
                    return 0;
                }
                if (id == IDC_EXPORT_SETTINGS &&
                    notification == BN_CLICKED) {
                    export_settings();
                    return 0;
                }
                if (id == IDC_IMPORT_SETTINGS &&
                    notification == BN_CLICKED) {
                    import_settings();
                    return 0;
                }
                if (id == IDC_HELP_BUTTON &&
                    notification == BN_CLICKED) {
                    nowplaying_help_window::open();
                    return 0;
                }
                if ((id == IDC_OUTPUT_EDIT || id == IDC_POST_MAX_SIZE ||
                     id == IDC_JPEG_QUALITY || id == IDC_HISTORY_LIMIT) &&
                    notification == EN_CHANGE) {
                    notify_state_changed();
                    return 0;
                }
                if ((id == IDC_POST_MAX_SIZE || id == IDC_JPEG_QUALITY) &&
                    notification == EN_KILLFOCUS) {
                    normalize_optimization_numeric_fields();
                    notify_state_changed();
                    return 0;
                }
                if (id == IDC_HISTORY_LIMIT &&
                    notification == EN_KILLFOCUS) {
                    normalize_history_limit_field();
                    notify_state_changed();
                    return 0;
                }
                if (id == IDC_POST_FORMAT && notification == CBN_SELCHANGE) {
                    update_optimization_controls_enabled();
                    notify_state_changed();
                    return 0;
                }
                if (id == IDC_SQUARE_MODE && notification == CBN_SELCHANGE) {
                    notify_state_changed();
                    return 0;
                }
                if (id == IDC_HISTORY_DISPLAY_LIMIT &&
                    notification == CBN_SELCHANGE) {
                    notify_state_changed();
                    return 0;
                }
                if ((id == IDC_OPTIMIZE_ARTWORK || id == IDC_SAVE_ARTWORK) &&
                    notification == BN_CLICKED) {
                    update_optimization_controls_enabled();
                    notify_state_changed();
                    return 0;
                }
                if ((id == IDC_SAVE_TEXT || id == IDC_SEARCH_ZIP ||
                     id == IDC_SHOW_COMPLETION || id == IDC_SHOW_PREVIEW) &&
                    notification == BN_CLICKED) {
                    notify_state_changed();
                    return 0;
                }
                break;
            }
            case WM_NCDESTROY:
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                m_wnd = nullptr;
                break;
            }
            return DefWindowProcW(wnd, msg, wp, lp);
        }

        HWND create_control(
            DWORD exStyle, const wchar_t* className,
            const wchar_t* text, DWORD style, int id = 0) {
            HWND control = CreateWindowExW(
                exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                0, 0, 0, 0, m_wnd,
                id == 0 ? nullptr : reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(id)),
                core_api::get_my_instance(), nullptr);
            if (control != nullptr) {
                SendMessageW(
                    control, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                    TRUE);
            }
            return control;
        }

        void create_controls() {
            m_templateLabel = create_control(
                0, L"STATIC", L"編集するテンプレート", 0);
            m_templateCombo = create_control(
                0, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                IDC_TEMPLATE_COMBO);
            m_addTemplateButton = create_control(
                0, L"BUTTON", L"追加",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_TEMPLATE_ADD);
            m_duplicateTemplateButton = create_control(
                0, L"BUTTON", L"複製",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_TEMPLATE_DUPLICATE);
            m_deleteTemplateButton = create_control(
                0, L"BUTTON", L"削除",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_TEMPLATE_DELETE);
            m_moveTemplateUpButton = create_control(
                0, L"BUTTON", L"↑",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_TEMPLATE_UP);
            m_moveTemplateDownButton = create_control(
                0, L"BUTTON", L"↓",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_TEMPLATE_DOWN);
            m_resetTemplatesButton = create_control(
                0, L"BUTTON", L"初期化",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_RESET_TEMPLATES);
            m_nameLabel = create_control(
                0, L"STATIC", L"テンプレート名", 0);
            m_nameEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_AUTOHSCROLL, IDC_TEMPLATE_NAME);
            m_formatLabel = create_control(
                0, L"STATIC", L"投稿文の書式", 0);
            m_formatEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                    WS_VSCROLL | ES_WANTRETURN,
                IDC_FORMAT_EDIT);
            m_formatHint = create_control(
                0, L"STATIC",
                L"使用例：%title%  %artist%  %album%  %date%  %tracknumber%  %length%",
                0);
            m_outputLabel = create_control(
                0, L"STATIC", L"保存先フォルダ", 0);
            m_outputEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_AUTOHSCROLL, IDC_OUTPUT_EDIT);
            m_browseButton = create_control(
                0, L"BUTTON", L"参照...",
                WS_TABSTOP | BS_PUSHBUTTON, IDC_BROWSE_BUTTON);
            m_showPreview = create_control(
                0, L"BUTTON", L"投稿前にプレビュー画面を表示する",
                WS_TABSTOP | BS_AUTOCHECKBOX, IDC_SHOW_PREVIEW);
            m_saveText = create_control(
                0, L"BUTTON", L"nowplaying.txtを保存する",
                WS_TABSTOP | BS_AUTOCHECKBOX, IDC_SAVE_TEXT);
            m_saveArtwork = create_control(
                0, L"BUTTON", L"アートワークを保存する",
                WS_TABSTOP | BS_AUTOCHECKBOX, IDC_SAVE_ARTWORK);
            m_searchZip = create_control(
                0, L"BUTTON", L"ZIP内のcover／folder／front画像を検索する",
                WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE,
                IDC_SEARCH_ZIP);
            m_showCompletion = create_control(
                0, L"BUTTON", L"処理完了メッセージを表示する",
                WS_TABSTOP | BS_AUTOCHECKBOX, IDC_SHOW_COMPLETION);
            m_optimizeArtwork = create_control(
                0, L"BUTTON", L"投稿用アートワークを最適化する",
                WS_TABSTOP | BS_AUTOCHECKBOX, IDC_OPTIMIZE_ARTWORK);
            m_postMaxSizeLabel = create_control(
                0, L"STATIC", L"最大サイズ(px)", 0);
            m_postMaxSizeEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, IDC_POST_MAX_SIZE);
            m_postFormatLabel = create_control(
                0, L"STATIC", L"形式", 0);
            m_postFormatCombo = create_control(
                0, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_POST_FORMAT);
            SendMessageW(m_postFormatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"JPEG"));
            SendMessageW(m_postFormatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"PNG"));
            m_jpegQualityLabel = create_control(
                0, L"STATIC", L"JPEG画質", 0);
            m_jpegQualityEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, IDC_JPEG_QUALITY);
            m_squareModeLabel = create_control(
                0, L"STATIC", L"画像処理", 0);
            m_squareModeCombo = create_control(
                0, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_SQUARE_MODE);
            SendMessageW(m_squareModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"そのまま縮小"));
            SendMessageW(m_squareModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"中央を正方形に切り抜く"));
            m_historyLimitLabel = create_control(
                0, L"STATIC", L"履歴保存上限", 0);
            m_historyLimitEdit = create_control(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                IDC_HISTORY_LIMIT);
            m_historyLimitUnitLabel = create_control(
                0, L"STATIC", L"件（10～1000）", 0);
            m_historyDisplayLimitLabel = create_control(
                0, L"STATIC", L"履歴表示上限", 0);
            m_historyDisplayLimitCombo = create_control(
                0, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                IDC_HISTORY_DISPLAY_LIMIT);
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(L"すべて表示"));
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(L"最新50件"));
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(L"最新100件"));
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(L"最新200件"));
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(L"最新500件"));
            m_exportSettingsButton = create_control(
                0, L"BUTTON", L"設定を書き出す...",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_EXPORT_SETTINGS);
            m_importSettingsButton = create_control(
                0, L"BUTTON", L"設定を読み込む...",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_IMPORT_SETTINGS);
            m_helpButton = create_control(
                0, L"BUTTON", L"ヘルプ",
                WS_TABSTOP | BS_PUSHBUTTON,
                IDC_HELP_BUTTON);
            m_note = create_control(
                0, L"STATIC",
                L"INIにはテンプレートと各種設定を保存します。投稿履歴と履歴画像は含みません。読み込み後は［適用］または［OK］で保存してください。",
                0);
        }

        void layout_controls() {
            if (m_wnd == nullptr) return;
            RECT rc{};
            GetClientRect(m_wnd, &rc);
            const int width = max(1, rc.right - rc.left);
            const int margin = 12;
            const int innerWidth = width - margin * 2;
            const int browseWidth = 82;
            const int gap = 8;
            const int halfWidth = (innerWidth - gap) / 2;

            // Keep the template toolbar compact so it fits inside
            // foobar2000's preference page without clipping.
            const int actionGap = 3;
            const int normalActionWidth = 42;
            const int arrowActionWidth = 28;
            const int resetActionWidth = 52;
            const int actionTotalWidth =
                normalActionWidth * 3 +
                arrowActionWidth * 2 +
                resetActionWidth +
                actionGap * 5;
            const int templateLabelWidth = 108;
            const int templateComboX =
                margin + templateLabelWidth;
            const int templateComboWidth =
                max(78, innerWidth - templateLabelWidth -
                    actionTotalWidth - gap);
            int actionX =
                templateComboX + templateComboWidth + gap;

            MoveWindow(m_templateLabel, margin, 8, 106, 20, TRUE);
            MoveWindow(
                m_templateCombo, templateComboX, 5,
                templateComboWidth, 240, TRUE);
            MoveWindow(
                m_addTemplateButton, actionX, 5,
                normalActionWidth, 25, TRUE);
            actionX += normalActionWidth + actionGap;
            MoveWindow(
                m_duplicateTemplateButton, actionX, 5,
                normalActionWidth, 25, TRUE);
            actionX += normalActionWidth + actionGap;
            MoveWindow(
                m_deleteTemplateButton, actionX, 5,
                normalActionWidth, 25, TRUE);
            actionX += normalActionWidth + actionGap;
            MoveWindow(
                m_moveTemplateUpButton, actionX, 5,
                arrowActionWidth, 25, TRUE);
            actionX += arrowActionWidth + actionGap;
            MoveWindow(
                m_moveTemplateDownButton, actionX, 5,
                arrowActionWidth, 25, TRUE);
            actionX += arrowActionWidth + actionGap;
            MoveWindow(
                m_resetTemplatesButton, actionX, 5,
                resetActionWidth, 25, TRUE);
            MoveWindow(m_nameLabel, margin, 36, innerWidth, 18, TRUE);
            MoveWindow(m_nameEdit, margin, 54, innerWidth, 23, TRUE);
            MoveWindow(m_formatLabel, margin, 82, innerWidth, 18, TRUE);
            MoveWindow(m_formatEdit, margin, 100, innerWidth, 60, TRUE);
            MoveWindow(m_formatHint, margin, 163, innerWidth, 20, TRUE);
            MoveWindow(m_outputLabel, margin, 187, innerWidth, 18, TRUE);
            MoveWindow(
                m_outputEdit, margin, 205,
                innerWidth - browseWidth - gap, 23, TRUE);
            MoveWindow(
                m_browseButton, width - margin - browseWidth, 205,
                browseWidth, 23, TRUE);

            // Arrange shorter options in two columns and give the
            // longer ZIP-search option the full available width.
            MoveWindow(m_showPreview, margin, 236, halfWidth, 21, TRUE);
            MoveWindow(
                m_saveText, margin + halfWidth + gap, 236,
                halfWidth, 21, TRUE);
            MoveWindow(m_saveArtwork, margin, 260, halfWidth, 21, TRUE);
            MoveWindow(
                m_showCompletion, margin + halfWidth + gap, 260,
                halfWidth, 21, TRUE);
            MoveWindow(m_searchZip, margin, 284, innerWidth, 21, TRUE);
            MoveWindow(m_optimizeArtwork, margin, 308, innerWidth, 21, TRUE);

            // Keep the compact numeric/format settings on one row.
            const int compactGap = 6;
            int settingsX = margin;
            const int settingsY = 334;

            MoveWindow(
                m_postMaxSizeLabel,
                settingsX, settingsY + 2, 92, 20, TRUE);
            settingsX += 92;
            MoveWindow(
                m_postMaxSizeEdit,
                settingsX, settingsY, 70, 23, TRUE);
            settingsX += 70 + compactGap;

            MoveWindow(
                m_postFormatLabel,
                settingsX, settingsY + 2, 32, 20, TRUE);
            settingsX += 32;
            MoveWindow(
                m_postFormatCombo,
                settingsX, settingsY, 76, 180, TRUE);
            settingsX += 76 + compactGap;

            MoveWindow(
                m_jpegQualityLabel,
                settingsX, settingsY + 2, 66, 20, TRUE);
            settingsX += 66;
            MoveWindow(
                m_jpegQualityEdit,
                settingsX, settingsY, 56, 23, TRUE);

            const int squareY = 362;
            MoveWindow(
                m_squareModeLabel,
                margin, squareY + 2, 64, 20, TRUE);
            MoveWindow(
                m_squareModeCombo,
                margin + 66, squareY,
                max(140, innerWidth - 66), 180, TRUE);

            const int historyY = 390;
            const int historyHalfWidth =
                max(220, (innerWidth - gap) / 2);

            MoveWindow(
                m_historyLimitLabel,
                margin, historyY + 2, 92, 20, TRUE);
            MoveWindow(
                m_historyLimitEdit,
                margin + 94, historyY, 62, 23, TRUE);
            MoveWindow(
                m_historyLimitUnitLabel,
                margin + 162, historyY + 2,
                max(72, historyHalfWidth - 162), 20, TRUE);

            const int displayX =
                margin + historyHalfWidth + gap;
            MoveWindow(
                m_historyDisplayLimitLabel,
                displayX, historyY + 2, 92, 20, TRUE);
            MoveWindow(
                m_historyDisplayLimitCombo,
                displayX + 94, historyY,
                max(112, innerWidth - historyHalfWidth -
                    gap - 94),
                180,
                TRUE);

            // Anchor the buttons near the bottom of the actual page.
            const int clientHeight = max(1, rc.bottom - rc.top);
            const int settingsButtonY =
                max(historyY + 28, clientHeight - 32);
            const int helpButtonWidth = 76;
            const int settingsButtonWidth =
                max(1, (innerWidth - helpButtonWidth -
                    gap * 2) / 2);

            MoveWindow(
                m_exportSettingsButton,
                margin,
                settingsButtonY,
                settingsButtonWidth,
                26,
                TRUE);
            MoveWindow(
                m_importSettingsButton,
                margin + settingsButtonWidth + gap,
                settingsButtonY,
                settingsButtonWidth,
                26,
                TRUE);
            MoveWindow(
                m_helpButton,
                margin + settingsButtonWidth * 2 +
                    gap * 2,
                settingsButtonY,
                max(1, innerWidth -
                    settingsButtonWidth * 2 -
                    gap * 2),
                26,
                TRUE);

            // The explanatory text is intentionally hidden to preserve
            // enough room on the standard foobar2000 preferences page.
            ShowWindow(m_note, SW_HIDE);
        }

        void load_from_config() {
            m_initializing = true;
            const auto templates =
                configured_template_definitions();
            m_templateNames.clear();
            m_templateFormats.clear();
            m_templateNames.reserve(templates.size());
            m_templateFormats.reserve(templates.size());
            for (const auto& item : templates) {
                m_templateNames.push_back(item.name);
                m_templateFormats.push_back(item.format);
            }
            m_selectedTemplate = selected_template_index();
            populate_template_combo();
            load_current_template_fields();
            SetWindowTextW(
                m_outputEdit, configured_output_directory_text().c_str());
            set_checkbox_value(m_saveText, g_cfg_save_text.get());
            set_checkbox_value(m_saveArtwork, g_cfg_save_artwork.get());
            set_checkbox_value(m_searchZip, g_cfg_search_zip.get());
            set_checkbox_value(
                m_showCompletion, g_cfg_show_completion.get());
            set_checkbox_value(m_showPreview, g_cfg_show_preview.get());
            set_checkbox_value(m_optimizeArtwork, g_cfg_optimize_artwork.get());
            SetWindowTextW(m_postMaxSizeEdit, configured_post_max_size_text().c_str());
            SendMessageW(m_postFormatCombo, CB_SETCURSEL, configured_post_format_index(), 0);
            SetWindowTextW(m_jpegQualityEdit, configured_jpeg_quality_text().c_str());
            SendMessageW(
                m_squareModeCombo,
                CB_SETCURSEL,
                configured_square_mode_index(),
                0);
            SetWindowTextW(
                m_historyLimitEdit,
                configured_history_limit_text().c_str());
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_SETCURSEL,
                configured_history_display_limit_index(),
                0);
            update_optimization_controls_enabled();
            m_initializing = false;
        }

        void normalize_optimization_numeric_fields() {
            const UINT maxSize = parse_uint_text_or_default(
                get_window_text_string(m_postMaxSizeEdit),
                2048,
                256,
                4096);
            const UINT jpegQuality = parse_uint_text_or_default(
                get_window_text_string(m_jpegQualityEdit),
                90,
                40,
                100);

            const std::wstring maxSizeText = std::to_wstring(maxSize);
            const std::wstring qualityText = std::to_wstring(jpegQuality);
            if (get_window_text_string(m_postMaxSizeEdit) != maxSizeText) {
                SetWindowTextW(m_postMaxSizeEdit, maxSizeText.c_str());
            }
            if (get_window_text_string(m_jpegQualityEdit) != qualityText) {
                SetWindowTextW(m_jpegQualityEdit, qualityText.c_str());
            }
        }

        void normalize_history_limit_field() {
            const UINT historyLimit =
                parse_uint_text_or_default(
                    get_window_text_string(m_historyLimitEdit),
                    history_limit_default,
                    history_limit_minimum,
                    history_limit_maximum);
            const std::wstring normalizedText =
                std::to_wstring(historyLimit);
            if (get_window_text_string(m_historyLimitEdit) !=
                normalizedText) {
                SetWindowTextW(
                    m_historyLimitEdit,
                    normalizedText.c_str());
            }
        }

        void update_optimization_controls_enabled() {
            const bool artworkEnabled = checkbox_value(m_saveArtwork);
            const bool optimizationEnabled =
                artworkEnabled && checkbox_value(m_optimizeArtwork);
            const LRESULT formatSelection = SendMessageW(
                m_postFormatCombo, CB_GETCURSEL, 0, 0);
            const bool jpegEnabled =
                optimizationEnabled && formatSelection != 1;

            EnableWindow(m_optimizeArtwork, artworkEnabled ? TRUE : FALSE);
            EnableWindow(m_postMaxSizeLabel, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_postMaxSizeEdit, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_postFormatLabel, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_postFormatCombo, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_squareModeLabel, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_squareModeCombo, optimizationEnabled ? TRUE : FALSE);
            EnableWindow(m_jpegQualityLabel, jpegEnabled ? TRUE : FALSE);
            EnableWindow(m_jpegQualityEdit, jpegEnabled ? TRUE : FALSE);
        }

        void reset_template_arrays() {
            const auto defaults = default_template_definitions();
            m_templateNames.clear();
            m_templateFormats.clear();
            for (const auto& item : defaults) {
                m_templateNames.push_back(item.name);
                m_templateFormats.push_back(item.format);
            }
        }

        void reset_templates_only() {
            if (MessageBoxW(
                    m_wnd,
                    L"テンプレートを初期状態の5個へ戻しますか？",
                    L"NowPlaying Copy & Artwork",
                    MB_YESNO | MB_ICONQUESTION |
                        MB_DEFBUTTON2) != IDYES) {
                return;
            }

            m_initializing = true;
            reset_template_arrays();
            m_selectedTemplate = 0;
            populate_template_combo();
            load_current_template_fields();
            m_initializing = false;
            notify_state_changed();
        }

        void update_template_action_buttons() {
            const size_t count = m_templateNames.size();
            const bool validSelection =
                m_selectedTemplate >= 0 &&
                static_cast<size_t>(m_selectedTemplate) < count;
            EnableWindow(
                m_addTemplateButton,
                count < template_max_count ? TRUE : FALSE);
            EnableWindow(
                m_duplicateTemplateButton,
                validSelection && count < template_max_count
                    ? TRUE : FALSE);
            EnableWindow(
                m_deleteTemplateButton,
                validSelection && count > template_min_count
                    ? TRUE : FALSE);
            EnableWindow(
                m_moveTemplateUpButton,
                validSelection && m_selectedTemplate > 0
                    ? TRUE : FALSE);
            EnableWindow(
                m_moveTemplateDownButton,
                validSelection &&
                    static_cast<size_t>(m_selectedTemplate + 1) < count
                    ? TRUE : FALSE);
        }

        void add_template() {
            save_current_template_fields();
            if (m_templateNames.size() >= template_max_count) {
                MessageBoxW(
                    m_wnd,
                    L"テンプレートは最大20個です。",
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONINFORMATION);
                return;
            }

            const size_t number = m_templateNames.size() + 1;
            m_templateNames.push_back(
                L"テンプレート " + std::to_wstring(number));
            m_templateFormats.push_back(
                utf8_to_wide(default_post_format));
            m_selectedTemplate =
                static_cast<int>(m_templateNames.size() - 1);
            populate_template_combo();
            load_current_template_fields();
            notify_state_changed();
            SetFocus(m_nameEdit);
            SendMessageW(m_nameEdit, EM_SETSEL, 0, -1);
        }

        void duplicate_template() {
            save_current_template_fields();
            if (m_templateNames.size() >= template_max_count) {
                MessageBoxW(
                    m_wnd,
                    L"テンプレートは最大20個です。",
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONINFORMATION);
                return;
            }
            if (m_selectedTemplate < 0 ||
                static_cast<size_t>(m_selectedTemplate) >=
                    m_templateNames.size()) {
                return;
            }

            const size_t sourceIndex =
                static_cast<size_t>(m_selectedTemplate);
            std::wstring copiedName =
                m_templateNames[sourceIndex];
            if (copiedName.empty()) {
                copiedName = L"テンプレート " +
                    std::to_wstring(sourceIndex + 1);
            }
            copiedName += L" のコピー";
            m_templateNames.push_back(std::move(copiedName));
            m_templateFormats.push_back(
                m_templateFormats[sourceIndex]);
            m_selectedTemplate =
                static_cast<int>(m_templateNames.size() - 1);
            populate_template_combo();
            load_current_template_fields();
            notify_state_changed();
        }

        void delete_template() {
            save_current_template_fields();
            if (m_templateNames.size() <= template_min_count) {
                MessageBoxW(
                    m_wnd,
                    L"テンプレートは最低1個必要です。",
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONINFORMATION);
                return;
            }
            if (m_selectedTemplate < 0 ||
                static_cast<size_t>(m_selectedTemplate) >=
                    m_templateNames.size()) {
                return;
            }

            std::wstring confirmation = L"テンプレート「";
            confirmation +=
                m_templateNames[static_cast<size_t>(m_selectedTemplate)];
            confirmation += L"」を削除しますか？";
            if (MessageBoxW(
                    m_wnd,
                    confirmation.c_str(),
                    L"NowPlaying Copy & Artwork",
                    MB_YESNO | MB_ICONWARNING |
                        MB_DEFBUTTON2) != IDYES) {
                return;
            }

            const size_t index =
                static_cast<size_t>(m_selectedTemplate);
            m_templateNames.erase(
                m_templateNames.begin() +
                static_cast<std::ptrdiff_t>(index));
            m_templateFormats.erase(
                m_templateFormats.begin() +
                static_cast<std::ptrdiff_t>(index));
            if (m_selectedTemplate >=
                static_cast<int>(m_templateNames.size())) {
                m_selectedTemplate =
                    static_cast<int>(m_templateNames.size() - 1);
            }
            populate_template_combo();
            load_current_template_fields();
            notify_state_changed();
        }

        void move_template(int direction) {
            save_current_template_fields();
            if (m_selectedTemplate < 0 ||
                static_cast<size_t>(m_selectedTemplate) >=
                    m_templateNames.size()) {
                return;
            }

            const int destination =
                m_selectedTemplate + direction;
            if (destination < 0 ||
                destination >=
                    static_cast<int>(m_templateNames.size())) {
                return;
            }

            std::swap(
                m_templateNames[static_cast<size_t>(m_selectedTemplate)],
                m_templateNames[static_cast<size_t>(destination)]);
            std::swap(
                m_templateFormats[static_cast<size_t>(m_selectedTemplate)],
                m_templateFormats[static_cast<size_t>(destination)]);
            m_selectedTemplate = destination;
            populate_template_combo();
            load_current_template_fields();
            notify_state_changed();
        }

        void populate_template_combo() {
            if (m_templateCombo == nullptr) return;
            if (m_templateNames.empty()) {
                m_templateNames.push_back(L"シンプル");
                m_templateFormats.push_back(
                    utf8_to_wide(default_post_format));
            }
            while (m_templateFormats.size() < m_templateNames.size()) {
                m_templateFormats.push_back(
                    utf8_to_wide(default_post_format));
            }
            if (m_templateFormats.size() > m_templateNames.size()) {
                m_templateFormats.resize(m_templateNames.size());
            }

            SendMessageW(m_templateCombo, CB_RESETCONTENT, 0, 0);
            for (size_t index = 0;
                 index < m_templateNames.size();
                 ++index) {
                std::wstring label = m_templateNames[index];
                if (label.empty()) {
                    label = L"テンプレート " +
                        std::to_wstring(index + 1);
                }
                SendMessageW(
                    m_templateCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(label.c_str()));
            }
            if (m_selectedTemplate < 0 ||
                m_selectedTemplate >=
                    static_cast<int>(m_templateNames.size())) {
                m_selectedTemplate = 0;
            }
            SendMessageW(
                m_templateCombo, CB_SETCURSEL,
                m_selectedTemplate, 0);
            update_template_action_buttons();
        }

        void save_current_template_fields() {
            if (m_initializing || m_selectedTemplate < 0 ||
                static_cast<size_t>(m_selectedTemplate) >=
                    m_templateNames.size()) {
                return;
            }
            const size_t index =
                static_cast<size_t>(m_selectedTemplate);
            m_templateNames[index] =
                get_window_text_string(m_nameEdit);
            m_templateFormats[index] =
                get_window_text_string(m_formatEdit);
        }

        void load_current_template_fields() {
            if (m_templateNames.empty()) {
                reset_template_arrays();
            }
            if (m_selectedTemplate < 0 ||
                m_selectedTemplate >=
                    static_cast<int>(m_templateNames.size())) {
                m_selectedTemplate = 0;
            }

            const bool wasInitializing = m_initializing;
            m_initializing = true;

            const size_t index =
                static_cast<size_t>(m_selectedTemplate);
            SetWindowTextW(
                m_nameEdit,
                m_templateNames[index].c_str());
            SetWindowTextW(
                m_formatEdit,
                m_templateFormats[index].c_str());

            m_initializing = wasInitializing;
            update_template_action_buttons();
        }

        bool has_changed() const {
            const auto configuredTemplates =
                configured_template_definitions();
            if (m_templateNames.size() != configuredTemplates.size() ||
                m_templateFormats.size() != configuredTemplates.size()) {
                return true;
            }
            for (size_t index = 0;
                 index < configuredTemplates.size();
                 ++index) {
                if (m_templateNames[index] !=
                    configuredTemplates[index].name) return true;
                if (m_templateFormats[index] !=
                    configuredTemplates[index].format) return true;
            }
            if (m_selectedTemplate != selected_template_index()) return true;
            if (get_window_text_string(m_outputEdit) !=
                configured_output_directory_text()) return true;
            if (checkbox_value(m_saveText) != g_cfg_save_text.get()) return true;
            if (checkbox_value(m_saveArtwork) !=
                g_cfg_save_artwork.get()) return true;
            if (checkbox_value(m_searchZip) != g_cfg_search_zip.get()) return true;
            if (checkbox_value(m_showCompletion) !=
                g_cfg_show_completion.get()) return true;
            if (checkbox_value(m_showPreview) !=
                g_cfg_show_preview.get()) return true;
            if (checkbox_value(m_optimizeArtwork) != g_cfg_optimize_artwork.get()) return true;
            if (get_window_text_string(m_postMaxSizeEdit) != configured_post_max_size_text()) return true;
            if (SendMessageW(m_postFormatCombo, CB_GETCURSEL, 0, 0) != configured_post_format_index()) return true;
            if (get_window_text_string(m_jpegQualityEdit) != configured_jpeg_quality_text()) return true;
            if (SendMessageW(m_squareModeCombo, CB_GETCURSEL, 0, 0) != configured_square_mode_index()) return true;
            if (get_window_text_string(m_historyLimitEdit) !=
                configured_history_limit_text()) return true;
            if (SendMessageW(
                    m_historyDisplayLimitCombo,
                    CB_GETCURSEL,
                    0,
                    0) !=
                configured_history_display_limit_index()) return true;
            return false;
        }

        void notify_state_changed() {
            if (!m_initializing && m_callback.is_valid()) {
                m_callback->on_state_changed();
            }
        }

        bool choose_settings_file(
            bool save,
            fs::path& selectedPath
        ) {
            wchar_t filePath[32768]{};
            std::wstring initialDirectory =
                get_window_text_string(m_outputEdit);

            if (save) {
                const wchar_t* defaultName =
                    L"NowPlaying-Copy-Settings.ini";
                std::wcsncpy(
                    filePath,
                    defaultName,
                    std::size(filePath) - 1);
            }

            const wchar_t filter[] =
                L"INI設定ファイル (*.ini)\0*.ini\0"
                L"すべてのファイル (*.*)\0*.*\0\0";

            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = m_wnd;
            dialog.hInstance =
                core_api::get_my_instance();
            dialog.lpstrFilter = filter;
            dialog.nFilterIndex = 1;
            dialog.lpstrFile = filePath;
            dialog.nMaxFile =
                static_cast<DWORD>(std::size(filePath));
            dialog.lpstrInitialDir =
                initialDirectory.empty()
                    ? nullptr
                    : initialDirectory.c_str();
            dialog.lpstrDefExt = L"ini";
            dialog.Flags =
                OFN_EXPLORER |
                OFN_NOCHANGEDIR |
                OFN_PATHMUSTEXIST |
                (save
                    ? OFN_OVERWRITEPROMPT
                    : OFN_FILEMUSTEXIST);

            const BOOL accepted =
                save
                    ? GetSaveFileNameW(&dialog)
                    : GetOpenFileNameW(&dialog);
            if (!accepted) return false;

            selectedPath = fs::path(filePath);
            return true;
        }

        std::string settings_value(
            const std::map<std::string, std::string>& values,
            const std::string& key,
            const std::string& fallback
        ) const {
            const auto found = values.find(key);
            return found == values.end()
                ? fallback
                : found->second;
        }

        void export_settings() {
            save_current_template_fields();
            normalize_optimization_numeric_fields();
            normalize_history_limit_field();

            fs::path destination;
            if (!choose_settings_file(true, destination)) {
                return;
            }

            const int postFormat =
                SendMessageW(
                    m_postFormatCombo,
                    CB_GETCURSEL,
                    0,
                    0) == 1
                    ? 1
                    : 0;
            const int squareMode =
                SendMessageW(
                    m_squareModeCombo,
                    CB_GETCURSEL,
                    0,
                    0) == 1
                    ? 1
                    : 0;
            const int historyDisplayLimit =
                history_display_limit_from_index(
                    SendMessageW(
                        m_historyDisplayLimitCombo,
                        CB_GETCURSEL,
                        0,
                        0));

            std::ostringstream ini;
            ini << "; NowPlaying Copy & Artwork settings\r\n";
            ini << "; UTF-8 / v1\r\n\r\n";
            ini << "[NowPlayingCopyArtwork]\r\n";
            ini << "FileType=NowPlaying Copy & Artwork Settings\r\n";
            ini << "Version=1\r\n";
            ini << "TemplateCount=" << m_templateNames.size() << "\r\n";
            ini << "SelectedTemplate="
                << m_selectedTemplate << "\r\n";

            for (size_t index = 0;
                 index < m_templateNames.size();
                 ++index) {
                const std::string number =
                    std::to_string(index + 1);
                ini << "Template" << number << "Name="
                    << settings_ini_escape(
                        m_templateNames[index])
                    << "\r\n";
                ini << "Template" << number << "Format="
                    << settings_ini_escape(
                        m_templateFormats[index])
                    << "\r\n";
            }

            ini << "OutputDirectory="
                << settings_ini_escape(
                    get_window_text_string(m_outputEdit))
                << "\r\n";
            ini << "SaveText="
                << (checkbox_value(m_saveText) ? 1 : 0)
                << "\r\n";
            ini << "SaveArtwork="
                << (checkbox_value(m_saveArtwork) ? 1 : 0)
                << "\r\n";
            ini << "SearchZip="
                << (checkbox_value(m_searchZip) ? 1 : 0)
                << "\r\n";
            ini << "ShowCompletion="
                << (checkbox_value(m_showCompletion) ? 1 : 0)
                << "\r\n";
            ini << "ShowPreview="
                << (checkbox_value(m_showPreview) ? 1 : 0)
                << "\r\n";
            ini << "OptimizeArtwork="
                << (checkbox_value(m_optimizeArtwork) ? 1 : 0)
                << "\r\n";
            ini << "ArtworkMaxSize="
                << wide_to_utf8(
                    get_window_text_string(
                        m_postMaxSizeEdit))
                << "\r\n";
            ini << "ArtworkFormat="
                << (postFormat == 1 ? "png" : "jpeg")
                << "\r\n";
            ini << "JpegQuality="
                << wide_to_utf8(
                    get_window_text_string(
                        m_jpegQualityEdit))
                << "\r\n";
            ini << "SquareMode="
                << squareMode << "\r\n";
            ini << "HistorySaveLimit="
                << wide_to_utf8(
                    get_window_text_string(
                        m_historyLimitEdit))
                << "\r\n";
            ini << "HistoryDisplayLimit="
                << historyDisplayLimit << "\r\n";

            if (!write_utf8_text(destination, ini.str())) {
                MessageBoxW(
                    m_wnd,
                    L"設定ファイルを書き出せませんでした。",
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONERROR);
                return;
            }

            std::wstring message =
                L"設定を書き出しました。\r\n\r\n";
            message += destination.wstring();
            MessageBoxW(
                m_wnd,
                message.c_str(),
                L"NowPlaying Copy & Artwork",
                MB_OK | MB_ICONINFORMATION);
        }

        void import_settings() {
            fs::path sourcePath;
            if (!choose_settings_file(false, sourcePath)) {
                return;
            }

            std::map<std::string, std::string> values;
            std::wstring errorText;
            if (!read_settings_ini(
                    sourcePath,
                    values,
                    errorText)) {
                MessageBoxW(
                    m_wnd,
                    errorText.c_str(),
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONERROR);
                return;
            }

            const auto version = values.find("Version");
            if (version == values.end() ||
                parse_ascii_int(
                    version->second,
                    0,
                    0,
                    100) != 1) {
                MessageBoxW(
                    m_wnd,
                    L"対応していない設定ファイルのバージョンです。",
                    L"NowPlaying Copy & Artwork",
                    MB_OK | MB_ICONERROR);
                return;
            }

            m_initializing = true;

            const int importedTemplateCount = parse_ascii_int(
                settings_value(
                    values,
                    "TemplateCount",
                    std::to_string(template_default_count)),
                static_cast<int>(template_default_count),
                static_cast<int>(template_min_count),
                static_cast<int>(template_max_count));

            m_templateNames.clear();
            m_templateFormats.clear();
            m_templateNames.reserve(
                static_cast<size_t>(importedTemplateCount));
            m_templateFormats.reserve(
                static_cast<size_t>(importedTemplateCount));

            for (int index = 0;
                 index < importedTemplateCount;
                 ++index) {
                const std::string number =
                    std::to_string(index + 1);
                const std::string nameKey =
                    "Template" + number + "Name";
                const std::string formatKey =
                    "Template" + number + "Format";

                std::wstring name = settings_ini_unescape(
                    settings_value(
                        values,
                        nameKey,
                        ""));
                std::wstring format = settings_ini_unescape(
                    settings_value(
                        values,
                        formatKey,
                        ""));
                if (name.empty()) {
                    name = L"テンプレート " +
                        std::to_wstring(index + 1);
                }
                if (format.empty()) {
                    format = utf8_to_wide(default_post_format);
                }
                m_templateNames.push_back(std::move(name));
                m_templateFormats.push_back(std::move(format));
            }

            m_selectedTemplate = parse_ascii_int(
                settings_value(
                    values,
                    "SelectedTemplate",
                    std::to_string(m_selectedTemplate)),
                m_selectedTemplate,
                0,
                importedTemplateCount - 1);

            const auto output = values.find("OutputDirectory");
            if (output != values.end()) {
                SetWindowTextW(
                    m_outputEdit,
                    settings_ini_unescape(
                        output->second).c_str());
            }

            set_checkbox_value(
                m_saveText,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "SaveText",
                        checkbox_value(m_saveText)
                            ? "1"
                            : "0"),
                    checkbox_value(m_saveText)));
            set_checkbox_value(
                m_saveArtwork,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "SaveArtwork",
                        checkbox_value(m_saveArtwork)
                            ? "1"
                            : "0"),
                    checkbox_value(m_saveArtwork)));
            set_checkbox_value(
                m_searchZip,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "SearchZip",
                        checkbox_value(m_searchZip)
                            ? "1"
                            : "0"),
                    checkbox_value(m_searchZip)));
            set_checkbox_value(
                m_showCompletion,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "ShowCompletion",
                        checkbox_value(m_showCompletion)
                            ? "1"
                            : "0"),
                    checkbox_value(m_showCompletion)));
            set_checkbox_value(
                m_showPreview,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "ShowPreview",
                        checkbox_value(m_showPreview)
                            ? "1"
                            : "0"),
                    checkbox_value(m_showPreview)));
            set_checkbox_value(
                m_optimizeArtwork,
                parse_ascii_bool(
                    settings_value(
                        values,
                        "OptimizeArtwork",
                        checkbox_value(m_optimizeArtwork)
                            ? "1"
                            : "0"),
                    checkbox_value(m_optimizeArtwork)));

            const int maxSize = parse_ascii_int(
                settings_value(
                    values,
                    "ArtworkMaxSize",
                    wide_to_utf8(
                        get_window_text_string(
                            m_postMaxSizeEdit))),
                2048,
                256,
                4096);
            SetWindowTextW(
                m_postMaxSizeEdit,
                std::to_wstring(maxSize).c_str());

            const std::string artworkFormat =
                trim_ascii(settings_value(
                    values,
                    "ArtworkFormat",
                    "jpeg"));
            SendMessageW(
                m_postFormatCombo,
                CB_SETCURSEL,
                artworkFormat == "png" ? 1 : 0,
                0);

            const int jpegQuality = parse_ascii_int(
                settings_value(
                    values,
                    "JpegQuality",
                    wide_to_utf8(
                        get_window_text_string(
                            m_jpegQualityEdit))),
                90,
                40,
                100);
            SetWindowTextW(
                m_jpegQualityEdit,
                std::to_wstring(jpegQuality).c_str());

            const int squareMode = parse_ascii_int(
                settings_value(
                    values,
                    "SquareMode",
                    "0"),
                0,
                0,
                1);
            SendMessageW(
                m_squareModeCombo,
                CB_SETCURSEL,
                squareMode,
                0);

            const int historySaveLimit = parse_ascii_int(
                settings_value(
                    values,
                    "HistorySaveLimit",
                    wide_to_utf8(
                        get_window_text_string(
                            m_historyLimitEdit))),
                history_limit_default,
                history_limit_minimum,
                history_limit_maximum);
            SetWindowTextW(
                m_historyLimitEdit,
                std::to_wstring(
                    historySaveLimit).c_str());

            const int historyDisplayLimit =
                parse_ascii_int(
                    settings_value(
                        values,
                        "HistoryDisplayLimit",
                        "100"),
                    100,
                    0,
                    500);
            int historyDisplayIndex = 2;
            switch (historyDisplayLimit) {
            case 0:
                historyDisplayIndex = 0;
                break;
            case 50:
                historyDisplayIndex = 1;
                break;
            case 100:
                historyDisplayIndex = 2;
                break;
            case 200:
                historyDisplayIndex = 3;
                break;
            case 500:
                historyDisplayIndex = 4;
                break;
            default:
                historyDisplayIndex = 2;
                break;
            }
            SendMessageW(
                m_historyDisplayLimitCombo,
                CB_SETCURSEL,
                historyDisplayIndex,
                0);

            populate_template_combo();
            load_current_template_fields();
            update_optimization_controls_enabled();

            m_initializing = false;
            notify_state_changed();

            MessageBoxW(
                m_wnd,
                L"設定を読み込みました。\r\n\r\n"
                L"［適用］または［OK］を押すと保存されます。",
                L"NowPlaying Copy & Artwork",
                MB_OK | MB_ICONINFORMATION);
        }

        static int CALLBACK browse_callback(
            HWND wnd, UINT msg, LPARAM, LPARAM data) {
            if (msg == BFFM_INITIALIZED && data != 0) {
                SendMessageW(wnd, BFFM_SETSELECTIONW, TRUE, data);
            }
            return 0;
        }

        void browse_for_folder() {
            const std::wstring current = get_window_text_string(m_outputEdit);
            BROWSEINFOW info{};
            info.hwndOwner = m_wnd;
            info.lpszTitle = L"NowPlayingの保存先フォルダを選択してください。";
            info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            info.lpfn = browse_callback;
            info.lParam = reinterpret_cast<LPARAM>(current.c_str());
            PIDLIST_ABSOLUTE selected = SHBrowseForFolderW(&info);
            if (selected == nullptr) return;
            wchar_t path[MAX_PATH]{};
            if (SHGetPathFromIDListW(selected, path)) {
                SetWindowTextW(m_outputEdit, path);
            }
            CoTaskMemFree(selected);
        }

        preferences_page_callback::ptr m_callback;
        fb2k::CCoreDarkModeHooks m_dark;
        HWND m_wnd = nullptr;
        HWND m_templateLabel = nullptr;
        HWND m_templateCombo = nullptr;
        HWND m_addTemplateButton = nullptr;
        HWND m_duplicateTemplateButton = nullptr;
        HWND m_deleteTemplateButton = nullptr;
        HWND m_moveTemplateUpButton = nullptr;
        HWND m_moveTemplateDownButton = nullptr;
        HWND m_resetTemplatesButton = nullptr;
        HWND m_nameLabel = nullptr;
        HWND m_nameEdit = nullptr;
        HWND m_formatLabel = nullptr;
        HWND m_formatEdit = nullptr;
        HWND m_formatHint = nullptr;
        HWND m_outputLabel = nullptr;
        HWND m_outputEdit = nullptr;
        HWND m_browseButton = nullptr;
        HWND m_showPreview = nullptr;
        HWND m_saveText = nullptr;
        HWND m_saveArtwork = nullptr;
        HWND m_searchZip = nullptr;
        HWND m_showCompletion = nullptr;
        HWND m_optimizeArtwork = nullptr;
        HWND m_postMaxSizeLabel = nullptr;
        HWND m_postMaxSizeEdit = nullptr;
        HWND m_postFormatLabel = nullptr;
        HWND m_postFormatCombo = nullptr;
        HWND m_jpegQualityLabel = nullptr;
        HWND m_jpegQualityEdit = nullptr;
        HWND m_squareModeLabel = nullptr;
        HWND m_squareModeCombo = nullptr;
        HWND m_historyLimitLabel = nullptr;
        HWND m_historyLimitEdit = nullptr;
        HWND m_historyLimitUnitLabel = nullptr;
        HWND m_historyDisplayLimitLabel = nullptr;
        HWND m_historyDisplayLimitCombo = nullptr;
        HWND m_exportSettingsButton = nullptr;
        HWND m_importSettingsButton = nullptr;
        HWND m_helpButton = nullptr;
        HWND m_note = nullptr;
        std::vector<std::wstring> m_templateNames;
        std::vector<std::wstring> m_templateFormats;
        int m_selectedTemplate = 0;
        bool m_initializing = false;
    };

    class nowplaying_preferences_page : public preferences_page_v3 {
    public:
        preferences_page_instance::ptr instantiate(
            HWND parent,
            preferences_page_callback::ptr callback) override {
            return new service_impl_t<nowplaying_preferences_instance>(
                parent, callback);
        }
        const char* get_name() override {
            return "NowPlaying Copy & Artwork";
        }
        GUID get_guid() override { return guid_preferences_page; }
        GUID get_parent_guid() override {
            return preferences_page::guid_tools;
        }
    };

    static preferences_page_factory_t<nowplaying_preferences_page>
        g_nowplaying_preferences_page_factory;

}

class nowplaying_mainmenu : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 3; }

    GUID get_command(t_uint32 index) override {
        if (index == 0) return guid_nowplaying_command;
        if (index == 1) return guid_nowplaying_history_command;
        if (index == 2) return guid_nowplaying_help_command;
        return pfc::guid_null;
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        if (index == 0) {
            out = "NowPlaying投稿を作成";
        } else if (index == 1) {
            out = "NowPlaying投稿履歴";
        } else if (index == 2) {
            out = "NowPlayingヘルプ";
        }
    }

    bool get_description(
        t_uint32 index,
        pfc::string_base& out
    ) override {
        if (index == 0) {
            out = "再生中の曲から投稿文とアートワークを作成し、プレビューします。";
            return true;
        }
        if (index == 1) {
            out = "保存済みのNowPlaying投稿履歴を開きます。";
            return true;
        }
        if (index == 2) {
            out = "NowPlaying Copy & Artworkの操作方法と設定項目を表示します。";
            return true;
        }
        return false;
    }

    GUID get_parent() override {
        return mainmenu_groups::file;
    }

    void execute(
        t_uint32 index,
        service_ptr_t<service_base>
    ) override {
        if (index == 0) {
            execute_nowplaying();
        } else if (index == 1) {
            nowplaying_history_window::open(output_directory());
        } else if (index == 2) {
            nowplaying_help_window::open();
        }
    }

    bool get_display(
        t_uint32 index,
        pfc::string_base& text,
        t_uint32& flags
    ) override {
        flags = 0;
        if (index >= get_command_count()) return false;
        get_name(index, text);
        return true;
    }
};

static mainmenu_commands_factory_t<nowplaying_mainmenu> g_nowplaying_mainmenu_factory;
