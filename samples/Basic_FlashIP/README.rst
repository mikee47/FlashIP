Basic FlashIP
=========

.. highlight:: bash


Introduction
------------

This is a simplified version of the :sample:`Basic_OTA` sample using the FlashIP approach instead.

There is one application partition and one LittleFS filing system partition.
NB. You can also use SPIFFS and FWFS.


Building
--------

1) Set :envvar:`WIFI_SSID` & :envvar:`WIFI_PWD` environment variables with your wifi details
2) Edit the OTA server details defined in the application ``component.mk`` file
3) ``make flash``
4) Put ``app.bin`` in the root of your webserver for OTA
5) Interact with the sample using a terminal (``make terminal``)


Testing
-------

For testing purposes Sming offers a simple python webserver that can be run on your development machine::

   make otaserver

The server listens on port 9999 and all network interfaces.

The current directory is changed to the firmware directory for the current project, so you could,
for example, open a separate terminal window in the ``Basic_Serial`` directory and do this::

   make SMING_SOC=rp2040
   make otaserver

This will then be ready to serve up the ``app.bin`` image file for the Basic_Serial sample.

This sample should be compiled and flashed as follows, replacing ``192.168.1.30`` with your development machine's IP address
and inserting details for your local WiFi::

   make flash WIFI_SSID=... WIFI_PWD=... FIRMWARE_URL=http://192.168.1.30:9999/app.bin

Once connected to WiFi, you can enter ``ota`` to download and re-program the new firmware.


Configuration
-------------

.. envvar:: FIRMWARE_URL

   The URL for the application image to be downloaded
