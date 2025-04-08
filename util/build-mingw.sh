#!/bin/bash -e

#######
# this expects unpacked libraries and a toolchain file like Luanti's buildbot uses
# $extradlls will typically contain the compiler-specific DLLs and libpng
toolchain_file=
libgd_dir=
zlib_dir=
zstd_dir=
sqlite_dir=
leveldb_dir=
extradlls=(
)
#######

[ -f "$toolchain_file" ] || exit 1
variant=win32
grep -q 'CX?X?_COMPILER.*x86_64-' $toolchain_file && variant=win64
echo "Detected target $variant"

[ -f ./CMakeLists.txt ] || { echo "run from root folder" >&2; exit 1; }

cmake -S . -B build \
	-DCMAKE_TOOLCHAIN_FILE="$toolchain_file" \
	-DCMAKE_EXE_LINKER_FLAGS="-s" \
	\
	-DENABLE_LEVELDB=1 \
	\
	-DLEVELDB_INCLUDE_DIR=$leveldb_dir/include \
	-DLEVELDB_LIBRARY=$leveldb_dir/lib/libleveldb.dll.a \
	-DLIBGD_INCLUDE_DIR=$libgd_dir/include \
	-DLIBGD_LIBRARY=$libgd_dir/lib/libgd.dll.a \
	-DSQLITE3_INCLUDE_DIR=$sqlite_dir/include \
	-DSQLITE3_LIBRARY=$sqlite_dir/lib/libsqlite3.dll.a \
	-DZLIB_INCLUDE_DIR=$zlib_dir/include \
	-DZLIB_LIBRARY=$zlib_dir/lib/libz.dll.a \
	-DZSTD_INCLUDE_DIR=$zstd_dir/include \
	-DZSTD_LIBRARY=$zstd_dir/lib/libzstd.dll.a

make -C build -j4

mkdir pack
cp -p \
	AUTHORS colors.txt COPYING README.rst \
	build/minetestmapper.exe \
	$leveldb_dir/bin/libleveldb.dll \
	$libgd_dir/bin/libgd*.dll \
	$sqlite_dir/bin/libsqlite*.dll \
	$zlib_dir/bin/zlib1.dll \
	$zstd_dir/bin/libzstd.dll \
	"${extradlls[@]}" \
	pack/
zipfile=$PWD/minetestmapper-$variant.zip
(cd pack; zip -9r "$zipfile" *)

rm -rf build pack
echo "Done."
