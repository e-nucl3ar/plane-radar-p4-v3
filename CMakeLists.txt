cmake_minimum_required(VERSION 3.16)

# ESP32-P4 Plane Radar — adapted from MatixYo/ESP32-Plane-Radar for the
# Waveshare ESP32-P4-WIFI6-Touch-LCD-4C (4" 720×720 MIPI-DSI round display).
#
# WiFi is provided by the onboard ESP32-C6-MINI via SDIO; the main application
# core is the ESP32-P4.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-p4-plane-radar)
