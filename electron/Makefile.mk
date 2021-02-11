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
	$Q echo '  "name": "Anklang",'					>> $(@D)/resources/app/package.json
	$Q echo '  "version": "$(VERSION_M.M.M)",'			>> $(@D)/resources/app/package.json
	$Q echo '  "main": "main.js",'					>> $(@D)/resources/app/package.json
	$Q echo '  "mode": "$(MODE)" }'					>> $(@D)/resources/app/package.json
	$Q mv $(@D)/electron $@
electron/all: $>/electron/anklang
