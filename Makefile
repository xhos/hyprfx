# --no-gnu-unique: prevents GCC optimization that breaks plugin unload/reload
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b
PKG_CONFIG = `pkg-config --cflags pixman-1 libdrm hyprland libinput libudev wayland-server xkbcommon`

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) src/main.cpp src/FxPassElement.cpp src/ShaderLoader.cpp src/Animation.cpp -o hyprfx.so $(PKG_CONFIG)

clean:
	rm -f ./hyprfx.so
