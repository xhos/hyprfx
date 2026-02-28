# --no-gnu-unique: prevents GCC optimization that breaks plugin unload/reload
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b
PKG_CONFIG = `pkg-config --cflags pixman-1 libdrm hyprland libinput libudev wayland-server xkbcommon`

# embed shader files as C byte arrays
define embed_file
	@echo "static const unsigned char $(1)[] = {" >> $(3)
	@od -An -tx1 -v $(2) | sed 's/[[:space:]]*$$//;s/[[:space:]][[:space:]]*/,0x/g;s/^,/  /;s/$$/,/' >> $(3)
	@echo "};" >> $(3)
	@echo "static const unsigned int $(1)_LEN = sizeof($(1));" >> $(3)
endef

all: src/shader_data.inc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) src/main.cpp src/FxPassElement.cpp -o hyprfx.so $(PKG_CONFIG)

src/shader_data.inc: shaders/default.vert shaders/aura-glow.frag
	@echo "// auto-generated from shaders/ — do not edit" > $@
	$(call embed_file,VERT_DATA,shaders/default.vert,$@)
	$(call embed_file,FRAG_DATA,shaders/aura-glow.frag,$@)

clean:
	rm -f ./hyprfx.so src/shader_data.inc
