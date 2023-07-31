# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/electron/*.d)
electron/cleandirs ::= $(wildcard $>/electron/)
CLEANDIRS         += $(electron/cleandirs)
ALL_TARGETS       += electron/all
electron/all:

electron/js.sources ::= electron/main.js electron/preload.js $>/ui/anklang.png

# == electron/anklang ==
$>/electron/anklang: $(electron/js.sources) electron/Makefile.mk $>/node_modules/.npm.done
	$(QGEN)
	$Q rm -f -r $(@D)
	$Q $(CP) -r $>/node_modules/electron/dist/ $(@D)
	$Q chmod -x $>/electron/lib*.so*
	$Q rm $(@D)/resources/default_app.asar
	$Q mkdir -p $(@D)/resources/app
	$Q $(CP) $(electron/js.sources) $(@D)/resources/app/
	$Q echo '{ "private": true,'					>  $(@D)/resources/app/package.json
	$Q echo '  "name": "anklang/electron",'				>> $(@D)/resources/app/package.json
	$Q echo '  "version": "$(version_short)",'			>> $(@D)/resources/app/package.json
	$Q echo '  "main": "main.js",'					>> $(@D)/resources/app/package.json
	$Q echo '  "mode": "$(MODE)" }'					>> $(@D)/resources/app/package.json
	$Q chmod g-w -R $>/electron/
	$Q mv $(@D)/electron $@
electron/all: $>/electron/anklang

# == installation ==
electron/installdir ::= $(pkgdir)/electron
.PHONY: electron/install
electron/install: $>/electron/anklang
	@$(QECHO) INSTALL '$(DESTDIR)$(electron/installdir)/.'
	$Q rm -f -r '$(DESTDIR)$(electron/installdir)'
	$Q $(INSTALL) -d $(DESTDIR)$(electron/installdir)/
	$Q $(CP) -Rp $>/electron/* $(DESTDIR)$(electron/installdir)
	$Q $(call INSTALL_SYMLINK, '../electron/anklang', '$(DESTDIR)$(pkgdir)/bin/anklang')
	$Q $(INSTALL) -d '$(DESTDIR)$(bindir)/'
	$Q rm -f '$(DESTDIR)$(bindir)/anklang'
	$Q ln -s -r '$(DESTDIR)$(pkgdir)/bin/anklang' '$(DESTDIR)$(bindir)/anklang'
install: electron/install
.PHONY: electron/uninstall
electron/uninstall:
	@$(QECHO) REMOVE '$(DESTDIR)$(electron/installdir)/.'
	$Q rm -f -r '$(DESTDIR)$(electron/installdir)'
	$Q rm -f '$(DESTDIR)$(pkgdir)/bin/anklang'
	$Q $(RMDIR_P) '$(DESTDIR)$(pkgdir)/bin' || true
	$Q rm -f '$(DESTDIR)$(bindir)/anklang'
uninstall: electron/uninstall
