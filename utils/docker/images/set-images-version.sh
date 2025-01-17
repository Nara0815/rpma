#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation
#

#
# set-images-version.sh -- set value of the 'IMG_VER' variable
#                          containing the current version of Docker images
#
# This file has to be located in the "utils/docker/images" subdirectory,
# because every change of a value of IMG_VER has to trigger the rebuild
# of all Docker images.
#
# A version of Docker images should be different only for different
# and standalone branches. It makes no sense to change it for the same branch.
#

export IMG_VER=master
