#include "vulkan_quick_setup.hpp"
#include "vulkan/renderer/simulation_render_graph_passes.hpp"
#include <print>

void instrumentate_pass_compile_call(const std::string_view& label, const std::function<void()>& fn) {
    std::println("Compiling render graph pass {}...", label);
    fn();
}

void pass_compile_test() {
    using namespace pop::vulkan::renderer;
    instrumentate_pass_compile_call("UploadMeshInfoPass",                         [](){ std::make_unique<UploadMeshParamsPass                      >(UploadMeshParamsPass::create());});
    instrumentate_pass_compile_call("IndirectDrawCommandsClearPass",              [](){ std::make_unique<IndirectDrawCommandsInstanceCountClearPass>(IndirectDrawCommandsInstanceCountClearPass::create());});
    instrumentate_pass_compile_call("SimulationAccelerationGridBoundClearPass",   [](){ std::make_unique<SimulationAccelerationGridBoundClearPass  >(SimulationAccelerationGridBoundClearPass::create());});
    instrumentate_pass_compile_call("RandomEventsPass",                           [](){ std::make_unique<RandomEventsPass                          >(RandomEventsPass::create());});
    instrumentate_pass_compile_call("SimulationStepPass",                         [](){ std::make_unique<SimulationInternalStepPass                >(SimulationInternalStepPass::create());});
    instrumentate_pass_compile_call("SimulationAccelerationGridSortPreparePass",  [](){ std::make_unique<SimulationAccelerationGridSortPreparePass >(SimulationAccelerationGridSortPreparePass::create());});
    instrumentate_pass_compile_call("SimulationAccelerationGridRadixSortPass",    [](){ std::make_unique<SimulationAccelerationGridRadixSortPass   >(SimulationAccelerationGridRadixSortPass::create());});
    instrumentate_pass_compile_call("SimulationAccelerationGridBoundScanPass",    [](){ std::make_unique<SimulationAccelerationGridBoundScanPass   >(SimulationAccelerationGridBoundScanPass::create());});
    instrumentate_pass_compile_call("IndirectDrawCommandsInstanceCountBuildPass", [](){ std::make_unique<IndirectDrawCommandsInstanceCountBuildPass>(IndirectDrawCommandsInstanceCountBuildPass::create());});
    instrumentate_pass_compile_call("IndirectDrawCommandsFirstInstanceBuildPass", [](){ std::make_unique<IndirectDrawCommandsFirstInstanceBuildPass>(IndirectDrawCommandsFirstInstanceBuildPass::create());});
    instrumentate_pass_compile_call("InstanceBufferBuildPass",                    [](){ std::make_unique<InstanceBufferBuildPass                   >(InstanceBufferBuildPass::create());});
    instrumentate_pass_compile_call("SimulationInfluenceStepPass",                [](){ std::make_unique<SimulationInfluenceStepPass               >(SimulationInfluenceStepPass::create());});
    instrumentate_pass_compile_call("BackgroundRenderPass",                       [](){ std::make_unique<BackgroundRenderPass                      >(BackgroundRenderPass::create());});
    instrumentate_pass_compile_call("FishTankRenderPass",                         [](){ std::make_unique<FishTankRenderPass                        >(FishTankRenderPass::create());});
    instrumentate_pass_compile_call("ImGuiRenderPass",                            [](){ std::make_unique<ImGuiRenderPass                           >(ImGuiRenderPass::create());});
    instrumentate_pass_compile_call("BlitMainImageToSwapchainPass",               [](){ std::make_unique<BlitMainImageToSwapchainPass              >(BlitMainImageToSwapchainPass::create());});
}

int main(int argc, char* argv[]) {
    vulkan_quick_setup(pass_compile_test);
    return 0;
}