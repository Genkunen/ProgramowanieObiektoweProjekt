namespace vk_structs {
	struct DrawIndexedIndirectCommand {
		uint indexCount;
		uint instanceCount;
		uint firstIndex;
		int  vertexOffset;
		uint firstInstance;
	};
}