#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <drm_fourcc.h>

#include "tables.h"

static void print_nvidia_modifier(uint64_t mod) {
	if (!(mod & 0x10)) {
		printf("NVIDIA(unknown)");
		return;
	}

	uint64_t h = mod & 0xF;
	uint64_t k = (mod >> 12) & 0xFF;
	uint64_t g = (mod >> 20) & 0x3;
	uint64_t s = (mod >> 22) & 0x1;
	uint64_t c = (mod >> 23) & 0x7;

	printf("NVIDIA_BLOCK_LINEAR_2D(h=%"PRIu64", k=%"PRIu64", g=%"PRIu64", "
		"s=%"PRIu64", c=%"PRIu64")", h, k, g, s, c);
}

static const char *amd_tile_version_str(uint64_t tile_version) {
	switch (tile_version) {
	case AMD_FMT_MOD_TILE_VER_GFX9:
		return "GFX9";
	case AMD_FMT_MOD_TILE_VER_GFX10:
		return "GFX10";
	case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
		return "GFX10_RBPLUS";
	case AMD_FMT_MOD_TILE_VER_GFX11:
		return "GFX11";
	}
	return "unknown";
}

static const char *amd_tile_str(uint64_t tile, uint64_t tile_version) {
	switch (tile_version) {
	case AMD_FMT_MOD_TILE_VER_GFX11:
		/* fallthrough */
	case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
		/* fallthrough */
	case AMD_FMT_MOD_TILE_VER_GFX10:
		/* fallthrough */
	case AMD_FMT_MOD_TILE_VER_GFX9:
		switch (tile) {
		case AMD_FMT_MOD_TILE_GFX9_64K_S:
			return "GFX9_64K_S";
		case AMD_FMT_MOD_TILE_GFX9_64K_D:
			return "GFX9_64K_D";
		case AMD_FMT_MOD_TILE_GFX9_64K_S_X:
			return "GFX9_64K_S_X";
		case AMD_FMT_MOD_TILE_GFX9_64K_D_X:
			return "GFX9_64K_D_X";
		case AMD_FMT_MOD_TILE_GFX9_64K_R_X:
			return "GFX9_64K_R_X";
		case AMD_FMT_MOD_TILE_GFX11_256K_R_X:
			return "GFX11_256K_R_X";
		}
	}
	return "unknown";
}

static const char *amd_dcc_block_size_str(uint64_t size) {
	switch (size) {
	case AMD_FMT_MOD_DCC_BLOCK_64B:
		return "64B";
	case AMD_FMT_MOD_DCC_BLOCK_128B:
		return "128B";
	case AMD_FMT_MOD_DCC_BLOCK_256B:
		return "256B";
	}
	return "unknown";
}

static bool amd_gfx9_tile_is_x_t(uint64_t tile) {
	switch (tile) {
	case AMD_FMT_MOD_TILE_GFX9_64K_S_X:
	case AMD_FMT_MOD_TILE_GFX9_64K_D_X:
	case AMD_FMT_MOD_TILE_GFX9_64K_R_X:
		return true;
	}
	return false;
}

static void print_amd_modifier(uint64_t mod) {
	uint64_t tile_version = AMD_FMT_MOD_GET(TILE_VERSION, mod);
	uint64_t tile = AMD_FMT_MOD_GET(TILE, mod);
	uint64_t dcc = AMD_FMT_MOD_GET(DCC, mod);
	uint64_t dcc_retile = AMD_FMT_MOD_GET(DCC_RETILE, mod);

	printf("AMD(TILE_VERSION = %s, TILE = %s",
		amd_tile_version_str(tile_version), amd_tile_str(tile, tile_version));

	if (dcc) {
		printf(", DCC");
		if (dcc_retile) {
			printf(", DCC_RETILE");
		}
		if (!dcc_retile && AMD_FMT_MOD_GET(DCC_PIPE_ALIGN, mod)) {
			printf(", DCC_PIPE_ALIGN");
		}
		if (AMD_FMT_MOD_GET(DCC_INDEPENDENT_64B, mod)) {
			printf(", DCC_INDEPENDENT_64B");
		}
		if (AMD_FMT_MOD_GET(DCC_INDEPENDENT_128B, mod)) {
			printf(", DCC_INDEPENDENT_128B");
		}
		uint64_t dcc_max_compressed_block =
			AMD_FMT_MOD_GET(DCC_MAX_COMPRESSED_BLOCK, mod);
		printf(", DCC_MAX_COMPRESSED_BLOCK = %s",
			amd_dcc_block_size_str(dcc_max_compressed_block));
		if (AMD_FMT_MOD_GET(DCC_CONSTANT_ENCODE, mod)) {
			printf(", DCC_CONSTANT_ENCODE");
		}
	}

	if (tile_version >= AMD_FMT_MOD_TILE_VER_GFX9 && amd_gfx9_tile_is_x_t(tile)) {
		printf(", PIPE_XOR_BITS = %"PRIu64, AMD_FMT_MOD_GET(PIPE_XOR_BITS, mod));
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9) {
			printf(", BANK_XOR_BITS = %"PRIu64, AMD_FMT_MOD_GET(BANK_XOR_BITS, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS) {
			printf(", PACKERS = %"PRIu64, AMD_FMT_MOD_GET(PACKERS, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9 && dcc) {
			printf(", RB = %"PRIu64, AMD_FMT_MOD_GET(RB, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9 && dcc &&
				(dcc_retile || AMD_FMT_MOD_GET(DCC_PIPE_ALIGN, mod))) {
			printf(", PIPE = %"PRIu64, AMD_FMT_MOD_GET(PIPE, mod));
		}
	}

	printf(")");
}

static const char *arm_afbc_block_size_str(uint64_t block_size) {
	switch (block_size) {
	case AFBC_FORMAT_MOD_BLOCK_SIZE_16x16:
		return "16x16";
	case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8:
		return "32x8";
	case AFBC_FORMAT_MOD_BLOCK_SIZE_64x4:
		return "64x4";
	case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4:
		return "32x8_64x4";
	}
	return "unknown";
}

static const char *arm_afrc_cu_size_str(uint64_t cu_size) {
	switch (cu_size) {
	case AFRC_FORMAT_MOD_CU_SIZE_16:
		return "16";
	case AFRC_FORMAT_MOD_CU_SIZE_24:
		return "24";
	case AFRC_FORMAT_MOD_CU_SIZE_32:
		return "32";
	}
	return "unknown";
}

static void print_arm_modifier(uint64_t mod) {
	uint64_t type = (mod >> 52) & 0xF;
	uint64_t value = mod & 0x000FFFFFFFFFFFFFULL;

	switch (type) {
	case DRM_FORMAT_MOD_ARM_TYPE_AFBC:;
		uint64_t block_size = value & AFBC_FORMAT_MOD_BLOCK_SIZE_MASK;
		printf("ARM_AFBC(BLOCK_SIZE = %s", arm_afbc_block_size_str(block_size));
		if (value & AFBC_FORMAT_MOD_YTR) {
			printf(", YTR");
		}
		if (value & AFBC_FORMAT_MOD_SPLIT) {
			printf(", SPLIT");
		}
		if (value & AFBC_FORMAT_MOD_SPARSE) {
			printf(", SPARSE");
		}
		if (value & AFBC_FORMAT_MOD_CBR) {
			printf(", CBR");
		}
		if (value & AFBC_FORMAT_MOD_TILED) {
			printf(", TILED");
		}
		if (value & AFBC_FORMAT_MOD_SC) {
			printf(", SC");
		}
		if (value & AFBC_FORMAT_MOD_DB) {
			printf(", DB");
		}
		if (value & AFBC_FORMAT_MOD_BCH) {
			printf(", BCH");
		}
		if (value & AFBC_FORMAT_MOD_USM) {
			printf(", USM");
		}
		printf(")");
		break;
	case DRM_FORMAT_MOD_ARM_TYPE_MISC:
		switch (mod) {
		case DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED:
			printf("ARM_16X16_BLOCK_U_INTERLEAVED");
			break;
		default:
			printf("ARM_MISC(unknown)");
		}
		break;
	case DRM_FORMAT_MOD_ARM_TYPE_AFRC:;
		uint64_t cu_size_p0 = value & AFRC_FORMAT_MOD_CU_SIZE_MASK;
		uint64_t cu_size_p12 = (value >> 4) & AFRC_FORMAT_MOD_CU_SIZE_MASK;
		printf("ARM_AFRC(");
		printf("CU_SIZE_P0 = %s", arm_afrc_cu_size_str(cu_size_p0));
		printf(", CU_SIZE_P12 = %s", arm_afrc_cu_size_str(cu_size_p12));
		if (value & AFRC_FORMAT_MOD_LAYOUT_SCAN)
			printf(", SCAN");
		else
			printf(", ROT");
		printf(")");
		break;
	default:
		printf("ARM(unknown)");
	}
}

static const char *amlogic_layout_str(uint64_t layout) {
	switch (layout) {
	case AMLOGIC_FBC_LAYOUT_BASIC:
		return "BASIC";
	case AMLOGIC_FBC_LAYOUT_SCATTER:
		return "SCATTER";
	}
	return "unknown";
}

static void print_amlogic_modifier(uint64_t mod) {
	uint64_t layout = mod & 0xFF;
	uint64_t options = (mod >> 8) & 0xFF;

	printf("AMLOGIC_FBC(layout = %s, options = %s)",
		amlogic_layout_str(layout),
		(options & AMLOGIC_FBC_OPTION_MEM_SAVING) ? "MEM_SAVING" : "0");
}

static const char *vivante_color_tiling_str(uint64_t tiling) {
	switch (tiling) {
	case 0:
		return "LINEAR";
	case DRM_FORMAT_MOD_VIVANTE_TILED:
		return "TILED";
	case DRM_FORMAT_MOD_VIVANTE_SUPER_TILED:
		return "SUPER_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED:
		return "SPLIT_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED:
		return "SPLIT_SUPER_TILED";
	}
	return "Unknown";
}

static const char *vivante_tile_status_str(uint64_t ts) {
	switch (ts) {
	case VIVANTE_MOD_TS_64_4:
		return "64_4";
	case VIVANTE_MOD_TS_64_2:
		return "64_2";
	case VIVANTE_MOD_TS_128_4:
		return "128_4";
	case VIVANTE_MOD_TS_256_4:
		return "256_4";
	}
	return "Unknown";
}

static const char *vivante_compression_str(uint64_t comp) {
	switch (comp) {
	case VIVANTE_MOD_COMP_DEC400:
		return "DEC400";
	}
	return "Unknown";
}

static void print_vivante_modifier(uint64_t mod) {
	uint64_t ts = mod & VIVANTE_MOD_TS_MASK;
	uint64_t comp = mod & VIVANTE_MOD_COMP_MASK;
	uint64_t tiling = mod & ~VIVANTE_MOD_EXT_MASK;

	printf("VIVANTE(tiling = %s", vivante_color_tiling_str(tiling));
	if (ts != 0) {
		printf(", ts = %s", vivante_tile_status_str(ts));
	}
	if (comp != 0) {
		printf(", comp = %s", vivante_compression_str(comp));
	}
	printf(")");
}

static uint8_t mod_vendor(uint64_t mod) {
	return (uint8_t)(mod >> 56);
}

void print_modifier(uint64_t mod) {
	switch (mod_vendor(mod)) {
	case DRM_FORMAT_MOD_VENDOR_NVIDIA:
		print_nvidia_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_AMD:
		print_amd_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_ARM:
		print_arm_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_AMLOGIC:
		print_amlogic_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_VIVANTE:
		print_vivante_modifier(mod);
		break;
	default:
		printf("%s", basic_modifier_str(mod));
	}
	printf(" (0x%"PRIx64")", mod);
}
