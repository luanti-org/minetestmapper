#!/bin/bash -e

install_linux_deps() {
	local upkgs=(
		cmake libgd-dev libsqlite3-dev libleveldb-dev libpq-dev
		libhiredis-dev libzstd-dev
	)
	local fpkgs=(
		cmake gcc-g++ gd-devel sqlite-devel libzstd-devel zlib-ng-devel
	)

	if command -v dnf; then
		sudo dnf install --setopt=install_weak_deps=False -y "${fpkgs[@]}"
	else
		sudo apt-get update
		sudo apt-get install -y --no-install-recommends "${upkgs[@]}"
	fi
}

run_build() {
	local args=(
		-DCMAKE_BUILD_TYPE=Debug
		-DENABLE_LEVELDB=ON -DENABLE_POSTGRESQL=ON -DENABLE_REDIS=ON
	)
	[[ "$CXX" == clang* ]] && args+=(-DCMAKE_CXX_FLAGS="-fsanitize=address")
	cmake . "${args[@]}"

	make -j2
}
