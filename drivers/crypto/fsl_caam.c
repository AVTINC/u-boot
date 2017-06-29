/*
 * Copyright (c) 2012-2016, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/crm_regs.h>
#include "fsl_caam_internal.h"
#include <fsl_caam.h>

#define DMA_ALIGN(x) (((uint32_t) x) & (~(ARCH_DMA_MINALIGN-1)))

/*---------- Global variables ----------*/
/* Input job ring - single entry input ring */
uint32_t g_input_ring[JOB_RING_ENTRIES] = {0};

/* Output job ring - single entry output ring (consists of two words) */
uint32_t g_output_ring[2*JOB_RING_ENTRIES] = {0, 0};

uint32_t decap_dsc[] =
{
	DECAP_BLOB_DESC1,
	DECAP_BLOB_DESC2,
	DECAP_BLOB_DESC3,
	DECAP_BLOB_DESC4,
	DECAP_BLOB_DESC5,
	DECAP_BLOB_DESC6,
	DECAP_BLOB_DESC7,
	DECAP_BLOB_DESC8,
	DECAP_BLOB_DESC9
};

uint32_t encap_dsc[] =
{
	ENCAP_BLOB_DESC1,
	ENCAP_BLOB_DESC2,
	ENCAP_BLOB_DESC3,
	ENCAP_BLOB_DESC4,
	ENCAP_BLOB_DESC5,
	ENCAP_BLOB_DESC6,
	ENCAP_BLOB_DESC7,
	ENCAP_BLOB_DESC8,
	ENCAP_BLOB_DESC9
};

uint32_t rng_inst_dsc[] =
{
	RNG_INST_DESC1,
	RNG_INST_DESC2,
	RNG_INST_DESC3,
	RNG_INST_DESC4,
	RNG_INST_DESC5,
	RNG_INST_DESC6,
	RNG_INST_DESC7,
	RNG_INST_DESC8,
	RNG_INST_DESC9
};

/*!
 * Secure memory run command.
 *
 * @param   sec_mem_cmd  Secure memory command register
 * @return  cmd_status  Secure memory command status register
 */
uint32_t secmem_set_cmd_1(uint32_t sec_mem_cmd)
{
	uint32_t temp_reg;

	__raw_writel(sec_mem_cmd, CAAM_SMCJR0);
	do
		temp_reg = __raw_readl(CAAM_SMCSJR0);
	while (temp_reg & CMD_COMPLETE);

	return temp_reg;
}

/*!
 * Use CAAM to decapsulate a blob to secure memory.
 * Such blob of secret key cannot be read once decrypted,
 * but can still be used for enc/dec operation of user's data.
 *
 * @param   blob_addr  Location address of the blob.
 *
 * @return  SUCCESS or ERROR_XXX
 */
uint32_t caam_decap_blob(void *plain_text, void *blob_addr, void *key_modifier, uint32_t data_size)
{
	uint32_t ret = SUCCESS;
	uint32_t blob_size = BLOB_SIZE(data_size);

	decap_dsc[0] = (uint32_t)0xB0800008;
	decap_dsc[1] = (uint32_t)0x14400010;
	decap_dsc[2] = (uint32_t)key_modifier;
	decap_dsc[3] = (uint32_t)0xF0000000 | (0x0000ffff & (blob_size));
	decap_dsc[4] = (uint32_t)blob_addr;
	decap_dsc[5] = (uint32_t)0xF8000000 | (0x0000ffff & (data_size));
	decap_dsc[6] = (uint32_t)plain_text;
	decap_dsc[7] = (uint32_t)0x860D0000;

	/* Run descriptor with result written to blob buffer */
	/* Add job to input ring */
	g_input_ring[0] = (uint32_t)decap_dsc;

	flush_dcache_range(DMA_ALIGN(blob_addr), DMA_ALIGN(blob_addr + 2 * blob_size));
	flush_dcache_range(DMA_ALIGN(plain_text), DMA_ALIGN(plain_text + 2 * data_size));
	flush_dcache_range(DMA_ALIGN(decap_dsc), DMA_ALIGN(decap_dsc + 128));
	flush_dcache_range(DMA_ALIGN(g_input_ring), DMA_ALIGN(g_input_ring + 128));
	flush_dcache_range(DMA_ALIGN(key_modifier), DMA_ALIGN(key_modifier  + 256));

	/* Increment jobs added */
	__raw_writel(1, CAAM_IRJAR0);

	/* Wait for job ring to complete the job: 1 completed job expected */
	while (__raw_readl(CAAM_ORSFR0) != 1)
		;

	invalidate_dcache_range(DMA_ALIGN(g_output_ring), DMA_ALIGN(g_output_ring + 128));
	/* check that descriptor address is the one expected in the output ring */

	if (g_output_ring[0] == (uint32_t)decap_dsc) {
		/* check if any error is reported in the output ring */
		if ((g_output_ring[1] & JOB_RING_STS) != 0) {
			debug("Error: blob decap job completed with errors 0x%X\n",
						g_output_ring[1]);
			ret = -1;
		}
	} else {
		debug("Error: blob decap job output ring descriptor address does" \
	                " not match\n");
		ret = -1;
	}
	flush_dcache_range(DMA_ALIGN(plain_text), DMA_ALIGN(plain_text + 2 * data_size));

	/* Remove job from Job Ring Output Queue */
	__raw_writel(1, CAAM_ORJRR0);

	return ret;
}

/*!
 * Use CAAM to generate a blob.
 *
 * @param   plain_data_addr  Location address of the plain data.
 * @param   blob_addr  Location address of the blob.
 *
 * @return  SUCCESS or ERROR_XXX
 */
uint32_t caam_gen_blob(void *plain_data_addr, void *blob_addr, void *key_modifier, uint32_t data_size)
{
	uint32_t ret = SUCCESS;
	uint32_t blob_size = BLOB_SIZE(data_size);
	uint8_t *blob = (uint8_t *)blob_addr;

	/* initialize the blob array */
	memset(blob, 0, blob_size);

	encap_dsc[0] = (uint32_t)0xB0800008;
	encap_dsc[1] = (uint32_t)0x14400010;
	encap_dsc[2] = (uint32_t)key_modifier;
	encap_dsc[3] = (uint32_t)0xF0000000 | (0x0000ffff & (data_size));
	encap_dsc[4] = (uint32_t)plain_data_addr;
	encap_dsc[5] = (uint32_t)0xF8000000 | (0x0000ffff & (blob_size));
	encap_dsc[6] = (uint32_t)blob;	
	encap_dsc[7] = (uint32_t)0x870D0000;

	/* Run descriptor with result written to blob buffer */
	/* Add job to input ring */
	g_input_ring[0] = (uint32_t)encap_dsc;

	flush_dcache_range(DMA_ALIGN(plain_data_addr), DMA_ALIGN(plain_data_addr + data_size));
	flush_dcache_range(DMA_ALIGN(encap_dsc), DMA_ALIGN(encap_dsc + 128));
	flush_dcache_range(DMA_ALIGN(blob), DMA_ALIGN(g_input_ring + 2 * blob_size));
	flush_dcache_range(DMA_ALIGN(key_modifier), DMA_ALIGN(key_modifier + 256));
	
	/* Increment jobs added */
	__raw_writel(1, CAAM_IRJAR0);

	/* Wait for job ring to complete the job: 1 completed job expected */
	while (__raw_readl(CAAM_ORSFR0) != 1)
		;

	// flush cache
	invalidate_dcache_range(DMA_ALIGN(g_output_ring), DMA_ALIGN(g_output_ring + 128));

	/* check that descriptor address is the one expected in the output ring */
	if (g_output_ring[0] == (uint32_t)encap_dsc) {
		/* check if any error is reported in the output ring */
		if ((g_output_ring[1] & JOB_RING_STS) != 0) {
			debug("Error: blob encap job completed with errors 0x%X\n",
			      g_output_ring[1]);
			ret = -1;
 		}
	} else {
		debug("Error: blob encap job output ring descriptor " \
			"address does not match\n");
		ret = -1;
	}
	/* Remove job from Job Ring Output Queue */
	__raw_writel(1, CAAM_ORJRR0);

	return ret;
}

/*!
 * Initialize the CAAM.
 *
 */
void caam_open(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	uint32_t temp_reg;

	/* switch on the clock */
	temp_reg = __raw_readl(&mxc_ccm->CCGR0);
	temp_reg |= MXC_CCM_CCGR0_CAAM_SECURE_MEM_MASK | 
		MXC_CCM_CCGR0_CAAM_WRAPPER_ACLK_MASK | 
		MXC_CCM_CCGR0_CAAM_WRAPPER_IPG_MASK;
	__raw_writel(temp_reg, &mxc_ccm->CCGR0);

	/* MID for CAAM - already done by HAB in ROM during preconfigure,
	 * That is JROWN for JR0/1 = 1 (TZ, Secure World, ARM)
	 * JRNSMID and JRSMID for JR0/1 = 2 (TZ, Secure World, CAAM)
	 *
	 * However, still need to initialize Job Rings as these are torn
	 * down by HAB for each command
	 */

	/* Initialize job ring addresses */
	__raw_writel((uint32_t)g_input_ring, CAAM_IRBAR0);   // input ring address
	__raw_writel((uint32_t)g_output_ring, CAAM_ORBAR0);  // output ring address

	/* Initialize job ring sizes to 1 */
	__raw_writel(JOB_RING_ENTRIES, CAAM_IRSR0);
	__raw_writel(JOB_RING_ENTRIES, CAAM_ORSR0);

	/* HAB disables interrupts for JR0 so do the same here */
	temp_reg = __raw_readl(CAAM_JRCFGR0_LS) | JRCFG_LS_IMSK;
	__raw_writel(temp_reg, CAAM_JRCFGR0_LS);    

	/********* Initialize and instantiate the RNG *******************/
	/* if RNG already instantiated then skip it */
	if ((__raw_readl(CAAM_RDSTA) & RDSTA_IF0) != RDSTA_IF0) {
		/* Enter TRNG Program mode */
		__raw_writel(RTMCTL_PGM, CAAM_RTMCTL);

		/* Set OSC_DIV field to TRNG */
		temp_reg = __raw_readl(CAAM_RTMCTL) | (RNG_TRIM_OSC_DIV << 2);
		__raw_writel(temp_reg, CAAM_RTMCTL);

		/* Set delay */
		__raw_writel(((RNG_TRIM_ENT_DLY << 16) | 0x09C4), CAAM_RTSDCTL);
		__raw_writel((RNG_TRIM_ENT_DLY >> 1), CAAM_RTFRQMIN);
		__raw_writel((RNG_TRIM_ENT_DLY << 4), CAAM_RTFRQMAX);

		/* Resume TRNG Run mode */
		temp_reg = __raw_readl(CAAM_RTMCTL) ^ RTMCTL_PGM;
		__raw_writel(temp_reg, CAAM_RTMCTL);

		/* Clear the ERR bit in RTMCTL if set. The TRNG error can occur
		 * if the RNG clock is not within 1/2x to 8x the system clock.
		 * This error is possible if ROM code does not initialize the
		 * system PLLs immediately after PoR.
		 */
		temp_reg = __raw_readl(CAAM_RTMCTL) | RTMCTL_ERR;
		__raw_writel(temp_reg, CAAM_RTMCTL);

		/* Run descriptor to instantiate the RNG */
		/* Add job to input ring */
		g_input_ring[0] = (uint32_t)rng_inst_dsc;

		flush_dcache_range(DMA_ALIGN(g_input_ring), DMA_ALIGN(g_input_ring + 128));
		/* Increment jobs added */
		__raw_writel(1, CAAM_IRJAR0);

		/* Wait for job ring to complete the job */
		while (__raw_readl(CAAM_ORSFR0) != 1)
			;
		invalidate_dcache_range(DMA_ALIGN(g_output_ring), DMA_ALIGN(g_output_ring + 128));
		
		/* check that descriptor address is the one expected */
		if (g_output_ring[0] == (uint32_t)rng_inst_dsc) {
			/* check if any error is reported in the output ring */
			if ((g_output_ring[1] & JOB_RING_STS) != 0) {
				printf("Error: RNG instantiation errors " \
					"g_output_ring[1]: 0x%X\n",
					g_output_ring[1]);
				printf("RTMCTL 0x%X\n",
					 __raw_readl(CAAM_RTMCTL));
				printf("RTSTATUS 0x%X\n",
					 __raw_readl(CAAM_RTSTATUS));
				printf("RTSTA 0x%X\n",
					 __raw_readl(CAAM_RDSTA));
			}
		} else
			printf("Error: RNG job output ring descriptor address" \
				"does not match: 0x%X != 0x%X\n",
				g_output_ring[0], rng_inst_dsc[0]);

		/* ensure that the RNG was correctly instantiated */
		temp_reg = __raw_readl(CAAM_RDSTA);
		if (temp_reg != (RDSTA_IF0 | RDSTA_SKVN))
			printf("Error: RNG instantiation failed 0x%X\n", temp_reg);
		
		/* Remove job from Job Ring Output Queue */
		__raw_writel(1, CAAM_ORJRR0);
	}
}
