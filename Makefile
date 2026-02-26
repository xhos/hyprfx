# --no-gnu-unique: prevents GCC optimization that breaks plugin unload/reload
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) src/main.cpp src/FxPassElement.cpp -o hyprfx.so `pkg-config --cflags pixman-1 libdrm hyprland libinput libudev wayland-server xkbcommon`

clean:
	rm -f ./hyprfx.so
