# astral_embed_file(<target> <file> <symbol>)
#   Compiles <file> into <target> as `astral::embedded::<symbol>` (const std::uint8_t[]) and
#   `<symbol>_size`. For small assets the engine ships inside the binary (the default font).
function(astral_embed_file target file symbol)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/astral_embed/${symbol}.cpp)
    file(READ ${file} hex HEX)
    string(LENGTH "${hex}" chars)
    math(EXPR size "${chars} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x..,){32})" "\\1\n" bytes "${bytes}")
    file(WRITE ${out}.tmp
        "// Generated from ${file} by astral_embed_file. Do not edit.\n"
        "#include <cstddef>\n#include <cstdint>\nnamespace astral::embedded {\n"
        "extern const std::uint8_t ${symbol}[];\nextern const std::size_t ${symbol}_size;\n"
        "const std::uint8_t ${symbol}[] = {\n${bytes}\n};\nconst std::size_t ${symbol}_size = ${size};\n}\n")
    configure_file(${out}.tmp ${out} COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${file})
    target_sources(${target} PRIVATE ${out})
endfunction()
