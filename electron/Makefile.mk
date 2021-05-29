# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/electron/*.d)
electron/cleandirs ::= $(wildcard $>/electron/)
CLEANDIRS         += $(electron/cleandirs)
ALL_TARGETS       += electron/all
electron/all:

electron/js.sources ::= electron/main.js electron/preload.js

# == electron/anklang ==
$>/electron/anklang: $(electron/js.sources) electron/Makefile.mk $>/node_modules/.npm.done
	$(QGEN)
	$Q rm -f -r $(@D)
	$Q $(CP) -r $>/node_modules/electron/dist/ $(@D)
	$Q rm $(@D)/resources/default_app.asar
	$Q mkdir -p $(@D)/resources/app
	$Q $(CP) $(electron/js.sources) $(@D)/resources/app/
	$Q echo '{ "private": true,'					>  $(@D)/resources/app/package.json
	$Q echo '  "name": "anklang/electron",'				>> $(@D)/resources/app/package.json
	$Q echo '  "version": "$(version_m.m.m)",'			>> $(@D)/resources/app/package.json
	$Q echo '  "main": "main.js",'					>> $(@D)/resources/app/package.json
	$Q echo '  "mode": "$(MODE)" }'					>> $(@D)/resources/app/package.json
	$Q mv $(@D)/electron $@
electron/all: $>/electron/anklang

# == installation ==
electron/installdir ::= $(DESTDIR)$(pkglibdir)/electron
.PHONY: electron/install
electron/install: $>/electron/anklang
	@$(QECHO) INSTALL '$(electron/installdir)/...'
	$Q rm -f -r '$(electron/installdir)'
	$Q $(INSTALL) -d $(electron/installdir)/
	$Q $(CP) -Rp $>/electron/* $(electron/installdir)
	$Q $(call INSTALL_SYMLINK, '../electron/anklang', '$(DESTDIR)$(pkglibdir)/bin/anklang')
install: electron/install
.PHONY: electron/uninstall
electron/uninstall:
	@$(QECHO) REMOVE '$(electron/installdir)/...'
	$Q rm -f -r '$(electron/installdir)'
	$Q rm -f '$(DESTDIR)$(pkglibdir)/bin/anklang'
	$Q $(RMDIR_P) '$(DESTDIR)$(pkglibdir)/bin' ; :
uninstall: electron/uninstall
