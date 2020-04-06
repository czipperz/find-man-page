#include <stdio.h>
#include <cz/allocator.hpp>
#include <cz/heap.hpp>
#include <cz/str.hpp>
#include <cz/vector.hpp>

static int get_man_paths(cz::Allocator path_allocator,
                         cz::Vector<cz::Str>* paths,
                         cz::Allocator paths_allocator) {
    FILE* manpath_file = popen("manpath", "r");
    if (!manpath_file) {
        perror("Couldn't retrieve the manpath");
        return 1;
    }

    cz::String buffer = {};
    CZ_DEFER(buffer.drop(cz::heap_allocator()));
    while (1) {
        buffer.reserve(cz::heap_allocator(), 1024);
        size_t read_len = fread(buffer.end(), 1, 1024, manpath_file);
        if (read_len == 0) {
            break;
        }
        buffer.set_len(buffer.len() + read_len);

        char* start = buffer.start();
        while (start < buffer.end()) {
            char* end = (char*)memchr(start, ':', buffer.end() - start);
            if (!end && buffer.end()[-1] == '\n') {
                end = buffer.end() - 1;
            }
            if (end) {
                paths->reserve(paths_allocator, 1);
                paths->push(cz::Str{start, (size_t)(end - start)}.duplicate(path_allocator));
                start = end + 1;
            } else {
                memmove(buffer.start(), end, buffer.end() - end);
                buffer.set_len(buffer.end() - end);
                break;
            }
        }
    }

    pclose(manpath_file);
    return 0;
}
