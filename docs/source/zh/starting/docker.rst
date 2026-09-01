====================================
Docker 环境配置
====================================

使用 **Docker** 是配置 HPMicro Zephyr 开发环境最快捷的方式。无需手动安装依赖，容器中已预先配置好所有环境。

前置条件
---------

开始之前，请确保：

- 已安装 Docker
- （可选）已安装 Docker Compose，便于容器管理
- USB 访问权限（用于烧录，推荐使用 Linux）

获取 Docker 镜像
------------------

您可以从 **DockerHub** 拉取镜像，或者下载镜像包后使用 ``docker load`` 加载。

从 DockerHub 拉取
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker pull swhpmicro/zephyr-hpmicro:latest

从镜像包加载（推荐国内用户使用）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

国内用户推荐下载镜像包后本地加载，速度更快更稳定。

#. 下载镜像包

   从以下地址下载镜像包：

   - `docker <https://resource.hpmicro.com/zephyr-hpmicro-latest.tar.gz>`_
   - 其他分发渠道获取 ``zephyr-hpmicro-latest.tar.gz`` 文件

#. 加载镜像

   .. code-block:: console

       docker load -i zephyr-hpmicro-latest.tar.gz

#. 验证镜像已加载

   .. code-block:: console

       docker images | grep zephyr-hpmicro

   您应该能看到类似以下输出：

   .. code-block:: text

       swhpmicro/zephyr-hpmicro    latest    xxxxxxxxxx    xx days ago    xxGB

配置工具链
-------------------

由于 Zephyr SDK 体积较大，未包含在 Docker 镜像中。

Zephyr SDK
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Zephyr 官方 SDK，功能完整，兼容性最佳。

.. code-block:: console

    mkdir ~/zephyr-hpmicro && cd ~/zephyr-hpmicro
    wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
    tar xvf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
    cd zephyr-sdk-0.16.5 && ./setup.sh -t all -h -c && cd ..


方法一：使用 Docker Run
-------------------------

此方法适合快速测试或一次性使用。

基本用法
~~~~~~~~~

.. code-block:: console

    docker run -it --rm \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

启用 USB 设备访问（用于烧录）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

烧录固件到开发板需要 USB 设备访问权限：

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

挂载自定义项目目录
~~~~~~~~~~~~~~~~~~~~

如需挂载您自己的项目目录：

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        -v ~/my_projects:/home/zephyr/zephyr_space/my_projects \
        swhpmicro/zephyr-hpmicro:latest \
        bash

使用本地加载的镜像运行
~~~~~~~~~~~~~~~~~~~~~~~~

如果您通过 ``docker load`` 加载了镜像包，运行命令与上述相同，使用相同的镜像名称：

.. code-block:: console

    docker run -it --rm \
        --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v ~/zephyr-hpmicro/zephyr-sdk-0.16.5:/home/zephyr/zephyr_space/zephyr-sdk-0.16.5 \
        swhpmicro/zephyr-hpmicro:latest \
        bash

.. note::

    无论是从 DockerHub 拉取还是从镜像包加载，镜像名称都是 ``swhpmicro/zephyr-hpmicro:latest``，使用方式完全相同。


方法二：使用 Docker Compose
-------------------------------------

Docker Compose 提供了更便捷的容器管理方式，配置可持久化保存。

步骤一：提取 docker-compose.yaml
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

方法一：使用 docker run

.. code-block:: console

    cd ~/zephyr-hpmicro
    docker run --rm swhpmicro/zephyr-hpmicro:latest cat /home/zephyr/zephyr_space/sdk_glue/docker-compose.yaml > docker-compose.yaml

.. note::

    注意：不要使用 ``-t`` 或 ``-it`` 选项，否则输出可能包含终端转义序列。

方法二：使用 docker cp

如果方法一生成的文件包含终端符号，可以使用以下方法：

.. code-block:: console

    cd ~/zephyr-hpmicro
    docker create --name temp_container zephyr-hpmicro:latest
    docker cp temp_container:/home/zephyr/zephyr_space/sdk_glue/docker-compose.yaml ./docker-compose.yaml
    docker rm temp_container

.. note::

    无论是从 DockerHub 拉取还是从镜像包加载的镜像，提取命令都是相同的。

步骤二：启动容器
~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose up -d

步骤三：进入容器
~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose exec zephyr-dev bash

步骤四：停止容器
~~~~~~~~~~~~~~~~~~

.. code-block:: console

    docker compose down


目录结构
---------

配置完成后，您的工作目录结构如下：

.. code-block:: text

    ~/zephyr-hpmicro/
    ├── docker-compose.yaml      # Docker Compose 配置文件
    ├── zephyr-sdk-0.16.5/       # Zephyr SDK（已下载）
    │   ├── setup.sh
    │   ├── sdk_version
    │   └── ...
    └── my_projects/             # 可选：您的自定义项目

容器内部结构：

.. code-block:: text

    /home/zephyr/zephyr_space/
    ├── bootloader/              # （内置于镜像）
    ├── modules/                 # （内置于镜像）
    ├── sdk_env/                 # HPMicro SDK（内置）
    ├── sdk_glue/                # （内置于镜像）
    ├── zephyr/                  # Zephyr 源码（内置）
    │   └── samples/             # 示例项目
    ├── zephyr-sdk-0.16.5/       # （从主机挂载）
    └── my_projects/             # （可选，从主机挂载）


编译与烧录
-----------

进入容器后，您可以编译和烧录示例程序：

#. 编译示例

    .. code-block:: console

        cd /home/zephyr/zephyr_space/zephyr
        west build -p always -b dust-hpm6750 -S blinky samples/basic/blinky

#. 烧录到目标板（请先连接开发板）

    .. code-block:: console

        west flash

#. 调试

    .. code-block:: console

        west debug

可用开发板
~~~~~~~~~~~~

.. code-block:: console

    west boards | grep hpm

常用开发板包括：

- ``dust-hpm6750``
- ``hpm6800evk``
- ``hpm6200evk``
- ``hpm6e00evk``


常见问题
---------

找不到 Zephyr SDK
~~~~~~~~~~~~~~~~~~~

确保 ``zephyr-sdk-0.16.5`` 文件夹已正确挂载，并且已在其中运行过 ``setup.sh``：

.. code-block:: console

    cd ~/zephyr-hpmicro/zephyr-sdk-0.16.5
    ./setup.sh -t all -h -c

无法烧录 - 找不到设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. 检查调试器是否已连接：

    .. code-block:: console

        lsusb | grep -i xxx

#. 确保使用了 ``--privileged`` 参数（docker run）或设置了 ``privileged: true`` (docker-compose)

#. 确认 ``/dev/bus/usb`` 已挂载

串口设备权限被拒绝
~~~~~~~~~~~~~~~~~~~~

方法一：添加到 dialout 组（适用于标准串口设备）

在主机上将您的用户添加到 dialout 组：

.. code-block:: console

    sudo usermod -aG dialout $USER

然后注销并重新登录。

方法二：配置 udev 规则

如果方法一仍然无法访问串口，特别是使用 USB 调试器（如 CMSIS-DAP、OpenOCD 等）时，需要配置 udev 规则。

某些 USB 调试器设备不是标准的串口设备，仅添加到 ``dialout`` 组可能不够。通过配置 udev 规则，可以为这些设备设置正确的权限。

#. 创建 udev 规则文件（host）：

   .. code-block:: console

       sudo nano /etc/udev/rules.d/99-openocd.rules

#. 添加以下规则内容（根据您的调试器类型选择）：

   .. code-block:: text

       # 允许所有用户访问 USB 调试器
       SUBSYSTEM=="usb", ATTR{idVendor}=="0d28", MODE="0666"
       KERNEL=="ttyACM*", MODE="0666"
       KERNEL=="ttyUSB*", MODE="0666"

#. 重新加载 udev 规则：

   .. code-block:: console

       sudo udevadm control --reload-rules
       sudo udevadm trigger

#. 重新插拔 USB 设备，或重启系统。

.. note::

   如果您的调试器不在上述列表中，可以通过以下命令查看设备的 ``idVendor`` 和 ``idProduct``：

   .. code-block:: console

       lsusb

   然后根据实际的 ``idVendor`` 和 ``idProduct`` 创建对应的 udev 规则。

使用不同版本的 SDK
~~~~~~~~~~~~~~~~~~~~

在启动前设置环境变量：

.. code-block:: console

    export ZEPHYR_SDK_PATH=./zephyr-sdk-0.17.0
    docker compose up -d


