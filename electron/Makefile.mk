# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

# ELECTRON_PKG_NAME determines the default name of app.getPath('appData')
ELECTRON_PKG_NAME	:= Anklang
ELECTRON_VERSION	:= $(version_short)
ELECTRON_DEV		:= false
ELECTRON_SOURCES	:= electron/main.js electron/preload.js electron/htmlgui.svg
ELECTRON_DEPS		:= node_modules/.npm.done
ELECTRON_INSTALLDIR	:= $(pkgdir)/electron
ALL_TARGETS		+= $>/electron/htmlgui

# == electron/executable ==
$>/electron/htmlgui: electron/Makefile.mk $(ELECTRON_SOURCES) $(ELECTRON_DEPS)
	$(QGEN)
	$Q rm -f -r $(@D)
	$Q $(CP) -r node_modules/electron/dist/ $(@D)
	$Q chmod -x $>/electron/lib*.so*
	$Q rm $(@D)/resources/default_app.asar
	$Q mkdir -p $(@D)/resources/app
	$Q $(CP) $(ELECTRON_SOURCES) $(@D)/resources/app/
	$Q echo '{ "private": true,'				>  $(@D)/resources/app/package.json
	$Q echo '  "name": "$(ELECTRON_PKG_NAME)",'		>> $(@D)/resources/app/package.json
	$Q echo '  "version": "$(ELECTRON_VERSION)",'		>> $(@D)/resources/app/package.json
	$Q echo '  "__DEV__": "$(ELECTRON_DEV)",'		>> $(@D)/resources/app/package.json
	$Q echo '  "main": "main.js" }'				>> $(@D)/resources/app/package.json
	$Q chmod g-w -R $>/electron/
	$Q test ! -x node_modules/.bin/asar || ( cd $(@D) \
		&& $(abspath node_modules/.bin/asar) pack resources/app resources/app.asar \
		&& rm -f -r resources/app )
	$Q mv $(@D)/electron $@
# node_modules/.bin/asar list $>/electron/resources/app.asar

# == install ==
electron/install: $>/electron/htmlgui
	@$(QECHO) INSTALL '$(DESTDIR)$(ELECTRON_INSTALLDIR)/'
	$Q rm -f -r '$(DESTDIR)$(ELECTRON_INSTALLDIR)'
	$Q $(INSTALL) -d $(DESTDIR)$(ELECTRON_INSTALLDIR)/
	$Q $(CP) -Rp $>/electron/* $(DESTDIR)$(ELECTRON_INSTALLDIR)
.PHONY: electron/install
install: electron/install

# == uninstall ==
electron/uninstall:
	@$(QECHO) REMOVE '$(DESTDIR)$(ELECTRON_INSTALLDIR)/'
	$Q rm -f -r '$(DESTDIR)$(ELECTRON_INSTALLDIR)'
.PHONY: electron/uninstall
uninstall: electron/uninstall
