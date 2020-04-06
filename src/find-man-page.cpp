#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <cz/buffer_array.hpp>
#include <cz/defer.hpp>
#include <cz/fs/directory.hpp>
#include <cz/heap.hpp>
#include <cz/str.hpp>
#include <cz/string.hpp>
#include "get_man_paths.cpp"
#include "lookup_context.cpp"

static int decompress_gz(FILE* file, cz::String* out, cz::Allocator out_allocator) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.next_in = 0;
    stream.avail_in = 0;
    // Add together the amount of memory to be used (8..15) and detection scheme (32 = automatic).
    int window_bits = 15 + 32;
    int ret = inflateInit2(&stream, window_bits);
    if (ret != Z_OK) {
        return 1;
    }

    char input_buffer[1 << 12];

    while (1) {
        size_t read_len = fread(input_buffer, 1, sizeof(input_buffer), file);
        if (read_len == 0) {
            inflateEnd(&stream);
            return 0;
        }

        stream.next_in = (unsigned char*)input_buffer;
        stream.avail_in = read_len;

        do {
            out->reserve(out_allocator, 1 << 10);
            stream.next_out = (unsigned char*)out->end();
            stream.avail_out = out->cap() - out->len();
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret == Z_OK) {
                out->set_len(out->cap() - stream.avail_out);
            } else if (ret == Z_STREAM_END) {
                out->set_len(out->cap() - stream.avail_out);
                inflateEnd(&stream);
                return 0;
            } else if (ret == Z_BUF_ERROR) {
                out->reserve(out_allocator, (out->cap() - out->len()) * 2);
            } else {
                fprintf(stderr, "Inflation error: %s %d\n", stream.msg, ret);
                inflateEnd(&stream);
                return 1;
            }
        } while (stream.avail_out == 0);
    }
}

static void add_result(cz::Slice<cz::Str> man_paths,
                       cz::String path,
                       cz::Vector<cz::Str>* results,
                       cz::Allocator result_allocator);

static void lookup_specific_man_page(cz::Slice<cz::Str> man_paths,
                                     cz::Str file_to_find,
                                     cz::Vector<cz::Str>* results,
                                     cz::Allocator result_allocator,
                                     Lookup_Context* context) {
    // file_to_find looks like "man2/man_page.2"
    cz::Str after_slash = {file_to_find.buffer + 5, file_to_find.len - 5};

    for (size_t man_path_index = 0; man_path_index < man_paths.len; ++man_path_index) {
        cz::Str directory_base = man_paths[man_path_index];

        context->directory.set_len(0);
        context->directory.reserve(cz::heap_allocator(), directory_base.len + 6);
        context->directory.append(directory_base);
        context->directory.append("/man");
        context->directory.push(file_to_find[3]);
        context->directory.null_terminate();

        context->local_results_buffer_array.clear();

        context->files.set_len(0);
        cz::Result files_result =
            cz::fs::files(cz::heap_allocator(), context->local_results_buffer_array.allocator(),
                          context->directory.buffer(), &context->files);
        if (files_result.is_err()) {
            continue;
        }

        for (size_t i = 0; i < context->files.len(); ++i) {
            cz::Str file = context->files[i];
            if (file.starts_with(after_slash)) {
                cz::String path = {};
                path.reserve(result_allocator, context->directory.len() + file.len + 2);
                path.append(context->directory);
                path.push('/');
                path.append(file);
                path.null_terminate();
                add_result(man_paths, path, results, result_allocator);
            }
        }
    }
}

static void add_result(cz::Slice<cz::Str> man_paths,
                       cz::String path,
                       cz::Vector<cz::Str>* results,
                       cz::Allocator result_allocator) {
    cz::String contents = {};
    CZ_DEFER(contents.drop(cz::heap_allocator()));

    {
        FILE* file = fopen(path.buffer(), "r");
        if (!file) {
            path.drop(result_allocator);
            return;
        }
        CZ_DEFER(fclose(file));

        if (decompress_gz(file, &contents, cz::heap_allocator())) {
            path.drop(result_allocator);
            return;
        }
    }

    if (contents.starts_with(".so ")) {
        path.drop(result_allocator);

        cz::Str file_to_find = {contents.buffer() + 4, contents.len() - 5};
        Lookup_Context context = {};
        context.create();
        CZ_DEFER(context.drop());
        lookup_specific_man_page(man_paths, file_to_find, results, result_allocator, &context);
    } else {
        results->reserve(cz::heap_allocator(), 1);
        results->push(path);
    }
}

static void lookup_man_page(cz::Slice<cz::Str> man_paths,
                            cz::Str man_page,
                            cz::Vector<cz::Str>* results,
                            cz::Allocator result_allocator,
                            Lookup_Context* context) {
    cz::String man_page_dot = {};
    man_page_dot.reserve(cz::heap_allocator(), man_page.len + 1);
    CZ_DEFER(man_page_dot.drop(cz::heap_allocator()));
    man_page_dot.append(man_page);
    man_page_dot.push('.');

    for (size_t man_path_index = 0; man_path_index < man_paths.len; ++man_path_index) {
        cz::Str directory_base = man_paths[man_path_index];

        context->directory.set_len(0);
        context->directory.reserve(cz::heap_allocator(), directory_base.len + 6);
        context->directory.append(directory_base);
        context->directory.append("/man0");
        context->directory.null_terminate();

        for (int man_index = 0; man_index <= 8; ++man_index) {
            context->local_results_buffer_array.clear();
            context->directory[context->directory.len() - 1] = man_index + '0';

            context->files.set_len(0);
            cz::Result files_result =
                cz::fs::files(cz::heap_allocator(), context->local_results_buffer_array.allocator(),
                              context->directory.buffer(), &context->files);
            if (files_result.is_err()) {
                continue;
            }

            for (size_t i = 0; i < context->files.len(); ++i) {
                cz::Str file = context->files[i];
                if (file.starts_with(man_page_dot)) {
                    cz::String path = {};
                    path.reserve(result_allocator, context->directory.len() + file.len + 2);
                    path.append(context->directory);
                    path.push('/');
                    path.append(file);
                    path.null_terminate();
                    add_result(man_paths, path, results, result_allocator);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc == 1) {
        fprintf(stderr, "Usage: %s <PAGE>\n", argv[0]);
        fprintf(stderr, "   or: %s <PAGE>.<SECTION>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Example 1: %s read\n", argv[0]);
        fprintf(stderr, "Example 2: %s read.2\n", argv[0]);
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

    Lookup_Context context = {};
    context.create();
    CZ_DEFER(context.drop());

    lookup_man_page(man_paths, man_page, &results, results_buffer_array.allocator(), &context);

    for (size_t i = 0; i < results.len(); ++i) {
        fwrite(results[i].buffer, 1, results[i].len, stdout);
        putchar('\n');
    }

    if (results.len() == 0) {
        fprintf(stderr, "Error: No results\n");
        return 1;
    }

    return 0;
}
