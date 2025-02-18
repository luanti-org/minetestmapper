#!/bin/bash
set -eo pipefail

msg () {
	echo
	echo "==== $1"
	echo
}

# encodes a block position by X, Y, Z (positive numbers only!)
encodepos () {
	echo "$(($1 + 0x1000 * $2 + 0x1000000 * $3))"
}

# create map file with sql statements
writemap () {
	mkdir -p testmap
	echo "backend = sqlite3" >testmap/world.mt
	echo "default:stone 255 0 0" >testmap/colors.txt
	rm -f testmap/map.sqlite
	printf '%s\n' \
		"CREATE TABLE d(d BLOB);" \
		"INSERT INTO d VALUES (x'$(cat util/ci/test_block)');" \
		"$1" \
		"DROP TABLE d;" | sqlite3 testmap/map.sqlite
}

# check that a non-empty ($1=1) or empty map ($1=0) was written with the args ($2 ...)
checkmap () {
	local c=$1
	shift
	rm -f map.png
	./minetestmapper --noemptyimage -i ./testmap -o map.png "$@"
	if [[ $c -eq 1 && ! -f map.png ]]; then
		echo "Output not generated!"
		exit 1
	elif [[ $c -eq 0 && -f map.png ]]; then
		echo "Output was generated, none expected!"
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

msg "old schema: all limits"
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
checkmap 1 --geometry 32:32+16+16 --min-y 32 --max-y $((32+16-1))

msg "new schema"
writemap "
$schema_new
INSERT INTO blocks SELECT 0, 1, 0, d FROM d;
"
checkmap 1

msg "new schema: all limits"
# same as above
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
checkmap 1 --geometry 32:32+16+16 --min-y 32 --max-y $((32+16-1))

msg "new schema: empty map"
writemap "$schema_new"
checkmap 0
