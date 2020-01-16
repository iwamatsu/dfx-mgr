/*
 * Copyright (c) 2016, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	dma.h
 * @brief	DMA primitives for libmetal.
 */

#ifndef __METAL_DMA__H__
#define __METAL_DMA__H__

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup dma DMA Interfaces
 *  @{ */

#include <stdint.h>
#include <acapd/shm.h>

/**
 * @brief ACAPD DMA transaction direction
 */
typedef enum acapd_dir {
	ACAPD_DMA_DEV_R	= 1U, /**< DMA direction, device read */
	ACAPD_DMA_DEV_W, /**< DMA direction, device write */
	ACAPD_DMA_DEV_WR, /**< DMA direction, device read/write */
} acapd_dir_t;

/**
 * @brief ACAPD channel connection type
 */
typedef enum acapd_chnl_conn {
	ACAPD_CHNL_CC		= 0x1U, /**< Connection is cache coherent */
	ACAPD_CHNL_NONC		= 0x2U, /**< Connection is not cache coherent */
	ACAPD_CHNL_VADDR	= 0x4U, /**< Connection is not cache coherent */
} acapd_chnl_conn_t;

#define ACAPD_MAX_DIMS	4U

/** DMA Channel Operations */
typedef struct acapd_dma_ops {
	const char name[128]; /**< name of the DMA operation */
	void *(*mmap)(void *buff_id, size_t start_off, size_t size, acapd_chnl_t *chnl);
	int (*munmap)(void *buff_id, size_t start_off, size_t size, acapd_chnl_t *chnl);
	int (*config_dma)(acapd_dma_dim *dim, void *buff_id, void *va, size_t size, uint32_t auto_repeat, acapd_fence_t *fence, acapd_chnl_t *chnl);
	int (*start_dma)(acapd_chnl_t *chnl, acapd_fence_t *fence);
	int (*poll_dma)(acapd_chnl_t *chnl, uint32_t wait_for_complete);
	int (*open_chnl)(acapd_chnl_t *chnl);
	int (*close_chnl)(acapd_chnl_t *chnl);
} acapd_dma_ops_t;

/**
 * @brief ACAPD DMA channel data structure
 */
struct acapd_chnl {
	char *name; /**< DMA channel name/or path */
	char *id; /**< DMA channel logical id */
	int chnl_id; /**< hardware channel id of a data mover controller */
	acapd_dir_t dir; /**< DMA channel direction */
	uint32_t data_conn_type; /**< type of data connection with this channel */
	acapd_dma_ops_t *ops;
} acapd_chnl_t;

/**
 * @brief DMA fence data structure
 * TODO
 */
typedef int acapd_fence_t;

/**
 * @brief DMA dimension structure
 */
typedef struct acapd_dim {
	int number_of_dims; /**< Number of dimentions */
	int strides[ACAPD_MAX_DIMS]; /**< stride of each dimention */
} acapd_dim_t;

int acapd_dma_config(acapd_shm_t *shm, acapd_chnl_t *chnl, acapd_dim_t *dim, uint32_t auto_repeat);
int acapd_dma_start(acapd_chnl_t *chnl, acapd_fence_t *fence);
int acapd_dma_stop(acapd_chnl_t *chnl);
int acapd_dma_poll(acapd_chnl_t *chnl, uint32_t wait_for_complete);
int acapd_create_dma_channel(char *name, int iommu_group, acapd_chnl_conn_t conn_type, int chnl_id, acapd_dir_t dir, acapd_chnl_t *chnl);
int acapd_destroy_dma_channel(acapd_chnl_t *chnl);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __METAL_DMA__H__ */