from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain


class DistKnapsack(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = ["fmt/12.1.0", "spdlog/1.17.0"]
    generators = "CMakeDeps"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = "build/CMakeUserPresets.json"
        tc.generate()

    def layout(self):
        cmake_layout(self)
        self.folders.build = "build"
        self.folders.generators = "build"
        