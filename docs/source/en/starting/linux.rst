======================================
Linux Environment Configuration Guide
======================================

**Linux** (using Ubuntu with bash) requires manual installation of the hpmicro riscv-openocd tool. Add openocd to your system path.


Installing Prerequisites
--------------------------

#. Update APT package lists

    .. code-block:: console

        sudo apt update
        sudo apt upgrade

#. Install dependencies

    .. code-block:: console

        sudo apt install --no-install-recommends git cmake ninja-build gperf \
            ccache dfu-util device-tree-compiler wget \
            python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
            make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1

#. Verify tool versions (minimum requirements below). Upgrade if versions are insufficient

.. list-table::
   :header-rows: 1

   * - Tool
     - Min. Version

   * - `CMake <https://cmake.org/>`_
     - 3.20.5

   * - `Python <https://www.python.org/>`_
     - 3.8

   * - `Devicetree compiler <https://www.devicetree.org/>`_
     - 1.4.6


Setting Up Workspace
----------------------

#. Install west and add `~/.local/bin`` to ~/.bashrc

    .. code-block:: console
        
        pip3 install --user -U west
        echo 'export PATH=~/.local/bin:"$PATH"' >> ~/.bashrc
        source ~/.bashrc

#. Create workspace directory and fetch source code

    .. code-block:: console

        mkdir ${workspace}
        cd ${workspace}
        west init -m https://github.com/hpmicro/zephyr_sdk_glue.git --mr main

#. Fetch repositories (use the first command to switch to a domestic mirror in China, default github):

    .. code-block:: console

        west config manifest.file west_gitee.yml # For domestic mirror
        west update

#. Configure CMake variables

    .. code-block:: console

        west zephyr-export

#. Install Zephyr Python dependencies

    .. code-block:: console

        pip3 install --user -r zephyr/scripts/requirements.txt

#. Apply HPM_SDK patches

    .. code-block:: console

        west supply

Configuring Toolchain
----------------------

Multiple toolchains are supported. Choose one based on your needs.

.. note::

   If you already have a RISC-V toolchain, you can skip the installation and configure environment variables directly. For detailed toolchain configuration and switching methods, refer to the :doc:`toolchain` section.

Zephyr SDK
^^^^^^^^^^^^^^^^^^^^^^^^^^

Official Zephyr SDK with full functionality and best compatibility.

Download from: `ZEPHYR-SDK <https://github.com/zephyrproject-rtos/sdk-ng/releases/>`_

#. Command-line installation

    .. code-block:: console

        cd ${workspace}
        wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
        wget -O - https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/sha256.sum | shasum --check --ignore-missing
        tar xvf zephyr-sdk-0.16.5_linux-x86_64.tar.xz

#. Configure toolchain environment variables

    .. code-block:: console

        cd zephyr-sdk-0.16.5
        ./setup.sh

ZCC Toolchain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ZCC is an LLVM/Clang-based toolchain with smaller download size and fast compilation.

#. Download and extract ZCC toolchain

    Obtain the ZCC toolchain package from the appropriate channel and extract to a directory, e.g., ``~/sdk_env/toolchains/zcc-4.1.5/``

#. Configure environment variables

    .. code-block:: console

        export ZEPHYR_TOOLCHAIN_VARIANT=zcc
        export ZCC_TOOLCHAIN_PATH=~/sdk_env/toolchains/zcc-4.1.5/
        export TOOLCHAIN_ROOT=${workspace}/sdk_glue/

    It is recommended to add these environment variables to ``~/.bashrc`` for persistence.

GNU Cross-Compile Toolchain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you already have a RISC-V GCC toolchain, you can use it directly. This is not officially supported; resolve any issues on your own.

#. Configure environment variables

    .. code-block:: console

        export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
        export CROSS_COMPILE=/path/to/riscv32-unknown-elf-gcc/bin/riscv32-unknown-elf-

    Replace ``/path/to/riscv32-unknown-elf-gcc`` with your actual toolchain installation path

Building Zephyr Button Sample
------------------------------
    Build the button sample for dust-hpm6750. The build directory can be placed anywhere in the workspace (recommended under workspace/zephyr/)

#. Build and compile

    .. code-block:: console

        cd ${workspace}/zephyr
        west build -p always -b dust-hpm6750 samples/basic/button

`-p` option: `always` for clean build, `auto` for incremental build.
`-S` option: Apply hardware-specific configurations.
    
    .. code-block:: console

        west build -p always -b dust-hpm6750 -S blinky_pwm samples/basic/blinky_pwm    

#. Flashing or Debugging

    .. code-block:: console
        
        west flash / west debug

Additional Commands
---------------------
    Useful commands:

#. Kconfig configuration

    .. code-block:: console

        west build -t menuconfig

#. List available boards

    .. code-block:: console

        west boards | grep hpm

#. Start GDB server (connect board first)

    .. code-block:: console

        west debugserver

#. Generate HTML documentation

    .. code-block:: console

        cd sdk_glue/docs
        make html

