/* 
 * gpu_utils.c:
 * Utilities for GPU usage
 */
 
 #include "../shared.h"

// VRAM usage setting
uint8_t use_vram = 0;

// vitaGL memory pool setup
static void *pool_addr = NULL;
static SceUID poolUid;
static unsigned int pool_index = 0;
static unsigned int pool_size = 0;

void* gpu_alloc_map(SceKernelMemBlockType type, SceGxmMemoryAttribFlags gpu_attrib, size_t size, SceUID *uid){
	
	// Aligning memory size
	void *addr;
	switch (type){
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
		size = ALIGN(size, 256 * 1024);
		break;
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
		size = ALIGN(size, 4 * 1024);
		*uid = _newlib_heap_memblock;
		return malloc(size);
		break;
	default: 
		size = ALIGN(size, 4 * 1024);
		break;
	}
	
	// Allocating requested memblock
	*uid = sceKernelAllocMemBlock("gpumem", type, size, NULL);
	if (*uid < 0) return NULL;

	// Getting memblock starting address
	if (sceKernelGetMemBlockBase(*uid, &addr) < 0) return NULL;

	// Mapping memblock into sceGxm
	if (sceGxmMapMemory(addr, size, gpu_attrib) < 0) {
		sceKernelFreeMemBlock(*uid);
		return NULL;
	}

	// Returning memblock starting address
	return addr;
	
}

void gpu_unmap_free(SceUID uid){
	
	// Checking for memblock coherency
	void *addr;
	if (sceKernelGetMemBlockBase(uid, &addr) < 0) return;
	
	// Unmapping memblock from sceGxm
	sceGxmUnmapMemory(addr);

	// Deallocating memblock
	sceKernelFreeMemBlock(uid);
	
}

void* gpu_vertex_usse_alloc_map(size_t size, SceUID *uid, unsigned int *usse_offset){
	
	// Aligning memory size
	void *addr;
	size = ALIGN(size, 4 * 1024);

	// Allocating memblock
	*uid = sceKernelAllocMemBlock("gpu_vertex_usse",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	if (*uid < 0) return NULL;

	// Getting memblock starting address
	if (sceKernelGetMemBlockBase(*uid, &addr) < 0) return NULL;

	// Mapping memblock into sceGxm
	if (sceGxmMapVertexUsseMemory(addr, size, usse_offset) < 0) return NULL;

	// Returning memblock starting address
	return addr;
	
}

void gpu_vertex_usse_unmap_free(SceUID uid){
	
	// Checking memblock coherency
	void *addr;
	if (sceKernelGetMemBlockBase(uid, &addr) < 0) return;

	// Unmapping memblock from sceGxm
	sceGxmUnmapVertexUsseMemory(addr);

	// Deallocating memblock
	sceKernelFreeMemBlock(uid);
	
}

void *gpu_fragment_usse_alloc_map(size_t size, SceUID *uid, unsigned int *usse_offset){

	// Aligning memory size
	void *addr;
	size = ALIGN(size, 4 * 1024);

	// Allocating memblock
	*uid = sceKernelAllocMemBlock("gpu_fragment_usse",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	if (*uid < 0) return NULL;

	// Getting memblock starting address
	if (sceKernelGetMemBlockBase(*uid, &addr) < 0) return NULL;

	// Mapping memblock into sceGxm
	if (sceGxmMapFragmentUsseMemory(addr, size, usse_offset) < 0) return NULL;

	// Returning memblock starting address
	return addr;
	
}

void gpu_fragment_usse_unmap_free(SceUID uid){
	
	// Checking memblock coherency
	void *addr;
	if (sceKernelGetMemBlockBase(uid, &addr) < 0)
		return;

	// Unmapping memblock from sceGxm
	sceGxmUnmapFragmentUsseMemory(addr);

	// Deallocating memblock
	sceKernelFreeMemBlock(uid);
	
}

void* gpu_pool_malloc(unsigned int size){
	
	// Reserving vitaGL mempool space
	if ((pool_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + pool_index);
		pool_index += size;
		return addr;
	}
	
	return NULL;
}

void* gpu_pool_memalign(unsigned int size, unsigned int alignment){
	
	// Aligning requested memory size
	unsigned int new_index = ALIGN(pool_index, alignment);
	
	// Reserving vitaGL mempool space
	if ((new_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + new_index);
		pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int gpu_pool_free_space(){
	
	// Returning vitaGL available mempool space
	return pool_size - pool_index;
	
}

void gpu_pool_reset(){
	
	// Resetting vitaGL available mempool space
	pool_index = 0;
	
}

void gpu_pool_init(uint32_t temp_pool_size){
	
	// Allocating vitaGL mempool
	pool_size = temp_pool_size;
	pool_addr = gpu_alloc_map(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		SCE_GXM_MEMORY_ATTRIB_READ,
		pool_size,
		&poolUid);
	
}

int tex_format_to_bytespp(SceGxmTextureFormat format){
	
	// Calculating bpp for the requested texture format
	switch (format & 0x9f000000U) {
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
			return 1;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
			return 2;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
			return 3;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
		default:
			return 4;
	}
	
}

palette *gpu_alloc_palette(const void* data, uint32_t w, uint32_t bpe){
	
	// Allocating a palette object
	palette *res = (palette*)malloc(sizeof(palette));
	
	// Allocating palette data buffer
	void *texture_palette = gpu_alloc_map(
		(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW),
		SCE_GXM_MEMORY_ATTRIB_READ, 256 * sizeof(uint32_t), &res->palette_UID);
	if (texture_palette == NULL){ // If alloc fails, use the non-preferred memblock type
		texture_palette = gpu_alloc_map(
			(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW),
			SCE_GXM_MEMORY_ATTRIB_READ, 256 * sizeof(uint32_t), &res->palette_UID);
	}
		
	// Initializing palette
	if (data == NULL) memset(texture_palette, 0, 256 * sizeof(uint32_t));
	else if (bpe == 4) memcpy(texture_palette, data, w * sizeof(uint32_t));
	res->data = texture_palette;
	
	// Returning palette
	return res;
	
}

void gpu_free_texture(texture *tex){
	
	// Deallocating texture
	if (tex->data_UID != 0){
		if (tex->data_UID == _newlib_heap_memblock) free(tex->data);
		else gpu_unmap_free(tex->data_UID);
	}
	
	// Invalidating texture object
	tex->data_UID = 0;
	tex->valid = 0;
	
}

void gpu_alloc_texture(uint32_t w, uint32_t h, SceGxmTextureFormat format, const void *data, texture *tex, uint8_t src_bpp, uint32_t (*read_cb)(void*),  void (*write_cb)(void*, uint32_t)){
	
	// If there's already a texture in passed texture object we first dealloc it
	if (tex->valid) gpu_free_texture(tex);
	
	// Getting texture format bpp
	uint8_t bpp = tex_format_to_bytespp(format);
	
	// Allocating texture data buffer
	const int tex_size = w * h * bpp;
	void *texture_data = gpu_alloc_map(
		(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW),
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		tex_size, &tex->data_UID);
	if (texture_data == NULL){ // If alloc fails, use the non-preferred memblock type
		texture_data = gpu_alloc_map(
			(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW),
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			tex_size, &tex->data_UID);
	}
	
	if (texture_data != NULL){
		
		// Initializing texture data buffer
		if (data != NULL){
			int i, j;
			uint8_t *src = (uint8_t*)data;
			uint8_t *dst;
			for (i=0;i<h;i++){
				dst = ((uint8_t*)texture_data) + (ALIGN(w, 8) * bpp) * i;
				for (j=0;j<w;j++){
					uint32_t clr = read_cb(src);
					write_cb(dst, clr);
					src += src_bpp;
					dst += bpp;
				}
			}
		}else memset(texture_data, 0, tex_size);
		
		// Initializing texture and validating it
		tex->data = texture_data;
		sceGxmTextureInitLinear(&tex->gxm_tex, texture_data, format, w, h, 0);
		if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8) tex->palette_UID = 1;
		else tex->palette_UID = 0;
		tex->valid = 1;
		
	}
}

void gpu_alloc_mipmaps(int level, texture *tex){
	
	// Getting current mipmap count in passed texture
	uint32_t count = sceGxmTextureGetMipmapCount(&tex->gxm_tex);
	
	// Getting textures info and calculating bpp
	uint32_t w, h;
	uint32_t orig_w = w = sceGxmTextureGetWidth(&tex->gxm_tex);
	uint32_t orig_h = h = sceGxmTextureGetHeight(&tex->gxm_tex);
	SceGxmTextureFormat format = sceGxmTextureGetFormat(&tex->gxm_tex);
	uint32_t bpp = tex_format_to_bytespp(format);
	
	// Checking if we need at least one more new mipmap level
	if ((level > count) || (level < 0)){ // Note: level < 0 means we will use max possible mipmaps level
		
		// Calculating new texture data buffer size
		uint32_t size = 0;
		int j;
		if (level > 0){
			for (j=0;j<level;j++){
				size += max(w, 8) * h * bpp;
				w /= 2;
				h /= 2;
			}
		}else{
			level = 0;
			while ((w > 1) && (h > 1)){
				size += max(w, 8) * h * bpp;
				w /= 2;
				h /= 2;
				level++;
			}
		}
		
		// Calculating needed sceGxmTransfer format for the downscale process
		SceGxmTransferFormat fmt;
		switch (tex->type){
			case GL_RGBA:
				fmt = SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR;
				break;
			case GL_RGB:
				fmt = SCE_GXM_TRANSFER_FORMAT_U8U8U8_BGR;
			default:
				break;
		}
			
		// Moving texture data to heap and deallocating texture memblock
		void *temp = (void*)malloc(orig_w * orig_h * bpp);
		memcpy(temp, sceGxmTextureGetData(&tex->gxm_tex), orig_w * orig_h * bpp);
		gpu_free_texture(tex);
			
		// Allocating the new texture data buffer
		void *texture_data = gpu_alloc_map(
			(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW),
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			size, &tex->data_UID);
		if (texture_data == NULL){ // If alloc fails, use the non-preferred memblock type
			texture_data = gpu_alloc_map(
				(use_vram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW),
				SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
				size, &tex->data_UID);
		}
		tex->valid = 1;
		
		// Moving back old texture data from heap to texture memblock
		memcpy(texture_data, temp, orig_w * orig_h * bpp);
		free(temp);
		
		// Performing a chain downscale process to generate requested mipmaps
		uint8_t *curPtr = (uint8_t*)texture_data;
		uint32_t curWidth = orig_w;
		uint32_t curHeight = orig_h;
		for (j = 0; j < level - 1; j++){
			uint32_t curSrcStride = max(curWidth, 8);
			uint32_t curDstStride = max((curWidth>>1), 8);
			uint8_t *dstPtr = curPtr + (curSrcStride * curHeight * bpp);
			sceGxmTransferDownscale(
				fmt, curPtr, 0, 0,
				curWidth, curHeight,
				curSrcStride * bpp,
				fmt, dstPtr, 0, 0,
				curDstStride * bpp,
				NULL, SCE_GXM_TRANSFER_FRAGMENT_SYNC, NULL);
			curPtr = dstPtr;
			curWidth /= 2;
			curHeight /= 2;
		}
		
		// Initializing texture in sceGxm
		tex->data = texture_data;
		sceGxmTextureInitLinear(&tex->gxm_tex, texture_data, format, orig_w, orig_h, level);
		
	}
}

void gpu_free_palette(palette *pal){
	
	// Unmapping and deallocating palette memblock and object
	if (pal == NULL) return;
	if (pal->palette_UID == _newlib_heap_memblock) free(pal->data);
	else gpu_unmap_free(pal->palette_UID);
	free(pal);
	
}