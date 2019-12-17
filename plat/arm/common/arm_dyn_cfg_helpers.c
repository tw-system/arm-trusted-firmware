/*
 * Copyright (c) 2018-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <libfdt.h>

#include <common/fdt_wrappers.h>
#include <plat/arm/common/arm_dyn_cfg_helpers.h>
#include <plat/arm/common/plat_arm.h>

#define DTB_PROP_MBEDTLS_HEAP_ADDR "mbedtls_heap_addr"
#define DTB_PROP_MBEDTLS_HEAP_SIZE "mbedtls_heap_size"

/*******************************************************************************
 * Helper to read the `disable_auth` property in config DTB. This function
 * expects the following properties to be present in the config DTB.
 *	name : disable_auth		size : 1 cell
 *
 * Arguments:
 *	void *dtb		 - pointer to the TB_FW_CONFIG in memory
 *	int node		 - The node offset to appropriate node in the
 *				   DTB.
 *	uint64_t *disable_auth	 - The value of `disable_auth` property on
 *				   successful read. Must be 0 or 1.
 *
 * Returns 0 on success and -1 on error.
 ******************************************************************************/
int arm_dyn_get_disable_auth(void *dtb, int node, uint32_t *disable_auth)
{
	int err;

	assert(dtb != NULL);
	assert(disable_auth != NULL);

	/* Check if the pointer to DT is correct */
	assert(fdt_check_header(dtb) == 0);

	/* Assert the node offset point to "arm,tb_fw" compatible property */
	assert(node == fdt_node_offset_by_compatible(dtb, -1, "arm,tb_fw"));

	/* Locate the disable_auth cell and read the value */
	err = fdtw_read_cells(dtb, node, "disable_auth", 1, disable_auth);
	if (err < 0) {
		WARN("Read cell failed for `disable_auth`\n");
		return -1;
	}

	/* Check if the value is boolean */
	if ((*disable_auth != 0U) && (*disable_auth != 1U)) {
		WARN("Invalid value for `disable_auth` cell %d\n", *disable_auth);
		return -1;
	}

	VERBOSE("Dyn cfg: `disable_auth` cell found with value = %d\n",
					*disable_auth);
	return 0;
}

/*******************************************************************************
 * Validate the tb_fw_config is a valid DTB file and returns the node offset
 * to "arm,tb_fw" property.
 * Arguments:
 *	void *dtb - pointer to the TB_FW_CONFIG in memory
 *	int *node - Returns the node offset to "arm,tb_fw" property if found.
 *
 * Returns 0 on success and -1 on error.
 ******************************************************************************/
int arm_dyn_tb_fw_cfg_init(void *dtb, int *node)
{
	assert(dtb != NULL);
	assert(node != NULL);

	/* Check if the pointer to DT is correct */
	if (fdt_check_header(dtb) != 0) {
		WARN("Invalid DTB file passed as TB_FW_CONFIG\n");
		return -1;
	}

	/* Assert the node offset point to "arm,tb_fw" compatible property */
	*node = fdt_node_offset_by_compatible(dtb, -1, "arm,tb_fw");
	if (*node < 0) {
		WARN("The compatible property `arm,tb_fw` not found in the config\n");
		return -1;
	}

	VERBOSE("Dyn cfg: Found \"arm,tb_fw\" in the config\n");
	return 0;
}

/*
 * Reads and returns the Mbed TLS shared heap information from the DTB.
 * This function is supposed to be called *only* when a DTB is present.
 * This function is supposed to be called only by BL2.
 *
 * Returns:
 *	0 = success
 *	-1 = error. In this case the values of heap_addr, heap_size should be
 *	    considered as garbage by the caller.
 */
int arm_get_dtb_mbedtls_heap_info(void *dtb, void **heap_addr,
	size_t *heap_size)
{
	int err, dtb_root;

	/* Verify the DTB is valid and get the root node */
	err = arm_dyn_tb_fw_cfg_init(dtb, &dtb_root);
	if (err < 0) {
		ERROR("Invalid TB_FW_CONFIG. Cannot retrieve Mbed TLS heap information from DTB\n");
		return -1;
	}

	/* Retrieve the Mbed TLS heap details from the DTB */
	err = fdtw_read_cells(dtb, dtb_root,
		DTB_PROP_MBEDTLS_HEAP_ADDR, 2, heap_addr);
	if (err < 0) {
		ERROR("Error while reading %s from DTB\n",
			DTB_PROP_MBEDTLS_HEAP_ADDR);
		return -1;
	}
	err = fdtw_read_cells(dtb, dtb_root,
		DTB_PROP_MBEDTLS_HEAP_SIZE, 1, heap_size);
	if (err < 0) {
		ERROR("Error while reading %s from DTB\n",
			DTB_PROP_MBEDTLS_HEAP_SIZE);
		return -1;
	}
	return 0;
}


/*
 * This function writes the Mbed TLS heap address and size in the DTB. When it
 * is called, it is guaranteed that a DTB is available. However it is not
 * guaranteed that the shared Mbed TLS heap implementation is used. Thus we
 * return error code from here and it's the responsibility of the caller to
 * determine the action upon error.
 *
 * This function is supposed to be called only by BL1.
 *
 * Returns:
 *	0 = success
 *	1 = error
 */
int arm_set_dtb_mbedtls_heap_info(void *dtb, void *heap_addr, size_t heap_size)
{
	int err, dtb_root;

	/*
	 * Verify that the DTB is valid, before attempting to write to it,
	 * and get the DTB root node.
	 */
	err = arm_dyn_tb_fw_cfg_init(dtb, &dtb_root);
	if (err < 0) {
		ERROR("Invalid TB_FW_CONFIG loaded. Unable to get root node\n");
		return -1;
	}

	/*
	 * Write the heap address and size in the DTB.
	 *
	 * NOTE: The variables heap_addr and heap_size are corrupted
	 * by the "fdtw_write_inplace_cells" function. After the
	 * function calls they must NOT be reused.
	 */
	err = fdtw_write_inplace_cells(dtb, dtb_root,
		DTB_PROP_MBEDTLS_HEAP_ADDR, 2, &heap_addr);
	if (err < 0) {
		ERROR("Unable to write DTB property %s\n",
			DTB_PROP_MBEDTLS_HEAP_ADDR);
		return -1;
	}

	err = fdtw_write_inplace_cells(dtb, dtb_root,
		DTB_PROP_MBEDTLS_HEAP_SIZE, 1, &heap_size);
	if (err < 0) {
		ERROR("Unable to write DTB property %s\n",
			DTB_PROP_MBEDTLS_HEAP_SIZE);
		return -1;
	}

	return 0;
}
