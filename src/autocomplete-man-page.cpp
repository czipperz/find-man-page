#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <cz/buffer_array.hpp>
#include <cz/defer.hpp>
#include <cz/fs/directory.hpp>
#include <cz/heap.hpp>
#include <cz/str.hpp>
#include <cz/string.hpp>
#include "lookup_context.cpp"

static void lookup_man_page(cz::Slice<cz::Str> man_paths, cz::Str man_page) {
    Lookup_Context context = {};
    context.create();
    CZ_DEFER(context.drop());

    for (size_t man_path_index = 0; man_path_index < man_paths.len; ++man_path_index) {
        cz::Str directory_base = man_paths[man_path_index];

        context.directory.set_len(0);
        context.directory.reserve(cz::heap_allocator(), directory_base.len + 6);
        context.directory.append(directory_base);
        context.directory.append("/man0");
        context.directory.null_terminate();

        for (int man_index = 0; man_index <= 8; ++man_index) {
            context.local_results_buffer_array.clear();
            context.directory[context.directory.len() - 1] = man_index + '0';

            context.files.set_len(0);
            cz::Result files_result =
                cz::fs::files(cz::heap_allocator(), context.local_results_buffer_array.allocator(),
                              context.directory.buffer(), &context.files);
            if (files_result.is_err()) {
                continue;
            }

            for (size_t i = 0; i < context.files.len(); ++i) {
                cz::Str file = context.files[i];
                if (file.starts_with(man_page)) {
                    const char* second_end = file.rfind('.');
                    if (second_end) {
                        const char* first_end =
                            cz::Str{file.buffer, (size_t)(second_end - file.buffer)}.rfind('.');
                        if (first_end) {
                            fwrite(file.buffer, 1, first_end - file.buffer, stdout);
                            putchar('\n');
                        }
                    }
                }
            }
        }
    }
}

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

int main(int argc, char** argv) {
    if (argc == 1) {
        fprintf(stderr, "Usage: %s <PAGE>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Example: %s read\n", argv[0]);
        return 1;
    }

    char* man_page = argv[1];

    cz::Vector<cz::Str> man_paths = {};
    CZ_DEFER(man_paths.drop(cz::heap_allocator()));

    cz::Buffer_Array man_paths_buffer_array = {};
    man_paths_buffer_array.create();
    CZ_DEFER(man_paths_buffer_array.drop());

    if (get_man_paths(man_paths_buffer_array.allocator(), &man_paths, cz::heap_allocator())) {
        return 1;
    }

    cz::Buffer_Array results_buffer_array = {};
    results_buffer_array.create();
    CZ_DEFER(results_buffer_array.drop());

    cz::Vector<cz::Str> results = {};
    CZ_DEFER(results.drop(cz::heap_allocator()));

    lookup_man_page(man_paths, man_page);
    return 0;
}
