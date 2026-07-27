/// @file
/// @brief Recovery of the original command line on Windows.
///
/// The `argv` handed to `main` on Windows has already been narrowed to the
/// active code page, which loses any character the code page cannot represent.
/// @ref cli::detail::compute_win32_argv goes back to the process's real command
/// line and re-narrows it as UTF-8 instead.
///
/// This partition contributes nothing on other platforms.

module;

// [CLI11:argv_inl_includes:verbatim]
#if defined(_WIN32)
#if !(defined(_AMD64_) || defined(_X86_) || defined(_ARM_))
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) ||           \
    defined(_M_AMD64)
#define _AMD64_
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__i386__) || defined(_M_IX86)
#define _X86_
#elif defined(__arm__) || defined(_M_ARM) || defined(_M_ARMT)
#define _ARM_
#elif defined(__aarch64__) || defined(_M_ARM64)
#define _ARM64_
#elif defined(_M_ARM64EC)
#define _ARM64EC_
#endif
#endif

#ifndef NOMINMAX
#define NOMINMAX
#include <windef.h>
#undef NOMINMAX
#else
#include <windef.h>
#endif

#include <winbase.h>
#include <processthreadsapi.h>
#include <shellapi.h>
#endif
// [CLI11:argv_inl_includes:end]

export module cli11:argv;

import std;
import :encoding;

export namespace cli
{

    namespace detail
    {

#ifdef _WIN32

        /// @brief Rebuilds the command line as UTF-8, bypassing the active code page.
        ///
        /// Reads the process command line with `GetCommandLineW`, splits it with
        /// `CommandLineToArgvW`, and narrows each argument through @ref cli::narrow.
        ///
        /// @return The arguments, including the program name at index zero.
        /// @throws std::runtime_error If the command line cannot be split.
        auto compute_win32_argv() -> std::vector<std::string>
        {
            std::vector<std::string> result;
            int argc = 0;

            auto deleter = [](wchar_t **ptr) { LocalFree(ptr); };
            // NOLINTBEGIN(*-avoid-c-arrays)
            auto wargv =
                std::unique_ptr<wchar_t *[], decltype(deleter)>(CommandLineToArgvW(GetCommandLineW(), &argc), deleter);
            // NOLINTEND(*-avoid-c-arrays)

            if (wargv == nullptr)
            {
                throw std::runtime_error(std::format("CommandLineToArgvW failed with code {}", GetLastError()));
            }

            const auto args = std::span {wargv.get(), static_cast<std::size_t>(argc)};
            result.reserve(args.size());
            for (wchar_t *arg : args)
            {
                result.push_back(narrow(arg));
            }

            return result;
        }

#endif

    } // namespace detail

} // namespace cli
