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
