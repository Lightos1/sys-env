#   make build everything and populate dist/
#   make dist same as above
#   make sysmodule build only the sysmodule
#   make overlay build only the overlay
#   make clean remove build artifacts and dist/

PROGRAM_ID := 420000000000042A

DIST        := dist
CONTENTS    := $(DIST)/atmosphere/contents/$(PROGRAM_ID)
OVERLAYS    := $(DIST)/switch/.overlays

NSP         := sysmodule/sys-env.nsp
OVL         := overlay/sys-env.ovl
TOOLBOX     := sysmodule/toolbox.json

.PHONY: all dist sysmodule overlay clean

all: dist

sysmodule:
	@echo ":: building sysmodule"
	@$(MAKE) -C sysmodule

overlay:
	@echo ":: building overlay"
	@$(MAKE) -C overlay

dist: sysmodule overlay
	@rm -rf $(DIST)
	@mkdir -p $(CONTENTS)/flags $(OVERLAYS)
	@cp $(NSP) $(CONTENTS)/exefs.nsp
	@cp $(TOOLBOX) $(CONTENTS)/toolbox.json
	@touch $(CONTENTS)/flags/boot2.flag
	@cp $(OVL) $(OVERLAYS)/sys-env.ovl
	@echo ""
	@echo ":: Done building"
	@cd $(DIST) && find . -type f | sed 's|^\.|   |'

clean:
	@echo ":: cleaning"
	@$(MAKE) -C sysmodule clean
	@$(MAKE) -C overlay clean
	@rm -rf $(DIST)
