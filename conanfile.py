# This file is managed by Conan, contents will be overwritten.
# To keep your changes, remove these comment lines, but the plugin won't be able to modify your requirements

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain
from conan.tools.build import check_min_cppstd

class ConanApplication(ConanFile):
    name = "covent"
    version = "0.1.0"
    email = "Dave Cridland <dave@cridland.net>"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    options = {
        "tests": [True, False],
        "sentry": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "tests": True,
        "sentry": True,
        "shared": True,
    }

    def validate(self):
        check_min_cppstd(self, "20")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["COVENT_SENTRY"] = self.options.sentry
        tc.variables["COVENT_BUILD_TESTS"] = self.options.tests
        tc.user_presets_path = False
        tc.generate()

    def configure(self):
        if self.options.sentry:
            self.options["sentry-native"].backend = "inproc"

    def requirements(self):
        requirements = self.conan_data.get('requirements', [])
        for requirement in requirements:
            self.requires(requirement)
        if self.options.sentry:
            self.requires("sentry-native/0.7.11")
        if self.options.tests:
            self.requires("gtest/1.12.1")
