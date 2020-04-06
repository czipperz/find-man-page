#include <cz/buffer_array.hpp>
#include <cz/str.hpp>
#include <cz/string.hpp>
#include <cz/vector.hpp>

struct Lookup_Context {
    cz::Buffer_Array local_results_buffer_array;
    cz::String directory;
    cz::Vector<cz::Str> files;

    void create() { local_results_buffer_array.create(); }
    void drop() {
        local_results_buffer_array.drop();
        directory.drop(cz::heap_allocator());
        files.drop(cz::heap_allocator());
    }
};
