=========================================
Docker Environment Configuration Guide
=========================================

Using **Docker** is the fastest way to set up the Zephyr development environment for HPMicro. No need to install dependencies manually - everything is pre-configured in the container.

Prerequisites
--------------

Before starting, ensure you have:

- Docker installed on your system
- (Optional) Docker Compose for easier container management
- USB access for flashing (Linux recommended)

Getting the Docker Image
--------------------------

You can pull the Docker image from **DockerHub**, or download the image package and load it using ``docker load``.

From DockerHub
~~~~~~~~~~~~~~~

.. code-block:: console

    docker pull swhpmicro/zephyr-hpmicro:latest

Loading from Image Package (Recommended for users in China)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Users in China are recommended to download the image package and load it locally for faster and more stable speeds.

#. Download the image package

   Download the image package from the following address:

   - `docker <https://resource.hpmicro.com/zephyr-hpmicro-latest.tar.gz>`_
   - obtain the ``zephyr-hpmicro-latest.tar.gz`` file from other distribution channels

#. Load the image

   .. code-block:: console

       docker load -i zephyr-hpmicro-latest.tar.gz

#. Verify the image is loaded

   .. code-block:: console

       docker images | grep zephyr-hpmicro

   You should see output similar to:

   .. code-block:: text

       swhpmicro/zephyr-hpmicro    latest    xxxxxxxxxx    xx days ago    xxGB

Configuring Toolchain
-----------------------

The Zephyr SDK is large and not included in the Docker image.

Zephyr SDK
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Official Zephyr SDK with full functionality and best compatibility.

.. code-block:: console

    mkdir ~/zephyr-hpmicro && cd ~/zephyr-hpmicro
    wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
    tar xvf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
    cd zephyr-sdk-0.16.5 && ./setup.sh -t all -h -c && cd ..


Method 1: Using Docker Run
----------------------------

This method is suitable for quick testing or one-time use.

Basic Usage
~~~~~~~~~~~~

.. code-block:: console

    docker run -it --rm \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

With USB Device Access (for Flashing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To flash firmware to your board, you need USB device access:

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

With Custom Project Directory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To mount your own project directory:

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        -v ~/my_projects:/home/zephyr/zephyr_space/my_projects \
        swhpmicro/zephyr-hpmicro:latest \
        bash

Running with Locally Loaded Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you loaded the image package using ``docker load``, the run command is the same as above, using the same image name:

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

.. note::

    Whether you pull from DockerHub or load from an image package, the image name is ``swhpmicro/zephyr-hpmicro:latest``, and the usage is exactly the same.


Method 2: Using Docker Compose
--------------------------------

Docker Compose provides a more convenient way to manage the container with persistent configuration.

Step 1: Extract docker-compose.yaml
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Method 1: Using docker run

.. code-block:: console

    cd ~/zephyr-hpmicro
    docker run --rm swhpmicro/zephyr-hpmicro:latest cat /home/zephyr/zephyr_space/sdk_glue/docker-compose.yaml > docker-compose.yaml

.. note::

    Note: Do not use ``-t`` or ``-it`` options, as they may cause terminal escape sequences in the output.

Method 2: Using docker cp

If Method 1 produces a file with terminal symbols, use the following method:

.. code-block:: console

    cd ~/zephyr-hpmicro
    docker create --name temp_container zephyr-hpmicro:latest
    docker cp temp_container:/home/zephyr/zephyr_space/sdk_glue/docker-compose.yaml ./docker-compose.yaml
    docker rm temp_container

.. note::

    Whether you pull from DockerHub or load from an image package, the extraction commands are the same.

Step 2: Start the Container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose up -d

Step 3: Enter the Container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose exec zephyr-dev bash

Step 4: Stop the Container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose down


Directory Structure
---------------------

After setup, your working directory should look like this:

.. code-block:: text

    ~/zephyr-hpmicro/
    ├── docker-compose.yaml      # Docker Compose configuration
    ├── zephyr-sdk-0.16.5/       # Zephyr SDK (downloaded)
    │   ├── setup.sh
    │   ├── sdk_version
    │   └── ...
    └── my_projects/             # Optional: Your custom projects

Inside the container:

.. code-block:: text

    /home/zephyr/zephyr_space/
    ├── bootloader/              # (embedded in image)
    ├── modules/                 # (embedded in image)
    ├── sdk_env/                 # HPMicro SDK (embedded)
    ├── sdk_glue/                # (embedded in image)
    ├── zephyr/                  # Zephyr source (embedded)
    │   └── samples/             # Sample projects to build
    ├── zephyr-sdk-0.16.5/       # (mounted from host)
    └── my_projects/             # (optional, mounted from host)


Building and Flashing
-----------------------

Once inside the container, you can build and flash samples:

#. Build a sample

    .. code-block:: console

        cd /home/zephyr/zephyr_space/zephyr
        west build -p always -b dust-hpm6750 -S blinky samples/basic/blinky

#. Flash to target (connect your board first)

    .. code-block:: console

        west flash

#. Debug

    .. code-block:: console

        west debug

Available Boards
~~~~~~~~~~~~~~~~~~

.. code-block:: console

    west boards | grep hpm

Common boards include:

- ``dust-hpm6750``
- ``hpm6800evk``
- ``hpm6200evk``
- ``hpm6e00evk``


Troubleshooting
-----------------

Zephyr SDK Not Found
~~~~~~~~~~~~~~~~~~~~~

Make sure the ``zephyr-sdk-0.16.5`` folder is correctly mounted and you ran ``setup.sh`` inside it:

.. code-block:: console

    cd ~/zephyr-hpmicro/zephyr-sdk-0.16.5
    ./setup.sh -t all -h -c

Cannot Flash - Device Not Found
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. Check if debugger is connected:

    .. code-block:: console

        lsusb | grep -i xxx

#. Ensure ``--privileged`` flag is used (docker run) or ``privileged: true`` is set (docker-compose)

#. Verify ``/dev/bus/usb`` is mounted

Permission Denied on Serial Device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Method 1: Add to dialout group (for standard serial devices)

Add your user to the dialout group on the host:

.. code-block:: console

    sudo usermod -aG dialout $USER

Then logout and login again.

Method 2: Configure udev rules

If Method 1 still doesn't allow access to the serial port, especially when using USB debuggers (such as CMSIS-DAP, OpenOCD, etc.), you need to configure udev rules.

Some USB debugger devices are not standard serial devices, and simply adding to the ``dialout`` group may not be sufficient. By configuring udev rules, you can set the correct permissions for these devices.

#. Create a udev rules file (on host):

   .. code-block:: console

       sudo nano /etc/udev/rules.d/99-openocd.rules

#. Add the following rule content (select according to your debugger type):

   .. code-block:: text

       # Allow all users to access USB debuggers
       SUBSYSTEM=="usb", ATTR{idVendor}=="0d28", MODE="0666"
       KERNEL=="ttyACM*", MODE="0666"
       KERNEL=="ttyUSB*", MODE="0666"

#. Reload udev rules:

   .. code-block:: console

       sudo udevadm control --reload-rules
       sudo udevadm trigger

#. Reconnect the USB device, or restart the system.

.. note::

    If your debugger is not in the list above, you can check the device's ``idVendor`` and ``idProduct`` using the following command:

    .. code-block:: console

        lsusb

    Then create the corresponding udev rules based on the actual ``idVendor`` and ``idProduct``.

Using a Different SDK Version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the environment variable before starting:

.. code-block:: console

    export ZEPHYR_SDK_PATH=./zephyr-sdk-0.17.0
    docker compose up -d


