namespace vk_structs {
	struct DrawIndexedIndirectCommand {
		uint indexCount;
		Atomic<uint> instanceCount;
		uint firstIndex;
		int  vertexOffset;
		uint firstInstance;
	};
	struct DrawIndexedIndirectCommandNonatomic {
		uint indexCount;
		uint instanceCount;
		uint firstIndex;
		int  vertexOffset;
		uint firstInstance;
	};
}