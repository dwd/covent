//
// Created by dwd on 12/27/24.
//

#ifndef APP_H
#define APP_H

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace covent {
    class Application {
    public:
        Application();

        explicit Application(std::string name);

        Application(Application &&) = delete;

        Application(Application const &) = delete;

        static Application &application();

        template<typename ...Args>
        [[nodiscard]] std::shared_ptr<spdlog::logger> constexpr logger(fmt::format_string<Args...> fmt_str, Args... args) const {
            std::string logger_name = fmt::vformat(fmt_str, fmt::make_format_args(args...));
            return spdlog::default_logger()->clone(logger_name);
        }
        auto const & name() const {
            return m_name;
        }

        ~Application();

    private:
        std::string m_name;
    };
}

#endif //APP_H
