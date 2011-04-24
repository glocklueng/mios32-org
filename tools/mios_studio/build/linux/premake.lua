
project.name = "MiosStudio"
project.bindir = "build"
project.libdir = "build"

project.configs = { "Debug", "Release" }

package = newpackage()
package.name = "MiosStudio"
package.kind = "winexe"
package.language = "c++"

package.objdir = "build/intermediate"
package.config["Debug"].objdir   = "build/intermediate/Debug"
package.config["Release"].objdir = "build/intermediate/Release"

package.config["Debug"].defines     = { "LINUX=1", "DEBUG=1", "_DEBUG=1" };
package.config["Debug"].buildoptions = { "-D_DEBUG -ggdb" }

package.config["Release"].defines   = { "LINUX=1", "NDEBUG=1" };

package.target = "MiosStudio"

package.includepaths = { 
    "/usr/include",
    "/usr/include/freetype2"
}

package.libpaths = { 
    "/usr/X11R6/lib/",
    "../../../../bin"
}

package.config["Debug"].links = { 
    "freetype", "pthread", "rt", "X11", "GL", "asound"
}

package.config["Release"].links = { 
    "freetype", "pthread", "rt", "X11", "GL", "asound"
}

package.linkflags = { "static-runtime" }

package.files = { matchfiles (
    "../../src/*.h",
    "../../src/*.cpp",
    "../../src/gui/*.h",
    "../../src/gui/*.cpp"
    )
}
