# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

CLEANDIRS += $>/external/

# == external/rapidjson ==
$>/external/rapidjson/rapidjson.h: misc/config-external.mk		| $>/external/
	@ $(eval S := 9a827f29371c4f17f831a682d18ca5bc8632fcdf5031fd3851fd68ba182db69c https://github.com/Tencent/rapidjson/archive/0ccdbf364c577803e2a751f5aededce935314313.tar.gz)
	@ $(eval H := $(firstword $(S))) $(eval U := $(lastword $(S))) $(eval T := $(notdir $(U)))
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf rapidjson* \
	   $(call AND_DOWNLOAD_SHAURL, $H, $U) && tar xf $T && rm $T
	$Q ln -s rapidjson-$(T:.tar.gz=)/include/rapidjson $>/external/rapidjson
	$Q test -e $@ && touch $@
ALL_TARGETS += $>/external/rapidjson/rapidjson.h

# == external/websocketpp ==
$>/external/websocketpp/server.hpp: misc/config-external.mk		| $>/external/
	@ $(eval S := 6ce889d85ecdc2d8fa07408d6787e7352510750daa66b5ad44aacb47bea76755 https://github.com/zaphoyd/websocketpp/archive/0.8.2.tar.gz)
	@ $(eval H := $(firstword $(S))) $(eval U := $(lastword $(S))) $(eval T := $(notdir $(U)))
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf websocketpp* \
	   $(call AND_DOWNLOAD_SHAURL, $H, $U) && tar xf $T && rm $T
	$Q ln -s websocketpp-$(T:.tar.gz=)/websocketpp $>/external/websocketpp
	$Q test -e $@ && touch $@
$>/external/websocketpp/config/asio_no_tls.hpp: $>/external/websocketpp/server.hpp
