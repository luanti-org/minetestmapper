#!/bin/bash -e

install_linux_deps() {
	local pkgs=(cmake libgd-dev libsqlite3-dev libleveldb-dev libpq-dev libhiredis-dev libzstd-dev)

	sudo apt-get update
	sudo apt-get remove -y 'libgd3' nginx || : # ????
	sudo apt-get install -y --no-install-recommends "${pkgs[@]}" "$@"
}

run_build() {
	local args=(
		-DCMAKE_BUILD_TYPE=Debug
		-DENABLE_LEVELDB=1 -DENABLE_POSTGRESQL=1 -DENABLE_REDIS=1
	)
	[[ "$CXX" == clang* ]] && args+=(-DCMAKE_CXX_FLAGS="-fsanitize=address")
	cmake . "${args[@]}"

	make -j2
}
