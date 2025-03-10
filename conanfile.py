from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMake, CMakeDeps
from conan.tools.build import check_min_cppstd

class ConanApplication(ConanFile):
    name = "covent"
    version = "0.1.10"
    email = "Dave Cridland <dave@cridland.net>"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "tests": [True, False],
        "sentry": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "tests": True,
        "sentry": True,
        "shared": False,
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
        tc.variables["COVENT_SENTRY"] = self.options.sentry
        tc.variables["COVENT_BUILD_TESTS"] = self.options.tests
        tc.user_presets_path = False
        tc.generate()

    def configure(self):
        if self.options.sentry:
            self.options["sentry-native"].backend = "inproc"

    def build(self):
        tc = CMake(self)
        tc.configure()
        tc.build()

    def requirements(self):
        requirements = self.conan_data.get('requirements', [])
        for requirement in requirements:
            self.requires(requirement)
        if self.options.sentry:
            self.requires("sentry-native/0.7.11")
        if self.options.tests:
            self.requires("gtest/1.12.1")

    def package(self):
        tc = CMake(self)
        tc.install()

    def package_info(self):
        self.cpp_info.libs = ["Covent::covent_static"]