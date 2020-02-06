/*
 * Copyright (c) 2019, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <acapd/accel.h>
#include <acapd/assert.h>
#include <acapd/print.h>
#include <errno.h>
#include <dirent.h>
#include <ftw.h>
#include <fcntl.h>
#include "generic-device.h"
#include "json-config.h"
#include <libfpga.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "accel.h"

#define DTBO_ROOT_DIR "/sys/kernel/config/device-tree/overlays"

static int remove_directory(const char *path)
{
	DIR *d = opendir(path);
	int r = -1;

	if (d) {
		struct dirent *p;
		size_t path_len;

		path_len = strlen(path);
		r = 0;
		while (!r && (p=readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;
			struct stat statbuf;

			/* Skip the names "." and ".." as we don't want
			 * to recurse on them. */
			if (!strcmp(p->d_name, ".") ||
			    !strcmp(p->d_name, "..")) {
				continue;
			}
			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);
			if (buf == NULL) {
				acapd_perror("Failed to allocate memory.\n");
				return -1;
			}

			sprintf(buf, "%s/%s", path, p->d_name);
			if (!stat(buf, &statbuf)) {
				if (S_ISDIR(statbuf.st_mode)) {
					r2 = remove_directory(buf);
				} else {
					r2 = unlink(buf);
				}
			}
			r = r2;
			free(buf);
		}
		closedir(d);
		if (r == 0) {
			r = rmdir(path);
		}
	}
	return r;
}

int sys_accel_config(acapd_accel_t *accel)
{
	acapd_accel_pkg_hd_t *pkg;
	char template[] = "/tmp/accel.XXXXXX";
	char *tmp_dirname;
	char cmd[512];
	int ret;
	char *pkg_name;
	char *env_config_path, config_path[128];

	env_config_path = getenv("ACCEL_CONFIG_PATH");
	memset(config_path, 0, sizeof(config_path));
	if(env_config_path == NULL) {
		size_t len;

		/* Use timestamp to name the tmparary directory */
		acapd_debug("%s: Creating tmp dir for package.\n", __func__);
		tmp_dirname = mkdtemp(template);
		if (tmp_dirname == NULL) {
			acapd_perror("Failed to create tmp dir for package:%s.\n",
			     strerror(errno));
			return ACAPD_ACCEL_FAILURE;
		}
		sprintf(accel->sys_info.tmp_dir, "%s/", tmp_dirname);
		pkg = accel->pkg;
		pkg_name = (char *)pkg;
		/* TODO: Assuming the package is a tar.gz format */
		sprintf(cmd, "tar -C %s -xzf %s", tmp_dirname, pkg_name);
		ret = system(cmd);
		if (ret != 0) {
			acapd_perror("Failed to extract package %s.\n", pkg_name);
			return ACAPD_ACCEL_FAILURE;
		}
		len = sizeof(config_path) - strlen("accel.json") - 1;
		if (len > strlen(accel->sys_info.tmp_dir)) {
			len = strlen(accel->sys_info.tmp_dir);
		} else {
			acapd_perror("%s: accel config path is too long.\n");
			return ACAPD_ACCEL_FAILURE;
		}
		strncpy(config_path, accel->sys_info.tmp_dir, len);
		strcat(config_path, "accel.json");
	} else {
		size_t len;

		len = sizeof(config_path) - 1;
		if (len > strlen(env_config_path)) {
			len = strlen(env_config_path);
		} else {
			acapd_perror("%s: accel config env path is too long.\n");
			return ACAPD_ACCEL_FAILURE;
		}
		strncpy(config_path, env_config_path, len);
	}
	parseAccelJson(accel, config_path);
	if (sys_needs_load_accel(accel) == 0) {
		for (int i = 0; i < accel->num_ip_devs; i++) {
			char *tmppath;
			char tmpstr[32];
			acapd_device_t *dev;

			dev = &(accel->ip_dev[i]);
			sprintf(tmpstr, "ACCEL_IP%d_PATH", i);
			tmppath = getenv(tmpstr);
			if (tmppath != NULL) {
				size_t len;
				len = sizeof(dev->path) - 1;
			    memset(dev->path, 0, len + 1);
				if (len > strlen(tmppath)) {
					len = strlen(tmppath);
				}
				strncpy(dev->path, tmppath, len);
			}
			if (tmppath == NULL) {
				break;
			}
		}
		return ACAPD_ACCEL_SUCCESS;
	} else {
		return ACAPD_ACCEL_SUCCESS;
	}
}

int sys_needs_load_accel(acapd_accel_t *accel) {
	char *pkg_dir;
	DIR *d;

	pkg_dir = accel->sys_info.tmp_dir;
	if (pkg_dir == NULL) {
		acapd_debug("%s: No need to load accel.\n", __func__);
		return 0;
	}
	d = opendir(pkg_dir);
	if (d) {
		struct dirent *p;

		while ((p = readdir(d)) != NULL) {
			/* Skip the names "." and ".." as we don't want
			 * to recurse on them. */
			if (strstr(p->d_name, ".dtbo") != NULL) {
				return 1;
			}
		}
	}
	return 0;
}

int sys_load_accel(acapd_accel_t *accel, unsigned int async)
{
	int ret;
	int fpga_cfg_id;

	(void)async;
	acapd_assert(accel != NULL);
	acapd_debug("%s: init package dir: %s.\n", __func__, accel->sys_info.tmp_dir);
	ret = fpga_cfg_init(accel->sys_info.tmp_dir, 0, 0);
	if (ret < 0) {
		acapd_perror("Failed to initialize fpga config, %d.\n", ret);
		return ACAPD_ACCEL_FAILURE;
	}
	fpga_cfg_id = ret;
	accel->sys_info.fpga_cfg_id = fpga_cfg_id;
	ret = fpga_cfg_load(fpga_cfg_id);
	if (ret != 0) {
		acapd_perror("Failed to load fpga config: %d\n",
		     fpga_cfg_id);
		return ACAPD_ACCEL_FAILURE;
	} else {
		return ACAPD_ACCEL_SUCCESS;
	}
}

int sys_load_accel_post(acapd_accel_t *accel)
{
	acapd_assert(accel != NULL);
	char cmd[512];
	char tmpstr[512];

	sprintf(cmd,"docker run --ulimit memlock=67108864:67108864 --rm -v /usr/lib:/x_usrlib -v /usr/bin/:/xbin/ -v /lib/:/xlib -v %s:%s ",accel->sys_info.tmp_dir,accel->sys_info.tmp_dir);
	for (int i = 0; i < accel->num_ip_devs; i++) {
		int ret;
		char tmpstr[512];

		ret = acapd_device_open(&accel->ip_dev[i]);
		if (ret != 0) {
			acapd_perror("%s: failed to open accel ip %s.\n",
				     __func__, accel->ip_dev[i].dev_name);
			return -EINVAL;
		}
		sprintf(tmpstr,"--device=%s:%s ",accel->ip_dev[i].path,accel->ip_dev[i].path);
		strcat(cmd,tmpstr);
		strcat(cmd,"--device=/dev/vfio:/dev/vfio ");
	}
	for (int i = 0; i < accel->num_chnls; i++) {
		int ret;

		ret = acapd_generic_device_bind(accel->chnls[i].dev,
						accel->chnls[i].dev->driver);
		if (ret != 0) {
			acapd_perror("%s: failed to open chnl dev %s.\n",
				     __func__, accel->chnls[i].dev->dev_name);
			return -EINVAL;
		}
	}

	sprintf(tmpstr,"docker load < %s/container.tar",accel->sys_info.tmp_dir);
	acapd_debug("%s:Loading docker container\n",__func__);
	system(tmpstr);

	sprintf(tmpstr," -e \"ACCEL_CONFIG_PATH=%s/accel.json\"",accel->sys_info.tmp_dir);
	strcat(cmd, tmpstr);
	strcat(cmd, " -it container");
	acapd_debug("%s: docker run cmd: %s\n",__func__,cmd);
	system(cmd);
	return 0;
}

int sys_close_accel(acapd_accel_t *accel)
{
	/* Close devices and free memory */
	acapd_assert(accel != NULL);
	for (int i = 0; i < accel->num_chnls; i++) {
		acapd_debug("%s: closing channel %d.\n", __func__, i);
		acapd_dma_close(&accel->chnls[i]);
	}
	if (accel->num_chnls > 0) {
		free(accel->chnls[0].dev->dev_name);
		free(accel->chnls[0].dev);
		free(accel->chnls);
		accel->chnls = NULL;
		accel->num_chnls = 0;
	}
	for (int i = 0; i < accel->num_ip_devs; i++) {
		acapd_debug("%s: closing accel ip %d %s.\n", __func__, i, accel->ip_dev[i].dev_name);
		acapd_device_close(&accel->ip_dev[i]);
	}
	if (accel->num_ip_devs > 0) {
		free(accel->ip_dev->dev_name);
		free(accel->ip_dev);
		accel->ip_dev = NULL;
		accel->num_ip_devs = 0;
	}
	return 0;
}

int sys_remove_accel(acapd_accel_t *accel, unsigned int async)
{
	int ret, fpga_cfg_id;

	/* TODO: for now, only synchronous mode is supported */
	(void)async;

	fpga_cfg_id = accel->sys_info.fpga_cfg_id;
	if (accel->sys_info.tmp_dir != NULL) {
		ret = remove_directory(accel->sys_info.tmp_dir);
		if (ret != 0) {
			acapd_perror("Failed to remove %s, %s\n",
				     accel->sys_info.tmp_dir, strerror(errno));
		}
	}
	if (fpga_cfg_id <= 0) {
		acapd_perror("Invalid fpga cfg id: %d.\n", fpga_cfg_id);
		return ACAPD_ACCEL_FAILURE;
	};
	ret = fpga_cfg_remove(fpga_cfg_id);
	if (ret != 0) {
		acapd_perror("Failed to remove accel: %d.\n", ret);
		return ACAPD_ACCEL_FAILURE;
	}
	ret = fpga_cfg_destroy(fpga_cfg_id);
	if (ret != 0) {
		acapd_perror("Failed to destroy accel: %d.\n", ret);
		return ACAPD_ACCEL_FAILURE;
	}
	return ACAPD_ACCEL_SUCCESS;
}
