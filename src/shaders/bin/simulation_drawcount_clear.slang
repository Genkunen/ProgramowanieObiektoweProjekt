struct PushConstants {
    Atomic<uint>* draw_count;
};

[vk::push_constant] PushConstants pushConstants;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    pushConstants.draw_count->store(0, MemoryOrder.Relaxed);
}