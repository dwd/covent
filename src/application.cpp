//
// Created by dwd on 12/27/24.
//

#include "covent/app.h"

using namespace covent;

namespace {
    Application * s_application = nullptr;
}

Application::Application() : m_name("covent") {
    if (s_application) throw std::logic_error("Application already exists");
    s_application = this;
}

Application::Application(std::string name) : m_name(name) {
    if (s_application) throw std::logic_error("Application already exists");
    s_application = this;
}

Application &Application::application() {
    if (!s_application) {
        new Application();
    }
    return *s_application;
}

Application::~Application() {
    if (s_application == this) s_application = nullptr;
}

