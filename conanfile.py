from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain
from pathlib import Path

class DistKnapsack(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = "build/CMakeUserPresets.json"
        tc.generate()

    def requirements(self):
        if self.requires is None:
            return
        self.requires("fmt/12.1.0")
        self.requires("spdlog/1.17.0")
        self.requires("pybind11/2.13.6")
        self.requires("eigen/5.0.1")
        pass


    def layout(self):
        cmake_layout(self)
        self.folders.build = "build"
        self.folders.generators = "build"
        
