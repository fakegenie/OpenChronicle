file(READ "${INPUT}" bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
file(WRITE "${OUTPUT}" "#include <stddef.h>\nconst unsigned char ${NAME}[] = {${bytes}};\nconst size_t ${NAME}_size = sizeof ${NAME};\n")
