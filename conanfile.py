from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMake, CMakeDeps
from conan.tools.build import check_min_cppstd

class ConanApplication(ConanFile):
    name = "covent"
    email = "Dave Cridland <dave@cridland.net>"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "tests": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "tests": False,
        "shared": False,
        "unbound/*:shared": False,
    }

    exports_sources = "src/*", "CMakeLists.txt", "include/*", "test/*"

    def validate(self):
        check_min_cppstd(self, "20")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["COVENT_BUILD_TESTS"] = self.options.tests
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.user_presets_path = False
        tc.generate()

    def configure(self):
        self.options["sentry-native"].backend = "inproc"
        if self.options.shared:
            for dep in 'openssl', 'yaml-cpp', 'sentry-native', 'libevent', 'icu':
                self.options[dep].shared = True


    def build(self):
        tc = CMake(self)
        tc.configure()
        tc.build()

    def requirements(self):
        requirements = self.conan_data.get('requirements', [])
        for requirement in requirements:
            self.requires(requirement)
        if self.options.tests:
            self.requires("gtest/1.12.1")

    def package(self):
        tc = CMake(self)
        tc.install()

    def package_info(self):
        self.cpp_info.names["cmake_find_package"] = "covent"
        self.cpp_info.names["cmake_find_package_multi"] = "covent"
        self.cpp_info.set_property("cmake_target_name", "covent::covent")
        self.cpp_info.libs = ["covent"]
