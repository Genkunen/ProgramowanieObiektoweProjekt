#include "vulkan/renderer/radix_sort.hpp"
#include "vulkan_quick_setup.hpp"

#include <print>

void vk_sanity() {
    using namespace pop::vulkan;
    std::println("Vulkan sanity OK");
    
    std::println(" -> Radix sort histogram and scatter shader execution mode: Wave{}", renderer::get_radix_sort_group_size());
}

int main(int argc, char* argv[]) {
    vulkan_quick_setup(vk_sanity);
}