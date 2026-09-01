.. _shell_module:

Shell 模块
===========
`zephyr 示例链接 <https://docs.zephyrproject.org/3.7.0/samples/subsys/shell/shell_module/README.html>`_

概述
--------

这是一个简单的应用程序，演示如何使用 Shell API 编写和注册命令：

- **注册静态命令**: ``version`` 是一个打印内核版本的静态命令。
- **注册动态命令**: 参考 ``dynamic`` 命令了解如何实现动态命令。
- **注册字典命令**: ``dictionary`` 实现了字典命令的子集。
- **设置旁路回调**: ``bypass`` 实现了旁路回调功能。

路径
---------------

.. code-block::

    zephyr/samples/subsys/shell/shell_module

命令行
-----------

以dust-hpm6750为例:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S shell_module zephyr/samples/subsys/shell/shell_module
