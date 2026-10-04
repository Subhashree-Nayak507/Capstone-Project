#!/bin/sh
rmmod envmon_drv && dmesg | tail -n 2
