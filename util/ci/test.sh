#!/bin/bash
set -eo pipefail
mapdir=./testmap

msg () {
	echo
	echo "==== $1"
	echo
}

# encodes a block position by X, Y, Z (positive numbers only!)
encodepos () {
	echo "$(($1 + 0x1000 * $2 + 0x1000000 * $3))"
}

# create map file using SQL statements
writemap () {
	rm -rf $mapdir
	mkdir $mapdir
	echo "backend = sqlite3" >$mapdir/world.mt
	echo "default:stone 10 10 10" >$mapdir/colors.txt
	printf '%s\n' \
		"CREATE TABLE d(d BLOB);" \
		"INSERT INTO d VALUES (x'$(cat util/ci/test_block)');" \
		"$1" \
		"DROP TABLE d;" | sqlite3 $mapdir/map.sqlite
}

# check that a non-empty ($1=1) or empty map ($1=0) was written with the args ($2 ...)
checkmap () {
	local c=$1
	shift
	rm -f map.png
	./minetestmapper --noemptyimage -v -i ./testmap -o map.png "$@"
	if [[ $c -eq 1 && ! -f map.png ]]; then
		echo "Output not generated!"
		exit 1
	elif [[ $c -eq 0 && -f map.png ]]; then
		echo "Output was generated, none expected!"
		exit 1
	fi
	echo "Passed."
}

# check that invocation returned an error
checkerr () {
	local r=0
	./minetestmapper --noemptyimage -v -i ./testmap -o map.png "$@" || r=1
	if [ $r -eq 0 ]; then
		echo "Did not return error!"
		exit 1
	fi
	echo "Passed."
}

# this is missing the indices and primary keys but that doesn't matter
schema_old="CREATE TABLE blocks(pos INT, data BLOB);"
schema_new="CREATE TABLE blocks(x INT, y INT, z INT, data BLOB);"

msg "old schema"
writemap "
$schema_old
INSERT INTO blocks SELECT $(encodepos 0 1 0), d FROM d;
"
checkmap 1

msg "old schema: Y limit"
# Note: test data contains a plane at y = 17 an a single node at y = 18
checkmap 1 --max-y 17
checkmap 0 --max-y 16
checkmap 1 --min-y 18
checkmap 0 --min-y 19

# do this for every strategy
for exh in never y full; do
msg "old schema: all limits ($exh)"
# fill the map with more blocks and then request just a single one to be rendered
# this will run through internal consistency asserts.
writemap "
$schema_old
INSERT INTO blocks SELECT $(encodepos 2 2 2), d FROM d;
INSERT INTO blocks SELECT $(encodepos 1 2 2), d FROM d;
INSERT INTO blocks SELECT $(encodepos 2 1 2), d FROM d;
INSERT INTO blocks SELECT $(encodepos 2 2 1), d FROM d;
INSERT INTO blocks SELECT $(encodepos 3 2 2), d FROM d;
INSERT INTO blocks SELECT $(encodepos 2 3 2), d FROM d;
INSERT INTO blocks SELECT $(encodepos 2 2 3), d FROM d;
"
checkmap 1 --geometry 32:32+16+16 --min-y 32 --max-y $((32+16-1)) --exhaustive $exh
done

msg "new schema"
writemap "
$schema_new
INSERT INTO blocks SELECT 0, 1, 0, d FROM d;
"
checkmap 1

# same as above
for exh in never y full; do
msg "new schema: all limits ($exh)"
writemap "
$schema_new
INSERT INTO blocks SELECT 2, 2, 2, d FROM d;
INSERT INTO blocks SELECT 1, 2, 2, d FROM d;
INSERT INTO blocks SELECT 2, 1, 2, d FROM d;
INSERT INTO blocks SELECT 2, 2, 1, d FROM d;
INSERT INTO blocks SELECT 3, 2, 2, d FROM d;
INSERT INTO blocks SELECT 2, 3, 2, d FROM d;
INSERT INTO blocks SELECT 2, 2, 3, d FROM d;
"
checkmap 1 --geometry 32:32+16+16 --min-y 32 --max-y $((32+16-1)) --exhaustive $exh
done

msg "new schema: empty map"
writemap "$schema_new"
checkmap 0

msg "drawplayers"
writemap "
$schema_new
INSERT INTO blocks SELECT 0, 0, 0, d FROM d;
"
mkdir $mapdir/players
printf '%s\n' "name = cat" "position = (80,0,80)" >$mapdir/players/cat
# we can't check that it actually worked, however
checkmap 1 --drawplayers --zoom 4

msg "block error (wrong version)"
writemap "
$schema_new
INSERT INTO blocks VALUES (0, 0, 0, x'150000');
"
checkerr

msg "block error (invalid zstd)"
writemap "
$schema_new
INSERT INTO blocks VALUES (0, 0, 0, x'1d28b52ffd2001090000');
"
checkerr
